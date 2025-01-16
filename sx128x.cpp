// Copyright Sandeep Mistry, Mark Qvist and Jacob Eva.
// Licensed under the MIT license.

#include "Boards.h"

#if MODEM == SX1280
#include "sx128x.h"

#define MCU_1284P 0x91
#define MCU_2560  0x92
#define MCU_ESP32 0x81
#define MCU_NRF52 0x71
#if defined(__AVR_ATmega1284P__)
  #define PLATFORM PLATFORM_AVR
  #define MCU_VARIANT MCU_1284P
#elif defined(__AVR_ATmega2560__)
  #define PLATFORM PLATFORM_AVR
  #define MCU_VARIANT MCU_2560
#elif defined(ESP32)
  #define PLATFORM PLATFORM_ESP32
  #define MCU_VARIANT MCU_ESP32
#elif defined(NRF52840_XXAA)
  #define PLATFORM PLATFORM_NRF52
  #define MCU_VARIANT MCU_NRF52
#endif

#ifndef MCU_VARIANT
  #error No MCU variant defined, cannot compile
#endif

#if MCU_VARIANT == MCU_ESP32
  #if MCU_VARIANT == MCU_ESP32 and !defined(CONFIG_IDF_TARGET_ESP32S3)
    #include "hal/wdt_hal.h"
  #endif
  #define ISR_VECT IRAM_ATTR
#else
  #define ISR_VECT
#endif

// SX128x registers
#define OP_RF_FREQ_8X               0x86
#define OP_SLEEP_8X                 0x84
#define OP_STANDBY_8X               0x80
#define OP_TX_8X                    0x83
#define OP_RX_8X                    0x82
#define OP_SET_IRQ_FLAGS_8X         0x8D
#define OP_CLEAR_IRQ_STATUS_8X      0x97
#define OP_GET_IRQ_STATUS_8X        0x15
#define OP_RX_BUFFER_STATUS_8X      0x17
#define OP_PACKET_STATUS_8X         0x1D
#define OP_CURRENT_RSSI_8X          0x1F
#define OP_MODULATION_PARAMS_8X     0x8B
#define OP_PACKET_PARAMS_8X         0x8C
#define OP_STATUS_8X                0xC0
#define OP_TX_PARAMS_8X             0x8E
#define OP_PACKET_TYPE_8X           0x8A
#define OP_BUFFER_BASE_ADDR_8X      0x8F
#define OP_READ_REGISTER_8X         0x19
#define OP_WRITE_REGISTER_8X        0x18
#define IRQ_TX_DONE_MASK_8X         0x01
#define IRQ_RX_DONE_MASK_8X         0x02
#define IRQ_HEADER_DET_MASK_8X      0x10
#define IRQ_HEADER_ERROR_MASK_8X    0x20
#define IRQ_PAYLOAD_CRC_ERROR_MASK_8X 0x40

#define MODE_LONG_RANGE_MODE_8X     0x01

#define OP_FIFO_WRITE_8X            0x1A
#define OP_FIFO_READ_8X             0x1B
#define IRQ_PREAMBLE_DET_MASK_8X    0x80

#define REG_PACKET_SIZE             0x901
#define REG_FIRM_VER_MSB            0x154
#define REG_FIRM_VER_LSB            0x153

#define XTAL_FREQ_8X (double)52000000
#define FREQ_DIV_8X (double)pow(2.0, 18.0)
#define FREQ_STEP_8X (double)(XTAL_FREQ_8X / FREQ_DIV_8X)

#if defined(NRF52840_XXAA)
  extern SPIClass spiModem;
  #define SPI spiModem
#endif

extern SPIClass SPI;

#define MAX_PKT_LENGTH           255

sx128x::sx128x() :
  _spiSettings(8E6, MSBFIRST, SPI_MODE0),
  _ss(LORA_DEFAULT_SS_PIN), _reset(LORA_DEFAULT_RESET_PIN), _dio0(LORA_DEFAULT_DIO0_PIN), _rxen(pin_rxen), _busy(LORA_DEFAULT_BUSY_PIN), _txen(pin_txen),
  _frequency(0), _txp(0), _sf(0x05), _bw(0x34), _cr(0x01), _packetIndex(0), _implicitHeaderMode(0), _payloadLength(255), _crcMode(0), _fifo_tx_addr_ptr(0),
  _fifo_rx_addr_ptr(0), _rxPacketLength(0), _preinit_done(false), _tcxo(false) { setTimeout(0); }

bool ISR_VECT sx128x::getPacketValidity() {
    uint8_t buf[2];
    buf[0] = 0x00;
    buf[1] = 0x00;
    executeOpcodeRead(OP_GET_IRQ_STATUS_8X, buf, 2);
    executeOpcode(OP_CLEAR_IRQ_STATUS_8X, buf, 2);
    if ((buf[1] & IRQ_PAYLOAD_CRC_ERROR_MASK_8X) == 0) { return true; }
    else { return false; }
}

void ISR_VECT sx128x::onDio0Rise() {
    BaseType_t int_status = taskENTER_CRITICAL_FROM_ISR();
    // On the SX1280, there is a bug which can cause the busy line
    // to remain high if a high amount of packets are received when
    // in continuous RX mode. This is documented as Errata 16.1 in
    // the SX1280 datasheet v3.2 (page 149)
    // Therefore, the modem is set into receive mode each time a packet is received.
    if (sx128x_modem.getPacketValidity()) { sx128x_modem.receive(); sx128x_modem.handleDio0Rise(); }
    else                                  { sx128x_modem.receive(); }

    taskEXIT_CRITICAL_FROM_ISR(int_status);
}

void sx128x::handleDio0Rise() {
    _packetIndex = 0;
    uint8_t rxbuf[2] = {0};
    executeOpcodeRead(OP_RX_BUFFER_STATUS_8X, rxbuf, 2);

    // If implicit header mode is enabled, use pre-set packet length as payload length instead.
    // See SX1280 datasheet v3.2, page 92
    if (_implicitHeaderMode == 0x80) { _rxPacketLength = _payloadLength; }
    else                             { _rxPacketLength = rxbuf[0]; }

    if (_receive_callback) { _receive_callback(_rxPacketLength); }
}

bool sx128x::preInit() {
  pinMode(_ss, OUTPUT);
  digitalWrite(_ss, HIGH);
  
  // TODO: Check if this change causes issues on any platforms
  #if MCU_VARIANT == MCU_ESP32
    #if BOARD_MODEL == BOARD_T3S3 || BOARD_MODEL == BOARD_HELTEC32_V3 || BOARD_MODEL == BOARD_TDECK
      SPI.begin(pin_sclk, pin_miso, pin_mosi, pin_cs);
    #else
      SPI.begin();
    #endif
  #else
    SPI.begin();
  #endif

  // Detect modem (retry for up to 500ms)
  long start = millis();
  uint8_t version_msb;
  uint8_t version_lsb;
  while (((millis() - start) < 500) && (millis() >= start)) {
      version_msb = readRegister(REG_FIRM_VER_MSB);
      version_lsb = readRegister(REG_FIRM_VER_LSB);
      if ((version_msb == 0xB7 && version_lsb == 0xA9) || (version_msb == 0xB5 && version_lsb == 0xA9)) { break; }
      delay(100);
  }

  if ((version_msb != 0xB7 || version_lsb != 0xA9) && (version_msb != 0xB5 || version_lsb != 0xA9)) { return false; }
  _preinit_done = true;
  return true;
}

uint8_t ISR_VECT sx128x::readRegister(uint16_t address) { return singleTransfer(OP_READ_REGISTER_8X, address, 0x00); }
void sx128x::writeRegister(uint16_t address, uint8_t value) { singleTransfer(OP_WRITE_REGISTER_8X, address, value); }

uint8_t ISR_VECT sx128x::singleTransfer(uint8_t opcode, uint16_t address, uint8_t value) {
    waitOnBusy();
    uint8_t response;
    digitalWrite(_ss, LOW);

    SPI.beginTransaction(_spiSettings);
    SPI.transfer(opcode);
    SPI.transfer((address & 0xFF00) >> 8);
    SPI.transfer(address & 0x00FF);
    if (opcode == OP_READ_REGISTER_8X) { SPI.transfer(0x00); }
    response = SPI.transfer(value);
    SPI.endTransaction();
    digitalWrite(_ss, HIGH);

    return response;
}

void sx128x::rxAntEnable() {
    if (_txen != -1) { digitalWrite(_txen, LOW); }
    if (_rxen != -1) { digitalWrite(_rxen, HIGH); }
}

void sx128x::txAntEnable() {
    if (_txen != -1) { digitalWrite(_txen, HIGH); }
    if (_rxen != -1) { digitalWrite(_rxen, LOW); }
}

void sx128x::loraMode() {
    uint8_t mode = MODE_LONG_RANGE_MODE_8X;
    executeOpcode(OP_PACKET_TYPE_8X, &mode, 1);
}

void sx128x::waitOnBusy() {
  unsigned long time = millis();
  while (digitalRead(_busy) == HIGH) {
    if (millis() >= (time + 100)) { break; }
  }
}

void sx128x::executeOpcode(uint8_t opcode, uint8_t *buffer, uint8_t size) {
    waitOnBusy();
    digitalWrite(_ss, LOW);
    SPI.beginTransaction(_spiSettings);
    SPI.transfer(opcode);
    for (int i = 0; i < size; i++) { SPI.transfer(buffer[i]); }
    SPI.endTransaction();
    digitalWrite(_ss, HIGH);
}

void sx128x::executeOpcodeRead(uint8_t opcode, uint8_t *buffer, uint8_t size) {
    waitOnBusy();
    digitalWrite(_ss, LOW);
    SPI.beginTransaction(_spiSettings);
    SPI.transfer(opcode);
    SPI.transfer(0x00);
    for (int i = 0; i < size; i++) { buffer[i] = SPI.transfer(0x00); }
    SPI.endTransaction();
    digitalWrite(_ss, HIGH);
}

void sx128x::writeBuffer(const uint8_t* buffer, size_t size) {
    waitOnBusy();
    digitalWrite(_ss, LOW);
    SPI.beginTransaction(_spiSettings);
    SPI.transfer(OP_FIFO_WRITE_8X);
    SPI.transfer(_fifo_tx_addr_ptr);
    for (int i = 0; i < size; i++) { SPI.transfer(buffer[i]); _fifo_tx_addr_ptr++; }
    SPI.endTransaction();
    digitalWrite(_ss, HIGH);
}

void sx128x::readBuffer(uint8_t* buffer, size_t size) {
    waitOnBusy();
    digitalWrite(_ss, LOW);
    SPI.beginTransaction(_spiSettings);
    SPI.transfer(OP_FIFO_READ_8X);
    SPI.transfer(_fifo_rx_addr_ptr);
    SPI.transfer(0x00);
    for (int i = 0; i < size; i++) { buffer[i] = SPI.transfer(0x00); }
    SPI.endTransaction();
    digitalWrite(_ss, HIGH);
}

void sx128x::setModulationParams(uint8_t sf, uint8_t bw, uint8_t cr) {
  // because there is no access to these registers on the sx1280, we have
  // to set all these parameters at once or not at all.
  uint8_t buf[3];
  buf[0] = sf << 4;
  buf[1] = bw;
  buf[2] = cr;
  executeOpcode(OP_MODULATION_PARAMS_8X, buf, 3);

  if (sf <= 6) {      writeRegister(0x925, 0x1E); }
  else if (sf <= 8) { writeRegister(0x925, 0x37); }
  else if (sf >= 9) { writeRegister(0x925, 0x32); }
  writeRegister(0x093C, 0x1);
}

uint8_t preamble_e = 0;
uint8_t preamble_m = 0;
uint32_t last_me_result_target = 0;
extern long lora_preamble_symbols;
void sx128x::setPacketParams(uint32_t target_preamble_symbols, uint8_t headermode, uint8_t payload_length, uint8_t crc) {  
  if (last_me_result_target != target_preamble_symbols) {
    // Calculate exponent and mantissa values for modem
    if (target_preamble_symbols >= 0xF000) target_preamble_symbols = 0xF000;
    uint32_t calculated_preamble_symbols;
    uint8_t e = 1;
    uint8_t m = 1;
    while (e <= 15) {
      while (m <= 15) {
        calculated_preamble_symbols = m * (pow(2,e));
        if (calculated_preamble_symbols >= target_preamble_symbols-4) break;
        m++;
      }

      if (calculated_preamble_symbols >= target_preamble_symbols-4) break;
      m = 1; e++;
    }

    last_me_result_target = target_preamble_symbols;
    lora_preamble_symbols = calculated_preamble_symbols+4;
    _preambleLength = lora_preamble_symbols;

    preamble_e = e;
    preamble_m = m;
  }

  uint8_t buf[7];
  buf[0] = (preamble_e << 4) | preamble_m;
  buf[1] = headermode;
  buf[2] = payload_length;
  buf[3] = crc;
  buf[4] = 0x40; // Standard IQ setting (no inversion)
  buf[5] = 0x00; // Unused params
  buf[6] = 0x00; 

  executeOpcode(OP_PACKET_PARAMS_8X, buf, 7);
}

void sx128x::reset() {
  if (_reset != -1) {
    pinMode(_reset, OUTPUT);
    digitalWrite(_reset, LOW);
    delay(10);
    digitalWrite(_reset, HIGH);
    delay(10);
  }
}

int sx128x::begin(unsigned long frequency) {
  reset();

  if (_rxen != -1) { pinMode(_rxen, OUTPUT); }
  if (_txen != -1) { pinMode(_txen, OUTPUT); }
  if (_busy != -1) { pinMode(_busy, INPUT); }

  if (!_preinit_done) {
    if (!preInit()) {
      return false;
    }
  }

  standby();
  loraMode();
  rxAntEnable();
  setFrequency(frequency);

  // TODO: Implement LNA boost
  //writeRegister(REG_LNA, 0x96);

  setModulationParams(_sf, _bw, _cr);
  setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
  setTxPower(_txp);

  // Set base addresses
  uint8_t basebuf[2] = {0};
  executeOpcode(OP_BUFFER_BASE_ADDR_8X, basebuf, 2);

  _radio_online = true;
  return 1;
}

void sx128x::end() {
  sleep();
  SPI.end();
  _bitrate = 0;
  _radio_online = false;
  _preinit_done = false;
}

int sx128x::beginPacket(int implicitHeader) {
  standby();

  if (implicitHeader) { implicitHeaderMode(); }
  else { explicitHeaderMode(); }

  _payloadLength = 0;
  _fifo_tx_addr_ptr = 0;
  setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);

  return 1;
}

int sx128x::endPacket() {
  setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
  txAntEnable();

  // Put in single TX mode
  uint8_t timeout[3] = {0};
  executeOpcode(OP_TX_8X, timeout, 3);

  uint8_t buf[2];
  buf[0] = 0x00;
  buf[1] = 0x00;
  executeOpcodeRead(OP_GET_IRQ_STATUS_8X, buf, 2);

  // Wait for TX done
  bool timed_out = false;
  uint32_t w_timeout = millis()+LORA_MODEM_TIMEOUT_MS;
  while ((millis() < w_timeout) && ((buf[1] & IRQ_TX_DONE_MASK_8X) == 0)) {
    buf[0] = 0x00;
    buf[1] = 0x00;
    executeOpcodeRead(OP_GET_IRQ_STATUS_8X, buf, 2);
    yield();
  }

  if (!(millis() < w_timeout)) { timed_out = true; }

  // clear IRQ's
  uint8_t mask[2];
  mask[0] = 0x00;
  mask[1] = IRQ_TX_DONE_MASK_8X;
  executeOpcode(OP_CLEAR_IRQ_STATUS_8X, mask, 2);
  
  if (timed_out) { return 0; }
  else           { return 1; }
}

unsigned long preamble_detected_at = 0;
extern long lora_preamble_time_ms;
extern long lora_header_time_ms;
bool false_preamble_detected = false;
bool sx128x::dcd() {
  uint8_t buf[2] = {0}; executeOpcodeRead(OP_GET_IRQ_STATUS_8X, buf, 2);
  uint32_t now = millis();

  bool header_detected = false;
  bool carrier_detected = false;

  if ((buf[1] & IRQ_HEADER_DET_MASK_8X) != 0) { header_detected = true; carrier_detected = true; }
  else { header_detected = false; }

  if ((buf[0] & IRQ_PREAMBLE_DET_MASK_8X) != 0) {
    carrier_detected = true;
    if (preamble_detected_at == 0) { preamble_detected_at = now; }
    if (now - preamble_detected_at > lora_preamble_time_ms + lora_header_time_ms) {
      preamble_detected_at = 0;
      if (!header_detected) { false_preamble_detected = true; }
      uint8_t clearbuf[2]  = {0}; clearbuf[0] = IRQ_PREAMBLE_DET_MASK_8X;
      executeOpcode(OP_CLEAR_IRQ_STATUS_8X, clearbuf, 2);
    }
  }

  // TODO: Maybe there's a way of unlatching the RSSI
  // status without re-activating receive mode?
  if (false_preamble_detected) { sx128x_modem.receive(); false_preamble_detected = false; }
  return carrier_detected;
}


uint8_t sx128x::currentRssiRaw() {
    uint8_t byte = 0;
    executeOpcodeRead(OP_CURRENT_RSSI_8X, &byte, 1);
    return byte;
}

int ISR_VECT sx128x::currentRssi() {
    uint8_t byte = 0;
    executeOpcodeRead(OP_CURRENT_RSSI_8X, &byte, 1);
    int rssi = -byte / 2;
    return rssi;
}

uint8_t sx128x::packetRssiRaw() {
    uint8_t buf[5] = {0};
    executeOpcodeRead(OP_PACKET_STATUS_8X, buf, 5);
    return buf[0];
}

int ISR_VECT sx128x::packetRssi(uint8_t pkt_snr_raw) {
    // TODO: May need more calculations here
    uint8_t buf[5] = {0};
    executeOpcodeRead(OP_PACKET_STATUS_8X, buf, 5);
    int pkt_rssi = -buf[0] / 2;
    return pkt_rssi;
}

uint8_t ISR_VECT sx128x::packetSnrRaw() {
    uint8_t buf[5] = {0};
    executeOpcodeRead(OP_PACKET_STATUS_8X, buf, 5);
    return buf[1];
}

float ISR_VECT sx128x::packetSnr() {
    uint8_t buf[5] = {0};
    executeOpcodeRead(OP_PACKET_STATUS_8X, buf, 5);
    return float(buf[1]) * 0.25;
}

long sx128x::packetFrequencyError() {
  // TODO: Implement this, page 120 of sx1280 datasheet
  int32_t freqError = 0;
  const float fError = 0.0;
  return static_cast<long>(fError);
}

void sx128x::flush() { }
int ISR_VECT sx128x::available() { return _rxPacketLength - _packetIndex; }
size_t sx128x::write(uint8_t byte) { return write(&byte, sizeof(byte)); }
size_t sx128x::write(const uint8_t *buffer, size_t size) {
  if ((_payloadLength + size) > MAX_PKT_LENGTH) { size = MAX_PKT_LENGTH - _payloadLength; }
  writeBuffer(buffer, size);
  _payloadLength = _payloadLength + size;
  return size;
}

int ISR_VECT sx128x::read() {
  if (!available()) { return -1; }

  // If received new packet
  if (_packetIndex == 0) {
    uint8_t rxbuf[2] = {0};
    executeOpcodeRead(OP_RX_BUFFER_STATUS_8X, rxbuf, 2);
    int size;
    
    // If implicit header mode is enabled, read packet length as payload length instead.
    // See SX1280 datasheet v3.2, page 92
    if (_implicitHeaderMode == 0x80) {
      size = _payloadLength;
    } else {
      size = rxbuf[0];
    }

    _fifo_rx_addr_ptr = rxbuf[1];
    if (size > 255) { size = 255; }

    readBuffer(_packet, size);
  }

  uint8_t byte = _packet[_packetIndex];
  _packetIndex++;
  return byte;
}

int sx128x::peek() {
  if (!available()) { return -1; }
  uint8_t b = _packet[_packetIndex];
  return b;
}


void sx128x::onReceive(void(*callback)(int)) {
  _receive_callback = callback;

  if (callback) {
    pinMode(_dio0, INPUT);

    // Set preamble and header detection irqs, plus dio0 mask
    uint8_t buf[8];

    // Set irq masks, enable all
    buf[0] = 0xFF; 
    buf[1] = 0xFF;

    // On the SX1280, no RxDone IRQ is generated if a packet is received with
    // an invalid header, but the modem will be taken out of single RX mode.
    // This can cause the modem to not receive packets until it is reset
    // again. This is documented as Errata 16.2 in the SX1280 datasheet v3.2
    // (page 150) Below, the header error IRQ is mapped to dio0 so that the
    // modem can be set into RX mode again on reception of a corrupted
    // header.
    // set dio0 masks
    buf[2] = 0x00;
    buf[3] = IRQ_RX_DONE_MASK_8X | IRQ_HEADER_ERROR_MASK_8X; 

    // Set dio1 masks
    buf[4] = 0x00; 
    buf[5] = 0x00;

    // Set dio2 masks
    buf[6] = 0x00; 
    buf[7] = 0x00;

    executeOpcode(OP_SET_IRQ_FLAGS_8X, buf, 8);

    #ifdef SPI_HAS_NOTUSINGINTERRUPT
        SPI.usingInterrupt(digitalPinToInterrupt(_dio0));
    #endif

    attachInterrupt(digitalPinToInterrupt(_dio0), onDio0Rise, RISING);

  } else {
    detachInterrupt(digitalPinToInterrupt(_dio0));
    #ifdef SPI_HAS_NOTUSINGINTERRUPT
      _spiModem->notUsingInterrupt(digitalPinToInterrupt(_dio0));
    #endif
  }
}

void sx128x::receive(int size) {
  if (size > 0) {
    implicitHeaderMode();
    // Tell radio payload length
    //_rxPacketLength = size;
    //_payloadLength = size;
    //setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
  } else {
    explicitHeaderMode();
  }

  rxAntEnable();

  // On the SX1280, there is a bug which can cause the busy line
  // to remain high if a high amount of packets are received when
  // in continuous RX mode. This is documented as Errata 16.1 in
  // the SX1280 datasheet v3.2 (page 149)
  // Therefore, the modem is set to single RX mode below instead.

  // uint8_t mode[3] = {0x03, 0xFF, 0xFF}; // Countinuous RX mode
  uint8_t mode[3] = {0}; // single RX mode
  executeOpcode(OP_RX_8X, mode, 3);
}

void sx128x::standby() {
    uint8_t byte = 0x01; // Always use STDBY_XOSC
    executeOpcode(OP_STANDBY_8X, &byte, 1); 
}

void sx128x::setPins(int ss, int reset, int dio0, int busy, int rxen, int txen) {
  _ss = ss;
  _reset = reset;
  _dio0 = dio0;
  _busy = busy;
  _rxen = rxen;
  _txen = txen;
}

void sx128x::setTxPower(int level, int outputPin) {
    uint8_t tx_buf[2];

    // RAK4631 with WisBlock SX1280 module (LIBSYS002)
    #if BOARD_VARIANT == MODEL_13 || BOARD_VARIANT == MODEL_21
      if (level > 27)     { level = 27; }
      else if (level < 0) { level = 0; }

      _txp = level;
      int reg_value;
      switch (level) {
        case 0:
          reg_value = -18;
          break;
        case 1:
          reg_value = -16;
          break;
        case 2:
          reg_value = -15;
          break;
        case 3:
          reg_value = -14;
          break;
        case 4:
          reg_value = -13;
          break;
        case 5:
          reg_value = -12;
          break;
        case 6:
          reg_value = -11;
          break;
        case 7:
          reg_value = -9;
          break;
        case 8:
          reg_value = -8;
          break;
        case 9:
          reg_value = -7;
          break;
        case 10:
          reg_value = -6;
          break;
        case 11:
          reg_value = -5;
          break;
        case 12:
          reg_value = -4;
          break;
        case 13:
          reg_value = -3;
          break;
        case 14:
          reg_value = -2;
          break;
        case 15:
          reg_value = -1;
          break;
        case 16:
          reg_value = 0;
          break;
        case 17:
          reg_value = 1;
          break;
        case 18:
          reg_value = 2;
          break;
        case 19:
          reg_value = 3;
          break;
        case 20:
          reg_value = 4;
          break;
        case 21:
          reg_value = 5;
          break;
        case 22:
          reg_value = 6;
          break;
        case 23:
          reg_value = 7;
          break;
        case 24:
          reg_value = 8;
          break;
        case 25:
          reg_value = 9;
          break;
        case 26:
          reg_value = 12;
          break;
        case 27:
          reg_value = 13;
          break;
        default:
          reg_value = 0;
          break;
      }

      tx_buf[0] = reg_value + 18;
      tx_buf[1] = 0xE0; // Ramping time, 20 microseconds
      executeOpcode(OP_TX_PARAMS_8X, tx_buf, 2);

    // T3S3 SX1280 PA
    #elif BOARD_VARIANT == MODEL_AC
      if (level > 20) { level = 20; }
      else if (level < 0) { level = 0; }

      _txp = level;
      int reg_value;
      switch (level) {
        case 0:
          reg_value = -18;
          break;
        case 1:
          reg_value = -17;
          break;
        case 2:
          reg_value = -16;
          break;
        case 3:
          reg_value = -15;
          break;
        case 4:
          reg_value = -14;
          break;
        case 5:
          reg_value = -13;
          break;
        case 6:
          reg_value = -12;
          break;
        case 7:
          reg_value = -10;
          break;
        case 8:
          reg_value = -9;
          break;
        case 9:
          reg_value = -8;
          break;
        case 10:
          reg_value = -7;
          break;
        case 11:
          reg_value = -6;
          break;
        case 12:
          reg_value = -5;
          break;
        case 13:
          reg_value = -4;
          break;
        case 14:
          reg_value = -3;
          break;
        case 15:
          reg_value = -2;
          break;
        case 16:
          reg_value = -1;
          break;
        case 17:
          reg_value = 0;
          break;
        case 18:
          reg_value = 1;
          break;
        case 19:
          reg_value = 2;
          break;
        case 20:
          reg_value = 3;
          break;
        default:
          reg_value = 0;
          break;
      }
      tx_buf[0] = reg_value;
      tx_buf[1] = 0xE0; // Ramping time, 20 microseconds

    // For SX1280 boards with no specific PA requirements
    #else
      if (level > 13)       { level = 13; }
      else if (level < -18) { level = -18; }
      _txp = level;
      tx_buf[0] = level + 18;
      tx_buf[1] = 0xE0; // Ramping time, 20 microseconds
    #endif
    
    executeOpcode(OP_TX_PARAMS_8X, tx_buf, 2);
}

void sx128x::setFrequency(uint32_t frequency) {
  _frequency = frequency;
  uint8_t buf[3];
  uint32_t freq = (uint32_t)((double)frequency / (double)FREQ_STEP_8X);
  buf[0] = ((freq >> 16) & 0xFF);
  buf[1] = ((freq >> 8) & 0xFF);
  buf[2] = (freq & 0xFF);

  executeOpcode(OP_RF_FREQ_8X, buf, 3);
}

uint32_t sx128x::getFrequency() {
  // We can't read the frequency on the sx1280
  uint32_t frequency = _frequency;
  return frequency;
}

void sx128x::setSpreadingFactor(int sf) {
  if (sf < 5) { sf = 5; }
  else if (sf > 12) { sf = 12; }
  _sf = sf;

  setModulationParams(sf, _bw, _cr);
  handleLowDataRate();
}

uint32_t sx128x::getSignalBandwidth() {
  int bw = _bw;
  switch (bw) {
    case 0x34: return 203.125E3;
    case 0x26: return 406.25E3;
    case 0x18: return 812.5E3;
    case 0x0A: return 1625E3;
  }
  
  return 0;
}

void sx128x::setSignalBandwidth(uint32_t sbw) {
  if      (sbw <= 203.125E3) { _bw = 0x34; }
  else if (sbw <= 406.25E3)  { _bw = 0x26; }
  else if (sbw <= 812.5E3)   { _bw = 0x18; }
  else                       { _bw = 0x0A; }

  setModulationParams(_sf, _bw, _cr);
  handleLowDataRate();
  optimizeModemSensitivity();
}

// TODO: add support for new interleaving scheme, see page 117 of sx1280 datasheet
void sx128x::setCodingRate4(int denominator) {
  if (denominator < 5) { denominator = 5; }
  else if (denominator > 8) { denominator = 8; }
  _cr = denominator - 4;
  setModulationParams(_sf, _bw, _cr);
}

extern bool lora_low_datarate;
void sx128x::handleLowDataRate() {
  if (_sf > 10) { lora_low_datarate = true; }
  else          { lora_low_datarate = false; }
}

void sx128x::optimizeModemSensitivity() { } // TODO: Check if there's anything the sx1280 can do here
uint8_t sx128x::getCodingRate4() { return _cr + 4; }
void sx128x::setPreambleLength(long preamble_symbols) { setPacketParams(preamble_symbols, _implicitHeaderMode, _payloadLength, _crcMode); }
void sx128x::setSyncWord(int sw) { } // TODO: Implement
void sx128x::enableTCXO() { } // TODO: Need to check how to implement on sx1280
void sx128x::disableTCXO() { } // TODO: Need to check how to implement on sx1280
void sx128x::sleep() { uint8_t byte = 0x00; executeOpcode(OP_SLEEP_8X, &byte, 1); }
uint8_t sx128x::getTxPower() { return _txp; }
uint8_t sx128x::getSpreadingFactor() { return _sf; }
void sx128x::enableCrc() { _crcMode = 0x20; setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode); }
void sx128x::disableCrc() { _crcMode = 0; setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode); }
void sx128x::setSPIFrequency(uint32_t frequency) { _spiSettings = SPISettings(frequency, MSBFIRST, SPI_MODE0); }
void sx128x::explicitHeaderMode() {  _implicitHeaderMode = 0; setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode); }
void sx128x::implicitHeaderMode() { _implicitHeaderMode = 0x80; setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode); }
void sx128x::dumpRegisters(Stream& out) { for (int i = 0; i < 128; i++) { out.print("0x"); out.print(i, HEX); out.print(": 0x"); out.println(readRegister(i), HEX); } }

sx128x sx128x_modem;
#endif