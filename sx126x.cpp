// Copyright (c) Sandeep Mistry. All rights reserved.
// Licensed under the MIT license.

// Modifications and additions copyright 2023 by Mark Qvist
// Obviously still under the MIT license.

#include "Boards.h"

#if MODEM == SX1262
#include "sx126x.h"

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
#define OP_SET_IRQ_FLAGS_6X         0x08 // also provides info such as
                                      // preamble detection, etc for
                                      // knowing when it's safe to switch
                                      // antenna modes
#define OP_CLEAR_IRQ_STATUS_6X      0x02
#define OP_GET_IRQ_STATUS_6X        0x12
#define OP_RX_BUFFER_STATUS_6X      0x13
#define OP_PACKET_STATUS_6X         0x14 // get snr & rssi of last packet
#define OP_CURRENT_RSSI_6X          0x15
#define OP_MODULATION_PARAMS_6X     0x8B // bw, sf, cr, etc.
#define OP_PACKET_PARAMS_6X         0x8C // crc, preamble, payload length, etc.
#define OP_STATUS_6X                0xC0
#define OP_TX_PARAMS_6X             0x8E // set dbm, etc
#define OP_PACKET_TYPE_6X           0x8A
#define OP_BUFFER_BASE_ADDR_6X      0x8F
#define OP_READ_REGISTER_6X         0x1D
#define OP_WRITE_REGISTER_6X        0x0D
#define OP_DIO3_TCXO_CTRL_6X        0x97
#define OP_DIO2_RF_CTRL_6X          0x9D
#define OP_CALIBRATE_6X             0x89
#define IRQ_TX_DONE_MASK_6X         0x01
#define IRQ_RX_DONE_MASK_6X         0x02
#define IRQ_HEADER_DET_MASK_6X      0x10
#define IRQ_PREAMBLE_DET_MASK_6X    0x04
#define IRQ_PAYLOAD_CRC_ERROR_MASK_6X 0x40

#define MODE_LONG_RANGE_MODE_6X     0x01

#define OP_FIFO_WRITE_6X            0x0E
#define OP_FIFO_READ_6X             0x1E
#define REG_OCP_6X                0x08E7
#define REG_LNA_6X                0x08AC // no agc in sx1262
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

#define SYNC_WORD_6X              0x1424

#define XTAL_FREQ_6X (double)32000000
#define FREQ_DIV_6X (double)pow(2.0, 25.0)
#define FREQ_STEP_6X (double)(XTAL_FREQ_6X / FREQ_DIV_6X)

#if defined(NRF52840_XXAA)
  extern SPIClass spiModem;
  #define SPI spiModem
#endif

extern SPIClass SPI;

#define MAX_PKT_LENGTH           255

sx126x::sx126x() :
  _spiSettings(8E6, MSBFIRST, SPI_MODE0),
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
{
  // overide Stream timeout value
  setTimeout(0);
}

bool sx126x::preInit() {
  // setup pins
  pinMode(_ss, OUTPUT);
  // set SS high
  digitalWrite(_ss, HIGH);
  
  #if BOARD_MODEL == BOARD_RNODE_NG_22
    SPI.begin(pin_sclk, pin_miso, pin_mosi, pin_cs);
  #else
    SPI.begin();
  #endif

  // check version (retry for up to 2 seconds)
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

uint8_t ISR_VECT sx126x::readRegister(uint16_t address)
{
  return singleTransfer(OP_READ_REGISTER_6X, address, 0x00);
}

void sx126x::writeRegister(uint16_t address, uint8_t value)
{
    singleTransfer(OP_WRITE_REGISTER_6X, address, value);
}

uint8_t ISR_VECT sx126x::singleTransfer(uint8_t opcode, uint16_t address, uint8_t value)
{
    waitOnBusy();

    uint8_t response;

    digitalWrite(_ss, LOW);

    SPI.beginTransaction(_spiSettings);
    SPI.transfer(opcode);
    SPI.transfer((address & 0xFF00) >> 8);
    SPI.transfer(address & 0x00FF);
    if (opcode == OP_READ_REGISTER_6X) {
        SPI.transfer(0x00);
    }
    response = SPI.transfer(value);
    SPI.endTransaction();

    digitalWrite(_ss, HIGH);

    return response;
}

void sx126x::rxAntEnable()
{
  if (_rxen != -1) {
    digitalWrite(_rxen, HIGH);
  }
}

void sx126x::loraMode() {
    // enable lora mode on the SX1262 chip
    uint8_t mode = MODE_LONG_RANGE_MODE_6X;
    executeOpcode(OP_PACKET_TYPE_6X, &mode, 1);
}

void sx126x::waitOnBusy() {
    unsigned long time = millis();
    if (_busy != -1) {
        while (digitalRead(_busy) == HIGH)
        {
            if (millis() >= (time + 100)) {
                break;
            }
            // do nothing
        }
    }
}

void sx126x::executeOpcode(uint8_t opcode, uint8_t *buffer, uint8_t size)
{
    waitOnBusy();

    digitalWrite(_ss, LOW);

    SPI.beginTransaction(_spiSettings);
    SPI.transfer(opcode);

    for (int i = 0; i < size; i++)
    {
        SPI.transfer(buffer[i]);
    }

    SPI.endTransaction();

    digitalWrite(_ss, HIGH);
}

void sx126x::executeOpcodeRead(uint8_t opcode, uint8_t *buffer, uint8_t size)
{
    waitOnBusy();

    digitalWrite(_ss, LOW);

    SPI.beginTransaction(_spiSettings);
    SPI.transfer(opcode);
    SPI.transfer(0x00);

    for (int i = 0; i < size; i++)
    {
        buffer[i] = SPI.transfer(0x00);
    }

    SPI.endTransaction();

    digitalWrite(_ss, HIGH);
}

void sx126x::writeBuffer(const uint8_t* buffer, size_t size)
{
    waitOnBusy();

    digitalWrite(_ss, LOW);

    SPI.beginTransaction(_spiSettings);
    SPI.transfer(OP_FIFO_WRITE_6X);
    SPI.transfer(_fifo_tx_addr_ptr);

    for (int i = 0; i < size; i++)
    {
        SPI.transfer(buffer[i]);
        _fifo_tx_addr_ptr++;
    }

    SPI.endTransaction();

    digitalWrite(_ss, HIGH);
}

void sx126x::readBuffer(uint8_t* buffer, size_t size)
{
    waitOnBusy();

    digitalWrite(_ss, LOW);

    SPI.beginTransaction(_spiSettings);
    SPI.transfer(OP_FIFO_READ_6X);
    SPI.transfer(_fifo_rx_addr_ptr);
    SPI.transfer(0x00);

    for (int i = 0; i < size; i++)
    {
        buffer[i] = SPI.transfer(0x00);
    }

    SPI.endTransaction();

    digitalWrite(_ss, HIGH);
}

void sx126x::setModulationParams(uint8_t sf, uint8_t bw, uint8_t cr, int ldro) {
  // because there is no access to these registers on the sx1262, we have
  // to set all these parameters at once or not at all.
  uint8_t buf[8];

  buf[0] = sf;
  buf[1] = bw;
  buf[2] = cr; 
  // low data rate toggle
  buf[3] = ldro;
  // unused params in LoRa mode
  buf[4] = 0x00; 
  buf[5] = 0x00;
  buf[6] = 0x00;
  buf[7] = 0x00;

  executeOpcode(OP_MODULATION_PARAMS_6X, buf, 8);
}

void sx126x::setPacketParams(long preamble, uint8_t headermode, uint8_t length, uint8_t crc) {
  // because there is no access to these registers on the sx1262, we have
  // to set all these parameters at once or not at all.
  uint8_t buf[9];

  buf[0] = uint8_t((preamble & 0xFF00) >> 8);
  buf[1] = uint8_t((preamble & 0x00FF));
  buf[2] = headermode;
  buf[3] = length;
  buf[4] = crc;
  // standard IQ setting (no inversion)
  buf[5] = 0x00; 
  // unused params
  buf[6] = 0x00; 
  buf[7] = 0x00; 
  buf[8] = 0x00; 

  executeOpcode(OP_PACKET_PARAMS_6X, buf, 9);
}


int sx126x::begin(long frequency)
{
  if (_reset != -1) {
    pinMode(_reset, OUTPUT);

    // perform reset
    digitalWrite(_reset, LOW);
    delay(10);
    digitalWrite(_reset, HIGH);
    delay(10);
  }

  if (_busy != -1) {
      pinMode(_busy, INPUT);
  }

  if (!_preinit_done) {
    if (!preInit()) {
      return false;
    }
  }

  loraMode();
  // cannot access registers in sleep mode on sx1262, set to idle instead
  idle();

  #if HAS_TCXO
    enableTCXO();
  #endif

  if (_rxen != -1) {
      pinMode(_rxen, OUTPUT);
  }

  #if DIO2_AS_RF_SWITCH
    // enable dio2 rf switch
    uint8_t byte = 0x01;
    executeOpcode(OP_DIO2_RF_CTRL_6X, &byte, 1);
  #endif

  rxAntEnable();

  // Set sync word
  setSyncWord(SYNC_WORD_6X);

  // calibrate RC64k, RC13M, PLL, ADC and image
  uint8_t calibrate = 0x7F; 
  executeOpcode(OP_CALIBRATE_6X, &calibrate, 1);

  setFrequency(frequency);

  // set output power to 2 dBm
  setTxPower(2);
  enableCrc();

  // set LNA boost
  writeRegister(REG_LNA_6X, 0x96);

  // set base addresses
  uint8_t basebuf[2] = {0};
  executeOpcode(OP_BUFFER_BASE_ADDR_6X, basebuf, 2);

  setModulationParams(_sf, _bw, _cr, _ldro);
  setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);

  return 1;
}

void sx126x::end()
{
  // put in sleep mode
  sleep();

  // stop SPI
  SPI.end();

  _preinit_done = false;
}

int sx126x::beginPacket(int implicitHeader)
{
  // put in standby mode
  idle();

  if (implicitHeader) {
    implicitHeaderMode();
  } else {
    explicitHeaderMode();
  }

  _payloadLength = 0;
  _fifo_tx_addr_ptr = 0;
  setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);

  return 1;
}

int sx126x::endPacket()
{
      setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);

      // put in single TX mode
      uint8_t timeout[3] = {0};
      executeOpcode(OP_TX_6X, timeout, 3);

      uint8_t buf[2];

      buf[0] = 0x00;
      buf[1] = 0x00;

      executeOpcodeRead(OP_GET_IRQ_STATUS_6X, buf, 2);

      // wait for TX done
      while ((buf[1] & IRQ_TX_DONE_MASK_6X) == 0) {
        buf[0] = 0x00;
        buf[1] = 0x00;
        executeOpcodeRead(OP_GET_IRQ_STATUS_6X, buf, 2);
        yield();
      }

      // clear IRQ's

      uint8_t mask[2];
      mask[0] = 0x00;
      mask[1] = IRQ_TX_DONE_MASK_6X;
      executeOpcode(OP_CLEAR_IRQ_STATUS_6X, mask, 2);
  return 1;
}

uint8_t sx126x::modemStatus() {
    // imitate the register status from the sx1276 / 78
    uint8_t buf[2] = {0};

    executeOpcodeRead(OP_GET_IRQ_STATUS_6X, buf, 2);
    uint8_t clearbuf[2] = {0};
    uint8_t byte = 0x00;

    if ((buf[1] & IRQ_PREAMBLE_DET_MASK_6X) != 0) {
      byte = byte | 0x01 | 0x04;
      // clear register after reading
      clearbuf[1] = IRQ_PREAMBLE_DET_MASK_6X;
    }

    if ((buf[1] & IRQ_HEADER_DET_MASK_6X) != 0) {
      byte = byte | 0x02 | 0x04;
    }

    executeOpcode(OP_CLEAR_IRQ_STATUS_6X, clearbuf, 2);

    return byte; 
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
    // may need more calculations here
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

long sx126x::packetFrequencyError()
{
    // todo: implement this, no idea how to check it on the sx1262
    const float fError = 0.0;
    return static_cast<long>(fError);
}

size_t sx126x::write(uint8_t byte)
{
  return write(&byte, sizeof(byte));
}

size_t sx126x::write(const uint8_t *buffer, size_t size)
{
    if ((_payloadLength + size) > MAX_PKT_LENGTH) {
        size = MAX_PKT_LENGTH - _payloadLength;
    }

    // write data
    writeBuffer(buffer, size);
    _payloadLength = _payloadLength + size;
    return size;
}

int ISR_VECT sx126x::available()
{
    uint8_t buf[2] = {0};
    executeOpcodeRead(OP_RX_BUFFER_STATUS_6X, buf, 2);
    return buf[0] - _packetIndex;
}

int ISR_VECT sx126x::read()
{
  if (!available()) {
    return -1;
  }

  // if received new packet
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

int sx126x::peek()
{
  if (!available()) {
    return -1;
  }

  // if received new packet
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

void sx126x::flush()
{
}

void sx126x::onReceive(void(*callback)(int))
{
  _onReceive = callback;

  if (callback) {
    pinMode(_dio0, INPUT);

    // set preamble and header detection irqs, plus dio0 mask
    uint8_t buf[8];

    // set irq masks, enable all
    buf[0] = 0xFF; 
    buf[1] = 0xFF;

    // set dio0 masks
    buf[2] = 0x00;
    buf[3] = IRQ_RX_DONE_MASK_6X; 

    // set dio1 masks
    buf[4] = 0x00; 
    buf[5] = 0x00;

    // set dio2 masks
    buf[6] = 0x00; 
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

void sx126x::receive(int size)
{
    if (size > 0) {
        implicitHeaderMode();

        // tell radio payload length
        _payloadLength = size;
        setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
    } else {
        explicitHeaderMode();
    }

    if (_rxen != -1) {
        rxAntEnable();
    }

    uint8_t mode[3] = {0xFF, 0xFF, 0xFF}; // continuous mode
    executeOpcode(OP_RX_6X, mode, 3);
}

void sx126x::idle()
{
  // STDBY_XOSC
  uint8_t byte = 0x01;
  // STDBY_RC
  // uint8_t byte = 0x00;
  executeOpcode(OP_STANDBY_6X, &byte, 1); 
}

void sx126x::sleep()
{
    uint8_t byte = 0x00;
    executeOpcode(OP_SLEEP_6X, &byte, 1);
}

void sx126x::enableTCXO() {
    // only tested for RAK4630, voltage may be different on other platforms
    #if BOARD_MODEL == BOARD_RAK4630
      uint8_t buf[4] = {MODE_TCXO_3_3V_6X, 0x00, 0x00, 0xFF};
    #elif BOARD_MODEL == BOARD_TBEAM
      uint8_t buf[4] = {MODE_TCXO_1_8V_6X, 0x00, 0x00, 0xFF};
    #endif

    executeOpcode(OP_DIO3_TCXO_CTRL_6X, buf, 4);
}

void sx126x::disableTCXO() {
    // currently cannot disable on SX1262?
}

void sx126x::setTxPower(int level, int outputPin) {
    // currently no low power mode for SX1262 implemented, assuming PA boost
    
    // WORKAROUND - Better Resistance of the SX1262 Tx to Antenna Mismatch, see DS_SX1261-2_V1.2 datasheet chapter 15.2
    // RegTxClampConfig = @address 0x08D8
    writeRegister(0x08D8, readRegister(0x08D8) | (0x0F << 1));

    uint8_t pa_buf[4];

    pa_buf[0] = 0x04;
    pa_buf[1] = 0x07;
    pa_buf[2] = 0x00;
    pa_buf[3] = 0x01;

    executeOpcode(OP_PA_CONFIG_6X, pa_buf, 4); // set pa_config for high power

    if (level > 22) {
        level = 22;
    }
    else if (level < -9) {
        level = -9;
    }

    writeRegister(REG_OCP_6X, 0x38); // 160mA limit, overcurrent protection

    uint8_t tx_buf[2];

    tx_buf[0] = level;
    tx_buf[1] = 0x02; // ramping time - 40 microseconds
    
    executeOpcode(OP_TX_PARAMS_6X, tx_buf, 2);

    _txp = level;
}

uint8_t sx126x::getTxPower() {
    return _txp;
}

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
    // we can't read the frequency on the sx1262 / 80
    uint32_t frequency = _frequency;

    return frequency;
}

void sx126x::setSpreadingFactor(int sf)
{
  if (sf < 5) {
      sf = 5;
  } else if (sf > 12) {
    sf = 12;
  }

  _sf = sf;

  setModulationParams(sf, _bw, _cr, _ldro);
  handleLowDataRate();
}

long sx126x::getSignalBandwidth()
{
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

void sx126x::handleLowDataRate(){
    _ldro = 1;
    setModulationParams(_sf, _bw, _cr, _ldro);
}

void sx126x::optimizeModemSensitivity(){
    // todo: check if there's anything the sx1262 can do here
}

void sx126x::setSignalBandwidth(long sbw)
{
    if (sbw <= 7.8E3) {
        _bw = 0x00;
    } else if (sbw <= 10.4E3) {
        _bw = 0x08;
    } else if (sbw <= 15.6E3) {
        _bw = 0x01;
    } else if (sbw <= 20.8E3) {
        _bw = 0x09;
    } else if (sbw <= 31.25E3) {
        _bw = 0x02;
    } else if (sbw <= 41.7E3) {
        _bw = 0x0A;
    } else if (sbw <= 62.5E3) {
        _bw = 0x03;
    } else if (sbw <= 125E3) {
        _bw = 0x04;
    } else if (sbw <= 250E3) {
        _bw = 0x05;
    } else /*if (sbw <= 250E3)*/ {
        _bw = 0x06;
    }

    setModulationParams(_sf, _bw, _cr, _ldro);

    handleLowDataRate();
  optimizeModemSensitivity();
}

void sx126x::setCodingRate4(int denominator)
{
  if (denominator < 5) {
    denominator = 5;
  } else if (denominator > 8) {
    denominator = 8;
  }

  int cr = denominator - 4;

  _cr = cr;

  setModulationParams(_sf, _bw, cr, _ldro);
}

void sx126x::setPreambleLength(long length)
{
  _preambleLength = length;
  setPacketParams(length, _implicitHeaderMode, _payloadLength, _crcMode);
}

void sx126x::setSyncWord(uint16_t sw)
{
  // TODO: Fix
    // writeRegister(REG_SYNC_WORD_MSB_6X, (sw & 0xFF00) >> 8);
    // writeRegister(REG_SYNC_WORD_LSB_6X, sw & 0x00FF);
    writeRegister(REG_SYNC_WORD_MSB_6X, 0x14);
    writeRegister(REG_SYNC_WORD_LSB_6X, 0x24);
}

void sx126x::enableCrc()
{
    _crcMode = 1;
    setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
}

void sx126x::disableCrc()
{
    _crcMode = 0;
    setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
}

byte sx126x::random()
{
    return readRegister(REG_RANDOM_GEN_6X);
}

void sx126x::setPins(int ss, int reset, int dio0, int busy, int rxen)
{
  _ss = ss;
  _reset = reset;
  _dio0 = dio0;
  _busy = busy;
  _rxen = rxen;
}

void sx126x::setSPIFrequency(uint32_t frequency)
{
  _spiSettings = SPISettings(frequency, MSBFIRST, SPI_MODE0);
}

void sx126x::dumpRegisters(Stream& out)
{
  for (int i = 0; i < 128; i++) {
    out.print("0x");
    out.print(i, HEX);
    out.print(": 0x");
    out.println(readRegister(i), HEX);
  }
}

void sx126x::explicitHeaderMode()
{
  _implicitHeaderMode = 0;
  setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
}

void sx126x::implicitHeaderMode()
{
  _implicitHeaderMode = 1;
  setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
}


void ISR_VECT sx126x::handleDio0Rise()
{
    uint8_t buf[2];

    buf[0] = 0x00;
    buf[1] = 0x00;

    executeOpcodeRead(OP_GET_IRQ_STATUS_6X, buf, 2);

    executeOpcode(OP_CLEAR_IRQ_STATUS_6X, buf, 2);

    if ((buf[1] & IRQ_PAYLOAD_CRC_ERROR_MASK_6X) == 0) {
        // received a packet
        _packetIndex = 0;

        // read packet length
        uint8_t rxbuf[2] = {0};
        executeOpcodeRead(OP_RX_BUFFER_STATUS_6X, rxbuf, 2);
        int packetLength = rxbuf[0];

        if (_onReceive) {
            _onReceive(packetLength);
        }
    }
    // else {
    //   Serial.println("CRCE");
    //   Serial.println(buf[0]);
    //   Serial.println(buf[1]);
    // }
}

void ISR_VECT sx126x::onDio0Rise()
{
    sx126x_modem.handleDio0Rise();
}

sx126x sx126x_modem;

#endif