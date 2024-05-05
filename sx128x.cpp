// Copyright (c) Sandeep Mistry. All rights reserved.
// Licensed under the MIT license.

// Modifications and additions copyright 2023 by Mark Qvist
// Obviously still under the MIT license.

#include "sx128x.h"
#include "Boards.h"

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

#define OP_RF_FREQ_8X               0x86
#define OP_SLEEP_8X                 0x84
#define OP_STANDBY_8X               0x80
#define OP_TX_8X                    0x83
#define OP_RX_8X                    0x82
#define OP_SET_IRQ_FLAGS_8X         0x8D // also provides info such as
                                         // preamble detection, etc for
                                         // knowing when it's safe to switch
                                         // antenna modes
#define OP_CLEAR_IRQ_STATUS_8X      0x97
#define OP_GET_IRQ_STATUS_8X        0x15
#define OP_RX_BUFFER_STATUS_8X      0x17
#define OP_PACKET_STATUS_8X         0x1D // get snr & rssi of last packet
#define OP_CURRENT_RSSI_8X          0x1F
#define OP_MODULATION_PARAMS_8X     0x8B // bw, sf, cr, etc.
#define OP_PACKET_PARAMS_8X         0x8C // crc, preamble, payload length, etc.
#define OP_STATUS_8X                0xC0
#define OP_TX_PARAMS_8X             0x8E // set dbm, etc
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

#define REG_PACKET_SIZE            0x901
#define REG_FIRM_VER_MSB           0x154
#define REG_FIRM_VER_LSB           0x153

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
  _ss(LORA_DEFAULT_SS_PIN), _reset(LORA_DEFAULT_RESET_PIN), _dio0(LORA_DEFAULT_DIO0_PIN), _rxen(LORA_DEFAULT_RXEN_PIN), _busy(LORA_DEFAULT_BUSY_PIN),
  _frequency(0),
  _txp(0),
  _sf(0x50),
  _bw(0x34),
  _cr(0x01),
  _packetIndex(0),
  _preambleLength(18),
  _implicitHeaderMode(0),
  _payloadLength(255),
  _crcMode(0),
  _fifo_tx_addr_ptr(0),
  _fifo_rx_addr_ptr(0),
  _packet({0}),
  _rxPacketLength(0),
  _preinit_done(false),
  _onReceive(NULL)
{
  // overide Stream timeout value
  setTimeout(0);
}

bool sx128x::preInit() {
  // setup pins
  pinMode(_ss, OUTPUT);
  // set SS high
  digitalWrite(_ss, HIGH);
  
  SPI.begin();

  // check version (retry for up to 2 seconds)
  long start = millis();

  uint8_t version_msb;
  uint8_t version_lsb;

  while (((millis() - start) < 2000) && (millis() >= start)) {

      version_msb = readRegister(REG_FIRM_VER_MSB);
      version_lsb = readRegister(REG_FIRM_VER_LSB);

      if ((version_msb == 0xB7 && version_lsb == 0xA9) || (version_msb == 0xB5 && version_lsb == 0xA9)) {
          break;
      }
      delay(100);
  }
  if ((version_msb != 0xB7 || version_lsb != 0xA9) && (version_msb != 0xB5 || version_lsb != 0xA9)) {
      return false;
  }

  _preinit_done = true;
  return true;
}

uint8_t ISR_VECT sx128x::readRegister(uint16_t address)
{
  return singleTransfer(OP_READ_REGISTER_8X, address, 0x00);
}

void sx128x::writeRegister(uint16_t address, uint8_t value)
{
    singleTransfer(OP_WRITE_REGISTER_8X, address, value);
}

uint8_t ISR_VECT sx128x::singleTransfer(uint8_t opcode, uint16_t address, uint8_t value)
{
    waitOnBusy();

    uint8_t response;

    digitalWrite(_ss, LOW);

    SPI.beginTransaction(_spiSettings);
    SPI.transfer(opcode);
    SPI.transfer((address & 0xFF00) >> 8);
    SPI.transfer(address & 0x00FF);
    if (opcode == OP_READ_REGISTER_8X) {
        SPI.transfer(0x00);
    }
    response = SPI.transfer(value);
    SPI.endTransaction();

    digitalWrite(_ss, HIGH);

    return response;
}

void sx128x::rxAntEnable()
{
    if (_txen != -1) {
        digitalWrite(_txen, LOW);
    }
    if (_rxen != -1) {
        digitalWrite(_rxen, HIGH);
    }
}

void sx128x::txAntEnable()
{
    if (_txen != -1) {
        digitalWrite(_txen, HIGH);
    }
    if (_rxen != -1) {
        digitalWrite(_rxen, LOW);
    }
}

void sx128x::loraMode() {
    // enable lora mode on the SX1262 chip
    uint8_t mode = MODE_LONG_RANGE_MODE_8X;
    executeOpcode(OP_PACKET_TYPE_8X, &mode, 1);
}

void sx128x::waitOnBusy() {
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

void sx128x::executeOpcode(uint8_t opcode, uint8_t *buffer, uint8_t size)
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

void sx128x::executeOpcodeRead(uint8_t opcode, uint8_t *buffer, uint8_t size)
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

void sx128x::writeBuffer(const uint8_t* buffer, size_t size)
{
    waitOnBusy();

    digitalWrite(_ss, LOW);

    SPI.beginTransaction(_spiSettings);
    SPI.transfer(OP_FIFO_WRITE_8X);
    SPI.transfer(_fifo_tx_addr_ptr);

    for (int i = 0; i < size; i++)
    {
        SPI.transfer(buffer[i]);
        _fifo_tx_addr_ptr++;
    }

    SPI.endTransaction();

    digitalWrite(_ss, HIGH);
}

void sx128x::readBuffer(uint8_t* buffer, size_t size)
{
    waitOnBusy();

    digitalWrite(_ss, LOW);

    SPI.beginTransaction(_spiSettings);
    SPI.transfer(OP_FIFO_READ_8X);
    SPI.transfer(_fifo_rx_addr_ptr);
    SPI.transfer(0x00);

    for (int i = 0; i < size; i++)
    {
        buffer[i] = SPI.transfer(0x00);
    }

    SPI.endTransaction();

    digitalWrite(_ss, HIGH);
}

void sx128x::setModulationParams(uint8_t sf, uint8_t bw, uint8_t cr) {
  // because there is no access to these registers on the sx1280, we have
  // to set all these parameters at once or not at all.
  uint8_t buf[3];

  buf[0] = sf;
  buf[1] = bw;
  buf[2] = cr; 
  executeOpcode(OP_MODULATION_PARAMS_8X, buf, 3);

  if (sf <= 6) {
      writeRegister(0x925, 0x1E);
  } else if (sf <= 8) {
      writeRegister(0x925, 0x37);
  } else if (sf >= 9) {
      writeRegister(0x925, 0x32);
  }
  writeRegister(0x093C, 0x1);
}

void sx128x::setPacketParams(uint32_t preamble, uint8_t headermode, uint8_t length, uint8_t crc) {
  // because there is no access to these registers on the sx1280, we have
  // to set all these parameters at once or not at all.
  uint8_t buf[7];

  // calculate exponent and mantissa values for modem
  uint8_t e = 1;
  uint8_t m = 1;
  uint32_t preamblelen;

  for (e <= 15; e++;) {
      for (m <= 15; m++;) {
          preamblelen = m * (uint32_t(1) << e);
          if (preamblelen >= preamble) break;
      }
      if (preamblelen >= preamble) break;
  }

  buf[0] = (e << 4) | m;
  buf[1] = headermode;
  buf[2] = length;
  buf[3] = crc;
  // standard IQ setting (no inversion)
  buf[4] = 0x40; 
  // unused params
  buf[5] = 0x00; 
  buf[6] = 0x00; 

  executeOpcode(OP_PACKET_PARAMS_8X, buf, 7);
}

int sx128x::begin(unsigned long frequency)
{
  if (_reset != -1) {
    pinMode(_reset, OUTPUT);

    // perform reset
    digitalWrite(_reset, LOW);
    delay(10);
    digitalWrite(_reset, HIGH);
    delay(10);
  }

  if (_rxen != -1) {
      pinMode(_rxen, OUTPUT);
  }

  if (_txen != -1) {
      pinMode(_txen, OUTPUT);
  }

  if (_busy != -1) {
      pinMode(_busy, INPUT);
  }

  if (!_preinit_done) {
    if (!preInit()) {
      return false;
    }
  }

  idle();
  loraMode();
  rxAntEnable();

  setFrequency(frequency);

  // set LNA boost
  // todo: implement this
  //writeRegister(REG_LNA, 0x96);

  setModulationParams(_sf, _bw, _cr);
  setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);

  // set output power to 2 dBm
  setTxPower(2);

  // set base addresses
  uint8_t basebuf[2] = {0};
  executeOpcode(OP_BUFFER_BASE_ADDR_8X, basebuf, 2);

  return 1;
}

void sx128x::end()
{
  // put in sleep mode
  sleep();

  // stop SPI
  SPI.end();

  _preinit_done = false;
}

int sx128x::beginPacket(int implicitHeader)
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

int sx128x::endPacket()
{
  setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);

  txAntEnable();

  // put in single TX mode
  uint8_t timeout[3] = {0};
  executeOpcode(OP_TX_8X, timeout, 3);

  uint8_t buf[2];

  buf[0] = 0x00;
  buf[1] = 0x00;

  executeOpcodeRead(OP_GET_IRQ_STATUS_8X, buf, 2);

  // wait for TX done
  while ((buf[1] & IRQ_TX_DONE_MASK_8X) == 0) {
    buf[0] = 0x00;
    buf[1] = 0x00;
    executeOpcodeRead(OP_GET_IRQ_STATUS_8X, buf, 2);
    yield();
  }

  // clear IRQ's

  uint8_t mask[2];
  mask[0] = 0x00;
  mask[1] = IRQ_TX_DONE_MASK_8X;
  executeOpcode(OP_CLEAR_IRQ_STATUS_8X, mask, 2);
  return 1;
}

uint8_t sx128x::modemStatus() {
    // imitate the register status from the sx1276 / 78
    uint8_t buf[2] = {0};

    executeOpcodeRead(OP_GET_IRQ_STATUS_8X, buf, 2);

    uint8_t clearbuf[2] = {0};

    uint8_t byte = 0x00;

    if ((buf[0] & IRQ_PREAMBLE_DET_MASK_8X) != 0) {
        byte = byte | 0x01 | 0x04;
        // clear register after reading
        clearbuf[0] = 0xFF;
    }

    if ((buf[1] & IRQ_HEADER_DET_MASK_8X) != 0) {
        byte = byte | 0x02 | 0x04;
        // clear register after reading
        clearbuf[1] = 0xFF;
    }

    executeOpcode(OP_CLEAR_IRQ_STATUS_8X, clearbuf, 2);

    return byte;
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

int ISR_VECT sx128x::packetRssi() {
    // may need more calculations here
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
    executeOpcodeRead(OP_PACKET_STATUS_8X, buf, 3);
    return float(buf[1]) * 0.25;
}

long sx128x::packetFrequencyError()
{
  int32_t freqError = 0;
  // todo: implement this, page 120 of sx1280 datasheet
  const float fError = 0.0;
  return static_cast<long>(fError);
}

size_t sx128x::write(uint8_t byte)
{
  return write(&byte, sizeof(byte));
}

size_t sx128x::write(const uint8_t *buffer, size_t size)
{
  if ((_payloadLength + size) > MAX_PKT_LENGTH) {
      size = MAX_PKT_LENGTH - _payloadLength;
  }

  // write data
  writeBuffer(buffer, size);
  _payloadLength = _payloadLength + size;
  return size;
}

int ISR_VECT sx128x::available()
{
    return _rxPacketLength - _packetIndex;
}

int ISR_VECT sx128x::read()
{
  if (!available()) {
    return -1;
  }

  uint8_t byte = _packet[_packetIndex];
  _packetIndex++;
  return byte;
}

int sx128x::peek()
{
  if (!available()) {
    return -1;
  }

  uint8_t b = _packet[_packetIndex];
  return b;
}

void sx128x::flush()
{
}

void sx128x::onReceive(void(*callback)(int))
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
      buf[3] = IRQ_RX_DONE_MASK_8X; 

      // set dio1 masks
      buf[4] = 0x00; 
      buf[5] = 0x00;

      // set dio2 masks
      buf[6] = 0x00; 
      buf[7] = 0x00;

      executeOpcode(OP_SET_IRQ_FLAGS_8X, buf, 8);
//#ifdef SPI_HAS_NOTUSINGINTERRUPT
//    SPI.usingInterrupt(digitalPinToInterrupt(_dio0));
//#endif
    attachInterrupt(digitalPinToInterrupt(_dio0), sx128x::onDio0Rise, RISING);
  } else {
    detachInterrupt(digitalPinToInterrupt(_dio0));
//#ifdef SPI_HAS_NOTUSINGINTERRUPT
//    SPI.notUsingInterrupt(digitalPinToInterrupt(_dio0));
//#endif
  }
}

void sx128x::receive(int size)
{
  if (size > 0) {
    implicitHeaderMode();

    // tell radio payload length
    _rxPacketLength = size;
    //_payloadLength = size;
    //setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
  } else {
    explicitHeaderMode();
  }

  rxAntEnable();

    uint8_t mode[3] = {0xFF, 0xFF, 0xFF}; // continuous mode
    executeOpcode(OP_RX_8X, mode, 3);
}

void sx128x::idle()
{
      #if HAS_TCXO
          // STDBY_XOSC
          uint8_t byte = 0x01;
      #else
          // STDBY_RC
          uint8_t byte = 0x00;
      #endif
      executeOpcode(OP_STANDBY_8X, &byte, 1); 
}

void sx128x::sleep()
{
    uint8_t byte = 0x00;
    executeOpcode(OP_SLEEP_8X, &byte, 1);
}

void sx128x::enableTCXO() {
    // todo: need to check how to implement on sx1280
}

void sx128x::disableTCXO() {
    // todo: need to check how to implement on sx1280
}

void sx128x::setTxPower(int level, int outputPin) {
    if (level > 13) {
        level = 13;
    } else if (level < -18) {
        level = -18;
    }

    _txp = level;

    level = level + 18;

    uint8_t tx_buf[2];

    tx_buf[0] = level;
    tx_buf[1] = 0xE0; // ramping time - 20 microseconds

    executeOpcode(OP_TX_PARAMS_8X, tx_buf, 2);
}

uint8_t sx128x::getTxPower() {
      return _txp;
}

void sx128x::setFrequency(unsigned long frequency) {
  _frequency = frequency;

  uint8_t buf[3];

  uint32_t freq = (uint32_t)((double)frequency / (double)FREQ_STEP_8X);

  buf[0] = ((freq >> 16) & 0xFF);
  buf[1] = ((freq >> 8) & 0xFF);
  buf[2] = (freq & 0xFF);

  executeOpcode(OP_RF_FREQ_8X, buf, 3);
}

uint32_t sx128x::getFrequency() {
  // we can't read the frequency on the sx1280
  uint32_t frequency = _frequency;

  return frequency;
}

void sx128x::setSpreadingFactor(int sf)
{
  if (sf < 5) {
      sf = 5;
  } else if (sf > 12) {
    sf = 12;
  }

  _sf = sf << 4;

  setModulationParams(sf << 4, _bw, _cr);
  handleLowDataRate();
}

long sx128x::getSignalBandwidth()
{
  int bw = _bw;
  switch (bw) {
      case 0x34: return 203.125E3;
      case 0x26: return 406.25E3;
      case 0x18: return 812.5E3;
      case 0x0A: return 1625E3;
  }
  
  return 0;
}

void sx128x::handleLowDataRate(){
    // todo: do i need this??
}

void sx128x::optimizeModemSensitivity(){
    // todo: check if there's anything the sx1280 can do here
}

void sx128x::setSignalBandwidth(long sbw)
{
      if (sbw <= 203.125E3) {
          _bw = 0x34;
      } else if (sbw <= 406.25E3) {
          _bw = 0x26;
      } else if (sbw <= 812.5E3) {
          _bw = 0x18;
      } else {
          _bw = 0x0A;
      }

      setModulationParams(_sf, _bw, _cr);

  handleLowDataRate();
  optimizeModemSensitivity();
}

void sx128x::setCodingRate4(int denominator)
{
  if (denominator < 5) {
    denominator = 5;
  } else if (denominator > 8) {
    denominator = 8;
  }

  _cr = denominator - 4;

  // todo: add support for new interleaving scheme, see page 117 of sx1280
  // datasheet

  // update cr values for sx1280's use

  setModulationParams(_sf, _bw, _cr);
}

void sx128x::setPreambleLength(long length)
{
  _preambleLength = length;
  setPacketParams(length, _implicitHeaderMode, _payloadLength, _crcMode);
}

void sx128x::setSyncWord(int sw)
{
    // not implemented
}

void sx128x::enableCrc()
{
      _crcMode = 0x20;
      setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
}

void sx128x::disableCrc()
{
    _crcMode = 0;
    setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
}

byte sx128x::random()
{
    // todo: implement
}

void sx128x::setPins(int ss, int reset, int dio0, int busy, int rxen, int txen)
{
  _ss = ss;
  _reset = reset;
  _dio0 = dio0;
  _busy = busy;
  _rxen = rxen;
  _txen = txen;
}

void sx128x::setSPIFrequency(uint32_t frequency)
{
  _spiSettings = SPISettings(frequency, MSBFIRST, SPI_MODE0);
}

void sx128x::dumpRegisters(Stream& out)
{
  for (int i = 0; i < 128; i++) {
    out.print("0x");
    out.print(i, HEX);
    out.print(": 0x");
    out.println(readRegister(i), HEX);
  }
}

void sx128x::explicitHeaderMode()
{
  _implicitHeaderMode = 0;

  setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
}

void sx128x::implicitHeaderMode()
{
    _implicitHeaderMode = 0x80;
    setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
}


void ISR_VECT sx128x::handleDio0Rise()
{
    uint8_t buf[2];

    buf[0] = 0x00;
    buf[1] = 0x00;

    executeOpcodeRead(OP_GET_IRQ_STATUS_8X, buf, 2);

    executeOpcode(OP_CLEAR_IRQ_STATUS_8X, buf, 2);

    if ((buf[1] & IRQ_PAYLOAD_CRC_ERROR_MASK_8X) == 0) {
        // received a packet
        _packetIndex = 0;

        uint8_t rxbuf[2] = {0};
        executeOpcodeRead(OP_RX_BUFFER_STATUS_8X, rxbuf, 2);
        _rxPacketLength = rxbuf[0];
        _fifo_rx_addr_ptr = rxbuf[1];
        readBuffer(_packet, _rxPacketLength);

        if (_onReceive) {
            _onReceive(_rxPacketLength);
        }

    }
}

void ISR_VECT sx128x::onDio0Rise()
{
  sx128x_modem.handleDio0Rise();
}

sx128x sx128x_modem;
