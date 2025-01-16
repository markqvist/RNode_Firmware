// Copyright Sandeep Mistry, Mark Qvist and Jacob Eva.
// Licensed under the MIT license.

#include "Boards.h"

#if MODEM == SX1262
#include "sx126x.h"

#if MCU_VARIANT == MCU_ESP32
  #if MCU_VARIANT == MCU_ESP32 and !defined(CONFIG_IDF_TARGET_ESP32S3)
    #include "soc/rtc_wdt.h"
  #endif
  #define ISR_VECT IRAM_ATTR
#else
  #define ISR_VECT
#endif

#define OP_RF_FREQ_6X               0x86
#define OP_SLEEP_6X                 0x84
#define OP_STANDBY_6X               0x80
#define OP_TX_6X                    0x83
#define OP_RX_6X                    0x82
#define OP_PA_CONFIG_6X             0x95
#define OP_SET_IRQ_FLAGS_6X         0x08 // Also provides info such as
                                         // preamble detection, etc for
                                         // knowing when it's safe to switch
                                         // antenna modes
#define OP_CLEAR_IRQ_STATUS_6X      0x02
#define OP_GET_IRQ_STATUS_6X        0x12
#define OP_RX_BUFFER_STATUS_6X      0x13
#define OP_PACKET_STATUS_6X         0x14 // Get snr & rssi of last packet
#define OP_CURRENT_RSSI_6X          0x15
#define OP_MODULATION_PARAMS_6X     0x8B // BW, SF, CR, etc.
#define OP_PACKET_PARAMS_6X         0x8C // CRC, preamble, payload length, etc.
#define OP_STATUS_6X                0xC0
#define OP_TX_PARAMS_6X             0x8E // Set dbm, etc
#define OP_PACKET_TYPE_6X           0x8A
#define OP_BUFFER_BASE_ADDR_6X      0x8F
#define OP_READ_REGISTER_6X         0x1D
#define OP_WRITE_REGISTER_6X        0x0D
#define OP_DIO3_TCXO_CTRL_6X        0x97
#define OP_DIO2_RF_CTRL_6X          0x9D
#define OP_CAD_PARAMS               0x88
#define OP_CALIBRATE_6X             0x89
#define OP_RX_TX_FALLBACK_MODE_6X   0x93
#define OP_REGULATOR_MODE_6X        0x96
#define OP_CALIBRATE_IMAGE_6X       0x98

#define MASK_CALIBRATE_ALL          0x7f

#define IRQ_TX_DONE_MASK_6X         0x01
#define IRQ_RX_DONE_MASK_6X         0x02
#define IRQ_HEADER_DET_MASK_6X      0x10
#define IRQ_PREAMBLE_DET_MASK_6X    0x04
#define IRQ_PAYLOAD_CRC_ERROR_MASK_6X 0x40
#define IRQ_ALL_MASK_6X             0b0100001111111111

#define MODE_LONG_RANGE_MODE_6X     0x01

#define OP_FIFO_WRITE_6X            0x0E
#define OP_FIFO_READ_6X             0x1E
#define REG_OCP_6X                0x08E7
#define REG_LNA_6X                0x08AC // No agc in sx1262
#define REG_SYNC_WORD_MSB_6X      0x0740
#define REG_SYNC_WORD_LSB_6X      0x0741
#define REG_PAYLOAD_LENGTH_6X     0x0702 // https://github.com/beegee-tokyo/SX126x-Arduino/blob/master/src/radio/sx126x/sx126x.h#L98
#define REG_RANDOM_GEN_6X         0x0819

#define MODE_TCXO_3_3V_6X           0x07
#define MODE_TCXO_3_0V_6X           0x06
#define MODE_TCXO_2_7V_6X           0x06
#define MODE_TCXO_2_4V_6X           0x06
#define MODE_TCXO_2_2V_6X           0x03
#define MODE_TCXO_1_8V_6X           0x02
#define MODE_TCXO_1_7V_6X           0x01
#define MODE_TCXO_1_6V_6X           0x00

#define MODE_STDBY_RC_6X            0x00
#define MODE_STDBY_XOSC_6X          0x01
#define MODE_FALLBACK_STDBY_RC_6X   0x20
#define MODE_IMPLICIT_HEADER        0x01
#define MODE_EXPLICIT_HEADER        0x00

#define SYNC_WORD_6X              0x1424

#define XTAL_FREQ_6X (double)32000000
#define FREQ_DIV_6X  (double)pow(2.0, 25.0)
#define FREQ_STEP_6X (double)(XTAL_FREQ_6X / FREQ_DIV_6X)

#if BOARD_MODEL == BOARD_TECHO
  SPIClass spim3 = SPIClass(NRF_SPIM3, pin_miso, pin_sclk, pin_mosi) ;
  #define SPI spim3

#elif defined(NRF52840_XXAA)
  extern SPIClass spiModem;
  #define SPI spiModem
#endif

extern SPIClass SPI;

#define MAX_PKT_LENGTH 255

sx126x::sx126x() :
  _spiSettings(16E6, MSBFIRST, SPI_MODE0),
  _ss(LORA_DEFAULT_SS_PIN), _reset(LORA_DEFAULT_RESET_PIN), _dio0(LORA_DEFAULT_DIO0_PIN), _busy(LORA_DEFAULT_BUSY_PIN), _rxen(LORA_DEFAULT_RXEN_PIN),
  _frequency(0),
  _txp(0),
  _sf(0x07),
  _bw(0x04),
  _cr(0x01),
  _ldro(0x00),
  _packetIndex(0),
  _preambleLength(18),
  _implicitHeaderMode(0),
  _payloadLength(255),
  _crcMode(1),
  _fifo_tx_addr_ptr(0),
  _fifo_rx_addr_ptr(0),
  _packet({0}),
  _preinit_done(false),
  _onReceive(NULL)
{ setTimeout(0); }

bool sx126x::preInit() {
  pinMode(_ss, OUTPUT);
  digitalWrite(_ss, HIGH);
  
  #if BOARD_MODEL == BOARD_T3S3 || BOARD_MODEL == BOARD_HELTEC32_V3 || BOARD_MODEL == BOARD_TDECK
    SPI.begin(pin_sclk, pin_miso, pin_mosi, pin_cs);
  #elif BOARD_MODEL == BOARD_TECHO
    SPI.setPins(pin_miso, pin_sclk, pin_mosi);
    SPI.begin();
  #else
    SPI.begin();
  #endif

  // Check version (retry for up to 2 seconds)
  // TODO: Actually read version registers, not syncwords
  long start = millis();
  uint8_t syncmsb;
  uint8_t synclsb;
  while (((millis() - start) < 2000) && (millis() >= start)) {
      syncmsb = readRegister(REG_SYNC_WORD_MSB_6X);
      synclsb = readRegister(REG_SYNC_WORD_LSB_6X);
      if ( uint16_t(syncmsb << 8 | synclsb) == 0x1424 || uint16_t(syncmsb << 8 | synclsb) == 0x4434) {
          break;
      }
      delay(100);
  }
  if ( uint16_t(syncmsb << 8 | synclsb) != 0x1424 && uint16_t(syncmsb << 8 | synclsb) != 0x4434) {
      return false;
  }

  _preinit_done = true;
  return true;
}

uint8_t ISR_VECT sx126x::readRegister(uint16_t address) {
  return singleTransfer(OP_READ_REGISTER_6X, address, 0x00);
}

void sx126x::writeRegister(uint16_t address, uint8_t value) {
  singleTransfer(OP_WRITE_REGISTER_6X, address, value);
}

uint8_t ISR_VECT sx126x::singleTransfer(uint8_t opcode, uint16_t address, uint8_t value) {
  waitOnBusy();
  
  uint8_t response;
  digitalWrite(_ss, LOW);
  SPI.beginTransaction(_spiSettings);
  SPI.transfer(opcode);
  SPI.transfer((address & 0xFF00) >> 8);
  SPI.transfer(address & 0x00FF);
  if (opcode == OP_READ_REGISTER_6X) { SPI.transfer(0x00); }
  response = SPI.transfer(value);
  SPI.endTransaction();

  digitalWrite(_ss, HIGH);

  return response;
}

void sx126x::rxAntEnable() {
  if (_rxen != -1) { digitalWrite(_rxen, HIGH); }
}

void sx126x::loraMode() {
  // Enable lora mode on the SX1262 chip
  uint8_t mode = MODE_LONG_RANGE_MODE_6X;
  executeOpcode(OP_PACKET_TYPE_6X, &mode, 1);
}

void sx126x::waitOnBusy() {
  unsigned long time = millis();
  if (_busy != -1) {
    while (digitalRead(_busy) == HIGH) {
        if (millis() >= (time + 100)) { break; }
    }
  }
}

void sx126x::executeOpcode(uint8_t opcode, uint8_t *buffer, uint8_t size) {
  waitOnBusy();
  digitalWrite(_ss, LOW);
  SPI.beginTransaction(_spiSettings);
  SPI.transfer(opcode);
  for (int i = 0; i < size; i++) { SPI.transfer(buffer[i]); }
  SPI.endTransaction();
  digitalWrite(_ss, HIGH);
}

void sx126x::executeOpcodeRead(uint8_t opcode, uint8_t *buffer, uint8_t size) {
  waitOnBusy();
  digitalWrite(_ss, LOW);
  SPI.beginTransaction(_spiSettings);
  SPI.transfer(opcode);
  SPI.transfer(0x00);
  for (int i = 0; i < size; i++) { buffer[i] = SPI.transfer(0x00); }
  SPI.endTransaction();
  digitalWrite(_ss, HIGH);
}

void sx126x::writeBuffer(const uint8_t* buffer, size_t size) {
  waitOnBusy();
  digitalWrite(_ss, LOW);
  SPI.beginTransaction(_spiSettings);
  SPI.transfer(OP_FIFO_WRITE_6X);
  SPI.transfer(_fifo_tx_addr_ptr);
  for (int i = 0; i < size; i++) { SPI.transfer(buffer[i]); _fifo_tx_addr_ptr++; }
  SPI.endTransaction();
  digitalWrite(_ss, HIGH);
}

void sx126x::readBuffer(uint8_t* buffer, size_t size) {
  waitOnBusy();
  digitalWrite(_ss, LOW);
  SPI.beginTransaction(_spiSettings);
  SPI.transfer(OP_FIFO_READ_6X);
  SPI.transfer(_fifo_rx_addr_ptr);
  SPI.transfer(0x00);
  for (int i = 0; i < size; i++) { buffer[i] = SPI.transfer(0x00); }
  SPI.endTransaction();
  digitalWrite(_ss, HIGH);
}

void sx126x::setModulationParams(uint8_t sf, uint8_t bw, uint8_t cr, int ldro) {
  // Because there is no access to these registers on the sx1262, we have
  // to set all these parameters at once or not at all.
  uint8_t buf[8];
  buf[0] = sf;
  buf[1] = bw;
  buf[2] = cr; 
  buf[3] = ldro; // Low data rate toggle
  buf[4] = 0x00; // Unused params in LoRa mode
  buf[5] = 0x00;
  buf[6] = 0x00;
  buf[7] = 0x00;
  executeOpcode(OP_MODULATION_PARAMS_6X, buf, 8);
}

void sx126x::setPacketParams(long preamble_symbols, uint8_t headermode, uint8_t payload_length, uint8_t crc) {
  // Because there is no access to these registers on the sx1262, we have
  // to set all these parameters at once or not at all.
  uint8_t buf[9];
  buf[0] = uint8_t((preamble_symbols & 0xFF00) >> 8);
  buf[1] = uint8_t((preamble_symbols & 0x00FF));
  buf[2] = headermode;
  buf[3] = payload_length;
  buf[4] = crc;
  buf[5] = 0x00; // standard IQ setting (no inversion)
  buf[6] = 0x00; // unused params
  buf[7] = 0x00; 
  buf[8] = 0x00; 
  executeOpcode(OP_PACKET_PARAMS_6X, buf, 9);
}

void sx126x::reset(void) {
  if (_reset != -1) {
    pinMode(_reset, OUTPUT);
    digitalWrite(_reset, LOW);
    delay(10);
    digitalWrite(_reset, HIGH);
    delay(10);
  }
}

void sx126x::calibrate(void) {
  // Put in STDBY_RC mode before calibration
  uint8_t mode_byte = MODE_STDBY_RC_6X;
  executeOpcode(OP_STANDBY_6X, &mode_byte, 1);

  // Calibrate RC64k, RC13M, PLL, ADC and image
  uint8_t calibrate = MASK_CALIBRATE_ALL;
  executeOpcode(OP_CALIBRATE_6X, &calibrate, 1);

  delay(5);
  waitOnBusy();
}

void sx126x::calibrate_image(long frequency) {
  uint8_t image_freq[2] = {0};
  if      (frequency >= 430E6 && frequency <= 440E6) { image_freq[0] = 0x6B; image_freq[1] = 0x6F; }
  else if (frequency >= 470E6 && frequency <= 510E6) { image_freq[0] = 0x75; image_freq[1] = 0x81; }
  else if (frequency >= 779E6 && frequency <= 787E6) { image_freq[0] = 0xC1; image_freq[1] = 0xC5; }
  else if (frequency >= 863E6 && frequency <= 870E6) { image_freq[0] = 0xD7; image_freq[1] = 0xDB; }
  else if (frequency >= 902E6 && frequency <= 928E6) { image_freq[0] = 0xE1; image_freq[1] = 0xE9; } // TODO: Allow higher freq calibration
  executeOpcode(OP_CALIBRATE_IMAGE_6X, image_freq, 2);
  waitOnBusy();
}

int sx126x::begin(long frequency) {
  reset();
  
  if (_busy != -1) { pinMode(_busy, INPUT); }
  if (!_preinit_done) { if (!preInit()) { return false; } }
  if (_rxen != -1) { pinMode(_rxen, OUTPUT); }

  calibrate();
  calibrate_image(frequency);
  enableTCXO();
  loraMode();
  standby();

  // Set sync word
  setSyncWord(SYNC_WORD_6X);

  #if DIO2_AS_RF_SWITCH
    // enable dio2 rf switch
    uint8_t byte = 0x01;
    executeOpcode(OP_DIO2_RF_CTRL_6X, &byte, 1);
  #endif

  rxAntEnable();
  setFrequency(frequency);
  setTxPower(2);
  enableCrc();
  writeRegister(REG_LNA_6X, 0x96); // Set LNA boost
  uint8_t basebuf[2] = {0}; // Set base addresses
  executeOpcode(OP_BUFFER_BASE_ADDR_6X, basebuf, 2);

  setModulationParams(_sf, _bw, _cr, _ldro);
  setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);

  return 1;
}

void sx126x::end() { sleep(); SPI.end(); _preinit_done = false; }

int sx126x::beginPacket(int implicitHeader) {
  standby();
  if (implicitHeader) { implicitHeaderMode(); }
  else { explicitHeaderMode(); }

  _payloadLength = 0;
  _fifo_tx_addr_ptr = 0;
  setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);

  return 1;
}

int sx126x::endPacket() {
  setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
  uint8_t timeout[3] = {0}; // Put in single TX mode
  executeOpcode(OP_TX_6X, timeout, 3);

  uint8_t buf[2];
  buf[0] = 0x00;
  buf[1] = 0x00;
  executeOpcodeRead(OP_GET_IRQ_STATUS_6X, buf, 2);

  // Wait for TX done
  bool timed_out = false;
  uint32_t w_timeout = millis()+LORA_MODEM_TIMEOUT_MS;
  while ((millis() < w_timeout) && ((buf[1] & IRQ_TX_DONE_MASK_6X) == 0)) {
    buf[0] = 0x00;
    buf[1] = 0x00;
    executeOpcodeRead(OP_GET_IRQ_STATUS_6X, buf, 2);
    yield();
  }

  if (!(millis() < w_timeout)) { timed_out = true; }

  // Clear IRQs
  uint8_t mask[2];
  mask[0] = 0x00;
  mask[1] = IRQ_TX_DONE_MASK_6X;
  executeOpcode(OP_CLEAR_IRQ_STATUS_6X, mask, 2);
  if (timed_out) { return 0; } else { return 1; }
}

unsigned long preamble_detected_at = 0;
extern long lora_preamble_time_ms;
extern long lora_header_time_ms;
bool false_preamble_detected = false;

bool sx126x::dcd() {
  uint8_t buf[2] = {0}; executeOpcodeRead(OP_GET_IRQ_STATUS_6X, buf, 2);
  uint32_t now = millis();

  bool header_detected = false;
  bool carrier_detected = false;

  if ((buf[1] & IRQ_HEADER_DET_MASK_6X) != 0) { header_detected = true; carrier_detected = true; }
  else { header_detected = false; }

  if ((buf[1] & IRQ_PREAMBLE_DET_MASK_6X) != 0) {
    carrier_detected = true;
    if (preamble_detected_at == 0) { preamble_detected_at = now; }
    if (now - preamble_detected_at > lora_preamble_time_ms + lora_header_time_ms) {
      preamble_detected_at = 0;
      if (!header_detected) { false_preamble_detected = true; }
      uint8_t clearbuf[2] = {0};
      clearbuf[1] = IRQ_PREAMBLE_DET_MASK_6X;
      executeOpcode(OP_CLEAR_IRQ_STATUS_6X, clearbuf, 2);
    }
  }

  // TODO: Maybe there's a way of unlatching the RSSI
  // status without re-activating receive mode?
  if (false_preamble_detected) { sx126x_modem.receive(); false_preamble_detected = false; }
  return carrier_detected;
}

uint8_t sx126x::currentRssiRaw() {
  uint8_t byte = 0;
  executeOpcodeRead(OP_CURRENT_RSSI_6X, &byte, 1);
  return byte;
}

int ISR_VECT sx126x::currentRssi() {
  uint8_t byte = 0;
  executeOpcodeRead(OP_CURRENT_RSSI_6X, &byte, 1);
  int rssi = -(int(byte)) / 2;
  return rssi;
}

uint8_t sx126x::packetRssiRaw() {
  uint8_t buf[3] = {0};
  executeOpcodeRead(OP_PACKET_STATUS_6X, buf, 3);
  return buf[2];
}

int ISR_VECT sx126x::packetRssi() {
  // TODO: May need more calculations here
  uint8_t buf[3] = {0};
  executeOpcodeRead(OP_PACKET_STATUS_6X, buf, 3);
  int pkt_rssi = -buf[0] / 2;
  return pkt_rssi;
}

int ISR_VECT sx126x::packetRssi(uint8_t pkt_snr_raw) {
  // TODO: May need more calculations here
  uint8_t buf[3] = {0};
  executeOpcodeRead(OP_PACKET_STATUS_6X, buf, 3);
  int pkt_rssi = -buf[0] / 2;
  return pkt_rssi;
}

uint8_t ISR_VECT sx126x::packetSnrRaw() {
  uint8_t buf[3] = {0};
  executeOpcodeRead(OP_PACKET_STATUS_6X, buf, 3);
  return buf[1];
}

float ISR_VECT sx126x::packetSnr() {
  uint8_t buf[3] = {0};
  executeOpcodeRead(OP_PACKET_STATUS_6X, buf, 3);
  return float(buf[1]) * 0.25;
}

long sx126x::packetFrequencyError() {
  // TODO: Implement this, no idea how to check it on the sx1262
  const float fError = 0.0;
  return static_cast<long>(fError);
}

size_t sx126x::write(uint8_t byte) { return write(&byte, sizeof(byte)); }
size_t sx126x::write(const uint8_t *buffer, size_t size) {
  if ((_payloadLength + size) > MAX_PKT_LENGTH) { size = MAX_PKT_LENGTH - _payloadLength; }
  writeBuffer(buffer, size);
  _payloadLength = _payloadLength + size;
  return size;
}

int ISR_VECT sx126x::available() {
  uint8_t buf[2] = {0};
  executeOpcodeRead(OP_RX_BUFFER_STATUS_6X, buf, 2);
  return buf[0] - _packetIndex;
}

int ISR_VECT sx126x::read(){
  if (!available()) { return -1; }
  if (_packetIndex == 0) {
    uint8_t rxbuf[2] = {0};
    executeOpcodeRead(OP_RX_BUFFER_STATUS_6X, rxbuf, 2);
    int size = rxbuf[0];
    _fifo_rx_addr_ptr = rxbuf[1];
    readBuffer(_packet, size);
  }

  uint8_t byte = _packet[_packetIndex];
  _packetIndex++;
  return byte;
}

int sx126x::peek() {
  if (!available()) { return -1; }
  if (_packetIndex == 0) {
      uint8_t rxbuf[2] = {0};
      executeOpcodeRead(OP_RX_BUFFER_STATUS_6X, rxbuf, 2);
      int size = rxbuf[0];
      _fifo_rx_addr_ptr = rxbuf[1];
      readBuffer(_packet, size);
  }

  uint8_t b = _packet[_packetIndex];
  return b;
}

void sx126x::flush() { }

void sx126x::onReceive(void(*callback)(int)){
  _onReceive = callback;

  if (callback) {
    pinMode(_dio0, INPUT);
    uint8_t buf[8]; // Set preamble and header detection irqs, plus dio0 mask
    buf[0] = 0xFF;  // Set irq masks, enable all
    buf[1] = 0xFF;
    buf[2] = 0x00;  // Set dio0 masks
    buf[3] = IRQ_RX_DONE_MASK_6X; 
    buf[4] = 0x00;  // Set dio1 masks
    buf[5] = 0x00;
    buf[6] = 0x00;  // Set dio2 masks 
    buf[7] = 0x00;
    executeOpcode(OP_SET_IRQ_FLAGS_6X, buf, 8);

    #ifdef SPI_HAS_NOTUSINGINTERRUPT
      SPI.usingInterrupt(digitalPinToInterrupt(_dio0));
    #endif
    attachInterrupt(digitalPinToInterrupt(_dio0), sx126x::onDio0Rise, RISING);

  } else {
    detachInterrupt(digitalPinToInterrupt(_dio0));
    #ifdef SPI_HAS_NOTUSINGINTERRUPT
      SPI.notUsingInterrupt(digitalPinToInterrupt(_dio0));
    #endif
  }
}

void sx126x::receive(int size) {
  if (size > 0) {
    implicitHeaderMode();
    _payloadLength = size;
    setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
  } else { explicitHeaderMode(); }

  if (_rxen != -1) { rxAntEnable(); }
  uint8_t mode[3] = {0xFF, 0xFF, 0xFF}; // Continuous mode
  executeOpcode(OP_RX_6X, mode, 3);
}

void sx126x::standby() {
  uint8_t byte = MODE_STDBY_XOSC_6X; // STDBY_XOSC
  executeOpcode(OP_STANDBY_6X, &byte, 1); 
}

void sx126x::sleep() { uint8_t byte = 0x00; executeOpcode(OP_SLEEP_6X, &byte, 1); }

void sx126x::enableTCXO() {
  #if HAS_TCXO
    #if BOARD_MODEL == BOARD_RAK4631 || BOARD_MODEL == BOARD_HELTEC32_V3
      uint8_t buf[4] = {MODE_TCXO_3_3V_6X, 0x00, 0x00, 0xFF};
    #elif BOARD_MODEL == BOARD_TBEAM
      uint8_t buf[4] = {MODE_TCXO_1_8V_6X, 0x00, 0x00, 0xFF};
    #elif BOARD_MODEL == BOARD_TDECK
      uint8_t buf[4] = {MODE_TCXO_1_8V_6X, 0x00, 0x00, 0xFF};
    #elif BOARD_MODEL == BOARD_TBEAM_S_V1
      uint8_t buf[4] = {MODE_TCXO_1_8V_6X, 0x00, 0x00, 0xFF};
    #elif BOARD_MODEL == BOARD_T3S3
      uint8_t buf[4] = {MODE_TCXO_1_8V_6X, 0x00, 0x00, 0xFF};
    #elif BOARD_MODEL == BOARD_HELTEC_T114
      uint8_t buf[4] = {MODE_TCXO_1_8V_6X, 0x00, 0x00, 0xFF};
    #elif BOARD_MODEL == BOARD_TECHO
      uint8_t buf[4] = {MODE_TCXO_1_8V_6X, 0x00, 0x00, 0xFF};
    #endif
    executeOpcode(OP_DIO3_TCXO_CTRL_6X, buf, 4);
  #endif
}

// TODO: Once enabled, SX1262 needs a complete reset to disable TCXO
void sx126x::disableTCXO() { }

void sx126x::setTxPower(int level, int outputPin) {
  // Currently no low power mode for SX1262 implemented, assuming PA boost
  
  // WORKAROUND - Better Resistance of the SX1262 Tx to Antenna Mismatch, see DS_SX1261-2_V1.2 datasheet chapter 15.2
  // RegTxClampConfig = @address 0x08D8
  writeRegister(0x08D8, readRegister(0x08D8) | (0x0F << 1));

  uint8_t pa_buf[4];
  pa_buf[0] = 0x04; // PADutyCycle needs to be 0x04 to achieve 22dBm output, but can be lowered for better efficiency at lower outputs
  pa_buf[1] = 0x07; // HPMax at 0x07 is maximum supported for SX1262
  pa_buf[2] = 0x00; // DeviceSel 0x00 for SX1262 (0x01 for SX1261)
  pa_buf[3] = 0x01; // PALut always 0x01 (reserved according to datasheet)
  executeOpcode(OP_PA_CONFIG_6X, pa_buf, 4); // set pa_config for high power

  if (level > 22) { level = 22; }
  else if (level < -9) { level = -9; }
  writeRegister(REG_OCP_6X, OCP_TUNED); // Use board-specific tuned OCP

  uint8_t tx_buf[2];
  tx_buf[0] = level;
  tx_buf[1] = 0x02; // PA ramping time - 40 microseconds
  executeOpcode(OP_TX_PARAMS_6X, tx_buf, 2);

  _txp = level;
}

uint8_t sx126x::getTxPower() { return _txp; }

void sx126x::setFrequency(long frequency) {
  _frequency = frequency;
  uint8_t buf[4];
  uint32_t freq = (uint32_t)((double)frequency / (double)FREQ_STEP_6X);
  buf[0] = ((freq >> 24) & 0xFF);
  buf[1] = ((freq >> 16) & 0xFF);
  buf[2] = ((freq >> 8) & 0xFF);
  buf[3] = (freq & 0xFF);
  executeOpcode(OP_RF_FREQ_6X, buf, 4);
}

uint32_t sx126x::getFrequency() {
    // We can't read the frequency on the sx1262 / 80
    uint32_t frequency = _frequency;
    return frequency;
}

void sx126x::setSpreadingFactor(int sf) {
  if (sf < 5)       { sf = 5; }
  else if (sf > 12) { sf = 12; }
  _sf = sf;

  handleLowDataRate();
  setModulationParams(sf, _bw, _cr, _ldro);
}

long sx126x::getSignalBandwidth() {
  int bw = _bw;
  switch (bw) {
    case 0x00: return 7.8E3;
    case 0x01: return 15.6E3;
    case 0x02: return 31.25E3;
    case 0x03: return 62.5E3;
    case 0x04: return 125E3;
    case 0x05: return 250E3;
    case 0x06: return 500E3;
    case 0x08: return 10.4E3;
    case 0x09: return 20.8E3;
    case 0x0A: return 41.7E3;
  }
  return 0;
}

extern bool lora_low_datarate;
void sx126x::handleLowDataRate() {
  if ( long( (1<<_sf) / (getSignalBandwidth()/1000)) > 16)
         { _ldro = 0x01; lora_low_datarate = true;  }
    else { _ldro = 0x00; lora_low_datarate = false; }
}

// TODO: Check if there's anything the sx1262 can do here
void sx126x::optimizeModemSensitivity(){ }

void sx126x::setSignalBandwidth(long sbw) {
  if (sbw <= 7.8E3)        { _bw = 0x00; }
  else if (sbw <= 10.4E3)  { _bw = 0x08; }
  else if (sbw <= 15.6E3)  { _bw = 0x01; }
  else if (sbw <= 20.8E3)  { _bw = 0x09; }
  else if (sbw <= 31.25E3) { _bw = 0x02; }
  else if (sbw <= 41.7E3)  { _bw = 0x0A; }
  else if (sbw <= 62.5E3)  { _bw = 0x03; }
  else if (sbw <= 125E3)   { _bw = 0x04; }
  else if (sbw <= 250E3)   { _bw = 0x05; } 
  else                     { _bw = 0x06; }

  handleLowDataRate();
  setModulationParams(_sf, _bw, _cr, _ldro);
  optimizeModemSensitivity();
}

void sx126x::setCodingRate4(int denominator) {
  if (denominator < 5) { denominator = 5; }
  else if (denominator > 8) { denominator = 8; }
  int cr = denominator - 4;
  _cr = cr;
  setModulationParams(_sf, _bw, cr, _ldro);
}

void sx126x::setPreambleLength(long preamble_symbols) {
  _preambleLength = preamble_symbols;
  setPacketParams(preamble_symbols, _implicitHeaderMode, _payloadLength, _crcMode);
}

void sx126x::setSyncWord(uint16_t sw) {
  // TODO: Why was this hardcoded instead of using the config value?
  // writeRegister(REG_SYNC_WORD_MSB_6X, (sw & 0xFF00) >> 8);
  // writeRegister(REG_SYNC_WORD_LSB_6X, sw & 0x00FF);
  writeRegister(REG_SYNC_WORD_MSB_6X, 0x14);
  writeRegister(REG_SYNC_WORD_LSB_6X, 0x24);
}

void sx126x::setPins(int ss, int reset, int dio0, int busy, int rxen) {
  _ss = ss;
  _reset = reset;
  _dio0 = dio0;
  _busy = busy;
  _rxen = rxen;
}

void sx126x::dumpRegisters(Stream& out) {
  for (int i = 0; i < 128; i++) {
    out.print("0x");
    out.print(i, HEX);
    out.print(": 0x");
    out.println(readRegister(i), HEX);
  }
}

void ISR_VECT sx126x::handleDio0Rise() {
  uint8_t buf[2];
  buf[0] = 0x00;
  buf[1] = 0x00;
  executeOpcodeRead(OP_GET_IRQ_STATUS_6X, buf, 2);
  executeOpcode(OP_CLEAR_IRQ_STATUS_6X, buf, 2);

  if ((buf[1] & IRQ_PAYLOAD_CRC_ERROR_MASK_6X) == 0) {
    _packetIndex = 0;
    uint8_t rxbuf[2] = {0}; // Read packet length
    executeOpcodeRead(OP_RX_BUFFER_STATUS_6X, rxbuf, 2);
    int packetLength = rxbuf[0];
    if (_onReceive) { _onReceive(packetLength); }
  }
}

void ISR_VECT sx126x::onDio0Rise() { sx126x_modem.handleDio0Rise(); }
void sx126x::setSPIFrequency(uint32_t frequency) { _spiSettings = SPISettings(frequency, MSBFIRST, SPI_MODE0); }
void sx126x::enableCrc() { _crcMode = 1; setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode); }
void sx126x::disableCrc() { _crcMode = 0; setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode); }
void sx126x::explicitHeaderMode() { _implicitHeaderMode = 0; setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode); }
void sx126x::implicitHeaderMode() { _implicitHeaderMode = 1; setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode); }
byte sx126x::random() { return readRegister(REG_RANDOM_GEN_6X); }

sx126x sx126x_modem;

#endif