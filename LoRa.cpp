// Copyright (c) Sandeep Mistry. All rights reserved.
// Licensed under the MIT license.

// Modifications and additions copyright 2023 by Mark Qvist
// Obviously still under the MIT license.

#include "Boards.h"
#include "LoRa.h"
#include "Modem.h"

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
  #if BOARD_MODEL != BOARD_RNODE_NG_22
    #include "soc/rtc_wdt.h"
  #endif
  #define ISR_VECT IRAM_ATTR
#else
  #define ISR_VECT
#endif

#if BOARD_MODEL == BOARD_RNODE_NG_22
  #define MODEM_MISO 3
  #define MODEM_MOSI 6
  #define MODEM_CLK  5
#endif

#if MODEM == SX1262
    #define OP_FIFO_WRITE            0x0E
    #define OP_FIFO_READ             0x1E
    #define OP_RF_FREQ               0x86
    #define OP_SLEEP                 0x84
    #define OP_STANDBY               0x80
    #define OP_TX                    0x83
    #define OP_RX                    0x82
    #define OP_PA_CONFIG             0x95
    #define OP_SET_IRQ_FLAGS         0x08 // also provides info such as
                                          // preamble detection, etc for
                                          // knowing when it's safe to switch
                                          // antenna modes
    #define OP_CLEAR_IRQ_STATUS      0x02
    #define OP_GET_IRQ_STATUS        0x12
    #define OP_RX_BUFFER_STATUS      0x13
    #define OP_PACKET_STATUS         0x14 // get snr & rssi of last packet
    #define OP_CURRENT_RSSI          0x15
    #define OP_MODULATION_PARAMS     0x8B // bw, sf, cr, etc.
    #define OP_PACKET_PARAMS         0x8C // crc, preamble, payload length, etc.
    #define OP_STATUS                0xC0
    #define OP_TX_PARAMS             0x8E // set dbm, etc
    #define OP_PACKET_TYPE           0x8A
    #define OP_BUFFER_BASE_ADDR      0x8F
    #define OP_READ_REGISTER         0x1D
    #define OP_WRITE_REGISTER        0x0D
    #define OP_DIO3_TCXO_CTRL        0x97
    #define OP_DIO2_RF_CTRL          0x9D
    #define OP_CALIBRATE             0x89

    #define REG_OCP                0x08E7
    #define REG_LNA                0x08AC // no agc in sx1262
    #define REG_SYNC_WORD_MSB      0x0740
    #define REG_SYNC_WORD_LSB      0x0741
    #define REG_PAYLOAD_LENGTH     0x0702 // https://github.com/beegee-tokyo/SX126x-Arduino/blob/master/src/radio/sx126x/sx126x.h#L98
    #define REG_RANDOM_GEN         0x0819

    #define MODE_LONG_RANGE_MODE     0x01

    #define MODE_TCXO_3_3V           0x07

    #define IRQ_TX_DONE_MASK         0x01
    #define IRQ_RX_DONE_MASK         0x02
    #define IRQ_PREAMBLE_DET_MASK    0x04
    #define IRQ_HEADER_DET_MASK      0x10
    #define IRQ_PAYLOAD_CRC_ERROR_MASK 0x40

    #define XTAL_FREQ (double)32000000
    #define FREQ_DIV (double)pow(2.0, 25.0)
    #define FREQ_STEP (double)(XTAL_FREQ / FREQ_DIV)
    int fifo_tx_addr_ptr = 0;
    int fifo_rx_addr_ptr = 0;
    uint8_t packet[256] = {0};

    #if defined(NRF52840_XXAA)
      extern SPIClass spiModem;
      #define SPI spiModem
    #endif

#elif MODEM == SX1276 || MODEM == SX1278
    // Registers
    #define REG_FIFO                 0x00
    #define REG_OP_MODE              0x01
    #define REG_FRF_MSB              0x06
    #define REG_FRF_MID              0x07
    #define REG_FRF_LSB              0x08
    #define REG_PA_CONFIG            0x09
    #define REG_OCP                  0x0b
    #define REG_LNA                  0x0c
    #define REG_FIFO_ADDR_PTR        0x0d
    #define REG_FIFO_TX_BASE_ADDR    0x0e
    #define REG_FIFO_RX_BASE_ADDR    0x0f
    #define REG_FIFO_RX_CURRENT_ADDR 0x10
    #define REG_IRQ_FLAGS            0x12
    #define REG_RX_NB_BYTES          0x13
    #define REG_MODEM_STAT           0x18
    #define REG_PKT_SNR_VALUE        0x19
    #define REG_PKT_RSSI_VALUE       0x1a
    #define REG_RSSI_VALUE           0x1b
    #define REG_MODEM_CONFIG_1       0x1d
    #define REG_MODEM_CONFIG_2       0x1e
    #define REG_PREAMBLE_MSB         0x20
    #define REG_PREAMBLE_LSB         0x21
    #define REG_PAYLOAD_LENGTH       0x22
    #define REG_MODEM_CONFIG_3       0x26
    #define REG_FREQ_ERROR_MSB       0x28
    #define REG_FREQ_ERROR_MID       0x29
    #define REG_FREQ_ERROR_LSB       0x2a
    #define REG_RSSI_WIDEBAND        0x2c
    #define REG_DETECTION_OPTIMIZE   0x31
    #define REG_HIGH_BW_OPTIMIZE_1   0x36
    #define REG_DETECTION_THRESHOLD  0x37
    #define REG_SYNC_WORD            0x39
    #define REG_HIGH_BW_OPTIMIZE_2   0x3a
    #define REG_DIO_MAPPING_1        0x40
    #define REG_VERSION              0x42
    #define REG_TCXO                 0x4b
    #define REG_PA_DAC               0x4d
    
    // Modes
    #define MODE_LONG_RANGE_MODE     0x80
    #define MODE_SLEEP               0x00
    #define MODE_STDBY               0x01
    #define MODE_TX                  0x03
    #define MODE_RX_CONTINUOUS       0x05
    #define MODE_RX_SINGLE           0x06
    
    // PA config
    #define PA_BOOST                 0x80
    
    // IRQ masks
    #define IRQ_TX_DONE_MASK           0x08
    #define IRQ_PAYLOAD_CRC_ERROR_MASK 0x20
    #define IRQ_RX_DONE_MASK           0x40
#endif

#define MAX_PKT_LENGTH           255

extern SPIClass SPI;

bool lora_preinit_done = false;

LoRaClass::LoRaClass() :
  _spiSettings(8E6, MSBFIRST, SPI_MODE0),
  _ss(LORA_DEFAULT_SS_PIN), _reset(LORA_DEFAULT_RESET_PIN), _dio0(LORA_DEFAULT_DIO0_PIN), _rxen(LORA_DEFAULT_RXEN_PIN), _busy(LORA_DEFAULT_BUSY_PIN),
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
  _onReceive(NULL)
{
  // overide Stream timeout value
  setTimeout(0);
}

bool LoRaClass::preInit() {
  // setup pins
  pinMode(_ss, OUTPUT);
  // set SS high
  digitalWrite(_ss, HIGH);
  
  #if BOARD_MODEL == BOARD_RNODE_NG_22
    SPI.begin(MODEM_CLK, MODEM_MISO, MODEM_MOSI);
  #else
    SPI.begin();
  #endif

  // check version (retry for up to 2 seconds)
  #if MODEM == SX1276 || MODEM == SX1278
    uint8_t version;
    long start = millis();
    while (((millis() - start) < 2000) && (millis() >= start)) {
      version = readRegister(REG_VERSION);
      if (version == 0x12) {
        break;
      }
      delay(100);
    }
    if (version != 0x12) {
      return false;
    }
  
    lora_preinit_done = true;
    return true;

  #elif MODEM == SX1262
    long start = millis();
    uint8_t syncmsb;
    uint8_t synclsb;
    while (((millis() - start) < 2000) && (millis() >= start)) {
      syncmsb = readRegister(REG_SYNC_WORD_MSB);
      synclsb = readRegister(REG_SYNC_WORD_LSB);
      if ( uint16_t(syncmsb << 8 | synclsb) == 0x1424 || uint16_t(syncmsb << 8 | synclsb) == 0x4434) {
          break;
      }
      delay(100);
    }

    if ( uint16_t(syncmsb << 8 | synclsb) != 0x1424 && uint16_t(syncmsb << 8 | synclsb) != 0x4434) {
        return false;
    }

    lora_preinit_done = true;
    return true;

  #else
    return false;
  #endif
}

#if MODEM == SX1276 || MODEM == SX1278
    uint8_t ISR_VECT LoRaClass::readRegister(uint8_t address)
    {
      return singleTransfer(address & 0x7f, 0x00);
    }
 
    void LoRaClass::writeRegister(uint8_t address, uint8_t value)
    {
      singleTransfer(address | 0x80, value);
    }

    uint8_t ISR_VECT LoRaClass::singleTransfer(uint8_t address, uint8_t value)
    {
      uint8_t response;

      digitalWrite(_ss, LOW);

      SPI.beginTransaction(_spiSettings);
      SPI.transfer(address);
      response = SPI.transfer(value);
      SPI.endTransaction();

      digitalWrite(_ss, HIGH);

      return response;
    }
#elif MODEM == SX1262
    uint8_t ISR_VECT LoRaClass::readRegister(uint16_t address)
    {
      return singleTransfer(OP_READ_REGISTER, address, 0x00);
    }

    void LoRaClass::writeRegister(uint16_t address, uint8_t value)
    {
        singleTransfer(OP_WRITE_REGISTER, address, value);
    }

    uint8_t ISR_VECT LoRaClass::singleTransfer(uint8_t opcode, uint16_t address, uint8_t value)
    {
        waitOnBusy();

        uint8_t response;

        digitalWrite(_ss, LOW);

        SPI.beginTransaction(_spiSettings);
        SPI.transfer(opcode);
        SPI.transfer((address & 0xFF00) >> 8);
        SPI.transfer(address & 0x00FF);
        if (opcode == OP_READ_REGISTER) {
            SPI.transfer(0x00);
        }
        response = SPI.transfer(value);
        SPI.endTransaction();

        digitalWrite(_ss, HIGH);

        return response;
    }

    void LoRaClass::enableAntenna()
    {
        uint8_t byte = 0x01;
        // enable dio2 rf switch
        executeOpcode(OP_DIO2_RF_CTRL, &byte, 1); 
        digitalWrite(_rxen, HIGH);
    }

    void LoRaClass::disableAntenna()
    {
        digitalWrite(_rxen, LOW);
    }

    void LoRaClass::loraMode() {
        // enable lora mode on the SX1262 chip
        uint8_t mode = MODE_LONG_RANGE_MODE;
        executeOpcode(OP_PACKET_TYPE, &mode, 1);
    }

    void LoRaClass::waitOnBusy() {
        if (_busy != -1) {
            while (digitalRead(_busy) == HIGH)
            {
                // do nothing
            }
        }
    }

    void LoRaClass::executeOpcode(uint8_t opcode, uint8_t *buffer, uint8_t size)
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

    void LoRaClass::executeOpcodeRead(uint8_t opcode, uint8_t *buffer, uint8_t size)
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

    void LoRaClass::writeBuffer(const uint8_t* buffer, size_t size)
    {
        waitOnBusy();

        digitalWrite(_ss, LOW);

        SPI.beginTransaction(_spiSettings);
        SPI.transfer(OP_FIFO_WRITE);
        SPI.transfer(fifo_tx_addr_ptr);

        for (int i = 0; i < size; i++)
        {
            SPI.transfer(buffer[i]);
            fifo_tx_addr_ptr++;
        }

        SPI.endTransaction();

        digitalWrite(_ss, HIGH);
    }

    void LoRaClass::readBuffer(uint8_t* buffer, size_t size)
    {
        waitOnBusy();

        digitalWrite(_ss, LOW);

        SPI.beginTransaction(_spiSettings);
        SPI.transfer(OP_FIFO_READ);
        SPI.transfer(fifo_rx_addr_ptr);
        SPI.transfer(0x00);

        for (int i = 0; i < size; i++)
        {
            buffer[i] = SPI.transfer(0x00);
        }

        SPI.endTransaction();

        digitalWrite(_ss, HIGH);
    }

    void LoRaClass::setModulationParams(uint8_t sf, uint8_t bw, uint8_t cr, int ldro) {
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

      executeOpcode(OP_MODULATION_PARAMS, buf, 8);
    }

    void LoRaClass::setPacketParams(long preamble, uint8_t headermode, uint8_t length, uint8_t crc) {
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
    
      executeOpcode(OP_PACKET_PARAMS, buf, 9);
    }
#endif


int LoRaClass::begin(long frequency)
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

  if (!lora_preinit_done) {
    if (!preInit()) {
      return false;
    }
  }

  #if MODEM == SX1276 || MODEM == SX1278
      // put in sleep mode
      sleep();

      // set frequency
      setFrequency(frequency);

      // set base addresses
      writeRegister(REG_FIFO_TX_BASE_ADDR, 0);
      writeRegister(REG_FIFO_RX_BASE_ADDR, 0);

      // set LNA boost
      writeRegister(REG_LNA, readRegister(REG_LNA) | 0x03);

      // set auto AGC
      writeRegister(REG_MODEM_CONFIG_3, 0x04);

      // set output power to 2 dBm
      setTxPower(2);

      // put in standby mode
      idle();
  #elif MODEM == SX1262
      //#if HAS_TCXO
          // turn TCXO on
          enableTCXO();
      //#endif
      loraMode();
      idle();
      // cannot access registers in sleep mode on sx1262, set to idle instead
      if (_rxen != -1) {
          pinMode(_rxen, OUTPUT);
          enableAntenna();
      }
      // calibrate RC64k, RC13M, PLL, ADC and image
      uint8_t calibrate = 0x7F; 
      executeOpcode(OP_CALIBRATE, &calibrate, 1);

      setFrequency(frequency);

      // set output power to 2 dBm
      setTxPower(2);

      // set LNA boost
      writeRegister(REG_LNA, 0x96);

      // set base addresses
      uint8_t basebuf[2] = {0};
      executeOpcode(OP_BUFFER_BASE_ADDR, basebuf, 2);

      setModulationParams(_sf, _bw, _cr, _ldro);
      setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
  #endif

  return 1;
}

void LoRaClass::end()
{
  // put in sleep mode
  sleep();

  // stop SPI
  SPI.end();

  lora_preinit_done = false;
}

int LoRaClass::beginPacket(int implicitHeader)
{
  // put in standby mode
  idle();

  if (implicitHeader) {
    implicitHeaderMode();
  } else {
    explicitHeaderMode();
  }

  #if MODEM == SX1276 || MODEM == SX1278
      // reset FIFO address and paload length
      writeRegister(REG_FIFO_ADDR_PTR, 0);
      writeRegister(REG_PAYLOAD_LENGTH, 0);
  #elif MODEM == SX1262
      _payloadLength = 0;
      fifo_tx_addr_ptr = 0;
      setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
  #endif

  return 1;
}

int LoRaClass::endPacket()
{
  #if MODEM == SX1276 || MODEM == SX1278
      // put in TX mode
      writeRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_TX);

      // wait for TX done
      while ((readRegister(REG_IRQ_FLAGS) & IRQ_TX_DONE_MASK) == 0) {
        yield();
      }

      // clear IRQ's
      writeRegister(REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);
  #elif MODEM == SX1262
      setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
      // put in single TX mode
      uint8_t timeout[3] = {0};
      executeOpcode(OP_TX, timeout, 3);

      uint8_t buf[2];

      buf[0] = 0x00;
      buf[1] = 0x00;

      executeOpcodeRead(OP_GET_IRQ_STATUS, buf, 2);

      // wait for TX done
      while ((buf[1] & IRQ_TX_DONE_MASK) == 0) {
        buf[0] = 0x00;
        buf[1] = 0x00;
        executeOpcodeRead(OP_GET_IRQ_STATUS, buf, 2);
        yield();
      }

      // clear IRQ's

      uint8_t mask[2];
      mask[0] = 0x00;
      mask[1] = IRQ_TX_DONE_MASK;
      executeOpcode(OP_CLEAR_IRQ_STATUS, mask, 2);
  #endif
  return 1;
}

int LoRaClass::parsePacket(int size)
{
  int packetLength = 0;
  #if MODEM == SX1276 || MODEM == SX1278
    int irqFlags = readRegister(REG_IRQ_FLAGS);
  #elif MODEM == SX1262
    uint8_t buf[2];
    buf[0] = 0x00;
    buf[1] = 0x00;

    executeOpcodeRead(OP_GET_IRQ_STATUS, buf, 2);
  #endif

  if (size > 0) {
    implicitHeaderMode();

    #if MODEM == SX1276 || MODEM == SX1278
        writeRegister(REG_PAYLOAD_LENGTH, size & 0xff);
    #elif MODEM == SX1262
        // tell radio payload length
        _payloadLength = size;
        setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
    #endif
  } else {
    explicitHeaderMode();
  }

  #if MODEM == SX1276 || MODEM == SX1278
    // clear IRQ's
    writeRegister(REG_IRQ_FLAGS, irqFlags);
    if ((irqFlags & IRQ_RX_DONE_MASK) && (irqFlags & IRQ_PAYLOAD_CRC_ERROR_MASK) == 0) {
  #elif MODEM == SX1262
    uint8_t irqBufFlags[2];

    irqBufFlags[0] = buf[0];
    irqBufFlags[1] = buf[1];

    executeOpcode(OP_CLEAR_IRQ_STATUS, irqBufFlags, 2);

    if ((buf[0] & IRQ_RX_DONE_MASK) && (buf[1] & IRQ_PAYLOAD_CRC_ERROR_MASK) == 0) {
  #endif

        // received a packet
        _packetIndex = 0;

    #if MODEM == SX1276 || MODEM == SX1278
        // read packet length
        if (_implicitHeaderMode) {
          packetLength = readRegister(REG_PAYLOAD_LENGTH);
        } else {
          packetLength = readRegister(REG_RX_NB_BYTES);
        }
    #elif MODEM == SX1262
        buf[0] = 0x00;
        buf[1] = 0x00;
        executeOpcodeRead(OP_RX_BUFFER_STATUS, buf, 2);
        packetLength = buf[0];
    #endif

    #if MODEM == SX1276 || MODEM == SX1278
        // set FIFO address to current RX address
        writeRegister(REG_FIFO_ADDR_PTR, readRegister(REG_FIFO_RX_CURRENT_ADDR));
    #endif

    // put in standby mode
    idle();
  
  #if MODEM == SX1276 || MODEM == SX1278
      } else if (readRegister(REG_OP_MODE) != (MODE_LONG_RANGE_MODE | MODE_RX_SINGLE)) {
        // not currently in RX mode

        // reset FIFO address
        writeRegister(REG_FIFO_ADDR_PTR, 0);

        // put in single RX mode
        writeRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_RX_SINGLE);
      }
  #elif MODEM == SX1262
  } else {
    uint8_t status;
    status = 0x00;
    executeOpcodeRead(OP_STATUS, &status, 1); 
    if ((status >> 4 & 0x7) != 0x5) {
        // not currently in RX mode
        
        // put in single RX mode
        uint8_t buf[3] = {0};
        executeOpcode(OP_RX, buf, 3);
    }
  }
  #endif
  return packetLength;
}

uint8_t LoRaClass::modemStatus() {
  #if MODEM == SX1276 || MODEM == SX1278 
    return readRegister(REG_MODEM_STAT);
  #elif MODEM == SX1262
    // imitate the register status from the sx1276 / 78
    uint8_t buf[2] = {0};


    executeOpcodeRead(OP_GET_IRQ_STATUS, buf, 2);

    uint8_t clearbuf[2] = {0};

    uint8_t byte = 0x00;

    if (buf[1] & IRQ_PREAMBLE_DET_MASK != 0) {
        byte = byte | 0x01 | 0x04;
        // clear register after reading
        clearbuf[1] = IRQ_PREAMBLE_DET_MASK; 
    }

    if (buf[1] & IRQ_HEADER_DET_MASK != 0) {
        byte = byte | 0x02 | 0x04;
        // clear register after reading
        clearbuf[1] = clearbuf[1] | IRQ_HEADER_DET_MASK; 
    }

    executeOpcode(OP_CLEAR_IRQ_STATUS, clearbuf, 2);

    return byte; 
  #endif
}


uint8_t LoRaClass::currentRssiRaw() {
  #if MODEM == SX1276 || MODEM == SX1278
    uint8_t rssi = readRegister(REG_RSSI_VALUE);
    return rssi;
  #elif MODEM == SX1262
    uint8_t byte = 0;
    executeOpcodeRead(OP_CURRENT_RSSI, &byte, 1);
    return byte;
  #endif
}

int ISR_VECT LoRaClass::currentRssi() {
  #if MODEM == SX1276 || MODEM == SX1278
    int rssi = (int)readRegister(REG_RSSI_VALUE) - RSSI_OFFSET;
    if (_frequency < 820E6) rssi -= 7;
    return rssi;
  #elif MODEM == SX1262
    uint8_t byte = 0;
    executeOpcodeRead(OP_CURRENT_RSSI, &byte, 1);
    int rssi = -(int(byte)) / 2;
    return rssi;
  #endif
}

uint8_t LoRaClass::packetRssiRaw() {
  #if MODEM == SX1276 || MODEM == SX1278
    uint8_t pkt_rssi_value = readRegister(REG_PKT_RSSI_VALUE);
    return pkt_rssi_value;
  #elif MODEM == SX1262
    uint8_t buf[3] = {0};
    executeOpcodeRead(OP_PACKET_STATUS, buf, 3);
    return buf[2];
  #endif
}

int ISR_VECT LoRaClass::packetRssi() {
  #if MODEM == SX1276 || MODEM == SX1278
      int pkt_rssi = (int)readRegister(REG_PKT_RSSI_VALUE) - RSSI_OFFSET;
      int pkt_snr = packetSnr();

      if (_frequency < 820E6) pkt_rssi -= 7;

      if (pkt_snr < 0) {
        pkt_rssi += pkt_snr;
      } else {
        // Slope correction is (16/15)*pkt_rssi,
        // this estimation looses one floating point
        // operation, and should be precise enough.
        pkt_rssi = (int)(1.066 * pkt_rssi);
      }
      return pkt_rssi;

  #elif MODEM == SX1262
    // may need more calculations here
    uint8_t buf[3] = {0};
    executeOpcodeRead(OP_PACKET_STATUS, buf, 3);
    int pkt_rssi = -buf[0] / 2;
    return pkt_rssi;
  #endif
}

uint8_t ISR_VECT LoRaClass::packetSnrRaw() {
  #if MODEM == SX1276 || MODEM == SX1278
    return readRegister(REG_PKT_SNR_VALUE);
  #elif MODEM == SX1262
    uint8_t buf[3] = {0};
    executeOpcodeRead(OP_PACKET_STATUS, buf, 3);
    return buf[1];
  #endif
}

float ISR_VECT LoRaClass::packetSnr() {
  #if MODEM == SX1276 || MODEM == SX1278
    return ((int8_t)readRegister(REG_PKT_SNR_VALUE)) * 0.25;
  #elif MODEM == SX1262
    uint8_t buf[3] = {0};
    executeOpcodeRead(OP_PACKET_STATUS, buf, 3);
    return float(buf[1]) * 0.25;
  #endif
}

long LoRaClass::packetFrequencyError()
{
  int32_t freqError = 0;
  #if MODEM == SX1276 || MODEM == SX1278
    freqError = static_cast<int32_t>(readRegister(REG_FREQ_ERROR_MSB) & B111);
    freqError <<= 8L;
    freqError += static_cast<int32_t>(readRegister(REG_FREQ_ERROR_MID));
    freqError <<= 8L;
    freqError += static_cast<int32_t>(readRegister(REG_FREQ_ERROR_LSB));

    if (readRegister(REG_FREQ_ERROR_MSB) & B1000) { // Sign bit is on
       freqError -= 524288; // B1000'0000'0000'0000'0000
    }

    const float fXtal = 32E6; // FXOSC: crystal oscillator (XTAL) frequency (2.5. Chip Specification, p. 14)
    const float fError = ((static_cast<float>(freqError) * (1L << 24)) / fXtal) * (getSignalBandwidth() / 500000.0f); // p. 37

    return static_cast<long>(fError);
  #elif MODEM == SX1262
    // todo: implement this, no idea how to check it on the sx1262
    const float fError = 0.0;
    return static_cast<long>(fError);
  #endif
}

size_t LoRaClass::write(uint8_t byte)
{
  return write(&byte, sizeof(byte));
}

size_t LoRaClass::write(const uint8_t *buffer, size_t size)
{
  #if MODEM == SX1276 || MODEM == SX1278
      int currentLength = readRegister(REG_PAYLOAD_LENGTH);

      // check size
      if ((currentLength + size) > MAX_PKT_LENGTH) {
        size = MAX_PKT_LENGTH - currentLength;
      }
  #elif MODEM == SX1262
      if ((_payloadLength + size) > MAX_PKT_LENGTH) {
          size = MAX_PKT_LENGTH - _payloadLength;
      }
  #endif

  // write data
  #if MODEM == SX1276 || MODEM == SX1278
    for (size_t i = 0; i < size; i++) {
        writeRegister(REG_FIFO, buffer[i]);
    }

    // update length
    writeRegister(REG_PAYLOAD_LENGTH, currentLength + size);
  #elif MODEM == SX1262
    writeBuffer(buffer, size);
    _payloadLength = _payloadLength + size;
  #endif
  return size;
}

int ISR_VECT LoRaClass::available()
{
  #if MODEM == SX1276 || MODEM == SX1278
    return (readRegister(REG_RX_NB_BYTES) - _packetIndex);
  #elif MODEM == SX1262
    uint8_t buf[2] = {0};
    executeOpcodeRead(OP_RX_BUFFER_STATUS, buf, 2);
    return buf[0] - _packetIndex;
  #endif
}

int ISR_VECT LoRaClass::read()
{
  if (!available()) {
    return -1;
  }

  #if MODEM == SX1276 || MODEM == SX1278
    _packetIndex++;
    return readRegister(REG_FIFO);
  #elif MODEM == SX1262
    // if received new packet
    if (_packetIndex == 0) {
        uint8_t rxbuf[2] = {0};
        executeOpcodeRead(OP_RX_BUFFER_STATUS, rxbuf, 2);
        int size = rxbuf[0];
        fifo_rx_addr_ptr = rxbuf[1];

        readBuffer(packet, size);
    }

    uint8_t byte = packet[_packetIndex];
    _packetIndex++;
    return byte;
  #endif

}

int LoRaClass::peek()
{
  if (!available()) {
    return -1;
  }

  #if MODEM == SX1276 || MODEM == SX1278
      // store current FIFO address
      int currentAddress = readRegister(REG_FIFO_ADDR_PTR);

      // read
      uint8_t b = readRegister(REG_FIFO);

      // restore FIFO address
      writeRegister(REG_FIFO_ADDR_PTR, currentAddress);

  #elif MODEM == SX1262
    // if received new packet
    if (_packetIndex == 0) {
        uint8_t rxbuf[2] = {0};
        executeOpcodeRead(OP_RX_BUFFER_STATUS, rxbuf, 2);
        int size = rxbuf[0];
        fifo_rx_addr_ptr = rxbuf[1];

        readBuffer(packet, size);
    }

    uint8_t b = packet[_packetIndex];
  #endif
  return b;
}

void LoRaClass::flush()
{
}

void LoRaClass::onReceive(void(*callback)(int))
{
  _onReceive = callback;

  if (callback) {
    pinMode(_dio0, INPUT);

    #if MODEM == SX1276 || MODEM == SX1278
      writeRegister(REG_DIO_MAPPING_1, 0x00);
    #elif MODEM == SX1262
      // set preamble and header detection irqs, plus dio0 mask
      uint8_t buf[8];

      // set irq masks, enable all
      buf[0] = 0xFF; 
      buf[1] = 0xFF;

      // set dio0 masks
      buf[2] = 0x00;
      buf[3] = IRQ_RX_DONE_MASK; 

      // set dio1 masks
      buf[4] = 0x00; 
      buf[5] = 0x00;

      // set dio2 masks
      buf[6] = 0x00; 
      buf[7] = 0x00;

      executeOpcode(OP_SET_IRQ_FLAGS, buf, 8);
    #endif
#ifdef SPI_HAS_NOTUSINGINTERRUPT
    SPI.usingInterrupt(digitalPinToInterrupt(_dio0));
#endif
    attachInterrupt(digitalPinToInterrupt(_dio0), LoRaClass::onDio0Rise, RISING);
  } else {
    detachInterrupt(digitalPinToInterrupt(_dio0));
#ifdef SPI_HAS_NOTUSINGINTERRUPT
    SPI.notUsingInterrupt(digitalPinToInterrupt(_dio0));
#endif
  }
}

void LoRaClass::receive(int size)
{
  if (size > 0) {
    implicitHeaderMode();

    #if MODEM == SX1276 || MODEM == SX1278
        writeRegister(REG_PAYLOAD_LENGTH, size & 0xff);
    #elif MODEM == SX1262
        // tell radio payload length
        _payloadLength = size;
        setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
    #endif
  } else {
    explicitHeaderMode();
  }

  #if MODEM == SX1276 || MODEM == SX1278
    writeRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_RX_CONTINUOUS);
  #elif MODEM == SX1262
    uint8_t mode[3] = {0xFF, 0xFF, 0xFF}; // continuous mode
    executeOpcode(OP_RX, mode, 3);
  #endif
}

void LoRaClass::idle()
{
  #if MODEM == SX1276 || MODEM == SX1278
      writeRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
  #elif MODEM == SX1262
      //#if HAS_TCXO
          // STDBY_XOSC
          uint8_t byte = 0x01;
      //#else
      //    // STDBY_RC
      //    uint8_t byte = 0x00;
      //#endif
      executeOpcode(OP_STANDBY, &byte, 1); 
  #endif
}

void LoRaClass::sleep()
{
  #if MODEM == SX1276 || MODEM == SX1278
    writeRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_SLEEP);
  #elif MODEM == SX1262
    if (_rxen != -1) {
       disableAntenna();
     }
     uint8_t byte = 0x00;
     executeOpcode(OP_SLEEP, &byte, 1);
  #endif
}

void LoRaClass::enableTCXO() {
  #if MODEM == SX1276 || MODEM == SX1278
      uint8_t tcxo_reg = readRegister(REG_TCXO);
      writeRegister(REG_TCXO, tcxo_reg | 0x10);
  #elif MODEM == SX1262
    // only tested for RAK4630, voltage may be different on other platforms
    uint8_t buf[4] = {MODE_TCXO_3_3V, 0x00, 0x00, 0xFF};
    executeOpcode(OP_DIO3_TCXO_CTRL, buf, 4);
  #endif
}

void LoRaClass::disableTCXO() {
  #if MODEM == SX1276 || MODEM == SX1278
      uint8_t tcxo_reg = readRegister(REG_TCXO);
      writeRegister(REG_TCXO, tcxo_reg & 0xEF);
  #elif MODEM == SX1262
      // currently cannot disable on SX1262?
  #endif
}

void LoRaClass::setTxPower(int level, int outputPin) {
  #if MODEM == SX1276 || MODEM == SX1278
      if (PA_OUTPUT_RFO_PIN == outputPin) {
        // RFO
        if (level < 0) {
          level = 0;
        } else if (level > 14) {
          level = 14;
        }

        writeRegister(REG_PA_DAC, 0x84);
        writeRegister(REG_PA_CONFIG, 0x70 | level);

      } else {
        // PA BOOST
        if (level < 2) {
          level = 2;
        } else if (level > 17) {
          level = 17;
        }

        writeRegister(REG_PA_DAC, 0x84);
        writeRegister(REG_PA_CONFIG, PA_BOOST | (level - 2));
      }
  #elif MODEM == SX1262
    // currently no low power mode for SX1262 implemented, assuming PA boost
    
    // WORKAROUND - Better Resistance of the SX1262 Tx to Antenna Mismatch, see DS_SX1261-2_V1.2 datasheet chapter 15.2
    // RegTxClampConfig = @address 0x08D8
    writeRegister(0x08D8, readRegister(0x08D8) | (0x0F << 1));

    uint8_t pa_buf[4];

    pa_buf[0] = 0x04;
    pa_buf[1] = 0x07;
    pa_buf[2] = 0x00;
    pa_buf[3] = 0x01;

    executeOpcode(OP_PA_CONFIG, pa_buf, 4); // set pa_config for high power

    if (level > 22) {
        level = 22;
    }
    else if (level < -9) {
        level = -9;
    }

    writeRegister(REG_OCP, 0x38); // 160mA limit, overcurrent protection

    uint8_t tx_buf[2];

    tx_buf[0] = level;
    tx_buf[1] = 0x02; // ramping time - 40 microseconds
    
    executeOpcode(OP_TX_PARAMS, tx_buf, 2);

    _txp = level;
  #endif
}

uint8_t LoRaClass::getTxPower() {
  #if MODEM == SX1276 || MODEM == SX1278
      byte txp = readRegister(REG_PA_CONFIG);
      return txp;
  #elif MODEM == SX1262
      return _txp;
  #endif
}

void LoRaClass::setFrequency(long frequency) {
  _frequency = frequency;

  #if MODEM == SX1276 || MODEM == SX1278
      uint32_t frf = ((uint64_t)frequency << 19) / 32000000;

      writeRegister(REG_FRF_MSB, (uint8_t)(frf >> 16));
      writeRegister(REG_FRF_MID, (uint8_t)(frf >> 8));
      writeRegister(REG_FRF_LSB, (uint8_t)(frf >> 0));

      optimizeModemSensitivity();
  #elif MODEM == SX1262
      uint8_t buf[4];

      uint32_t freq = (uint32_t)((double)frequency / (double)FREQ_STEP);

      buf[0] = ((freq >> 24) & 0xFF);
      buf[1] = ((freq >> 16) & 0xFF);
      buf[2] = ((freq >> 8) & 0xFF);
      buf[3] = (freq & 0xFF);
 
      executeOpcode(OP_RF_FREQ, buf, 4);
  #endif
}

uint32_t LoRaClass::getFrequency() {
  #if MODEM == SX1276 || MODEM == SX1278
      uint8_t msb = readRegister(REG_FRF_MSB);
      uint8_t mid = readRegister(REG_FRF_MID);
      uint8_t lsb = readRegister(REG_FRF_LSB);

      uint32_t frf = ((uint32_t)msb << 16) | ((uint32_t)mid << 8) | (uint32_t)lsb;
      uint64_t frm = (uint64_t)frf*32000000;
      uint32_t frequency = (frm >> 19);

  #elif MODEM == SX1262
      // we can't read the frequency on the sx1262
      uint32_t frequency = _frequency;
  #endif

  return frequency;
}

void LoRaClass::setSpreadingFactor(int sf)
{
  if (sf < 6) {
    sf = 6;
  } else if (sf > 12) {
    sf = 12;
  }

  #if MODEM == SX1276 || MODEM == SX1278
      if (sf == 6) {
        writeRegister(REG_DETECTION_OPTIMIZE, 0xc5);
        writeRegister(REG_DETECTION_THRESHOLD, 0x0c);
      } else {
        writeRegister(REG_DETECTION_OPTIMIZE, 0xc3);
        writeRegister(REG_DETECTION_THRESHOLD, 0x0a);
      }

      writeRegister(REG_MODEM_CONFIG_2, (readRegister(REG_MODEM_CONFIG_2) & 0x0f) | ((sf << 4) & 0xf0));
  #elif MODEM == SX1262
      setModulationParams(sf, _bw, _cr, _ldro);
  #endif
  handleLowDataRate();
}

long LoRaClass::getSignalBandwidth()
{
  #if MODEM == SX1276 || MODEM == SX1278
      byte bw = (readRegister(REG_MODEM_CONFIG_1) >> 4);
      switch (bw) {
        case 0: return 7.8E3;
        case 1: return 10.4E3;
        case 2: return 15.6E3;
        case 3: return 20.8E3;
        case 4: return 31.25E3;
        case 5: return 41.7E3;
        case 6: return 62.5E3;
        case 7: return 125E3;
        case 8: return 250E3;
        case 9: return 500E3;
      }
  #elif MODEM == SX1262
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
  #endif
  
  return 0;
}

void LoRaClass::handleLowDataRate(){
  #if MODEM == SX1276 || MODEM == SX1278
      int sf = (readRegister(REG_MODEM_CONFIG_2) >> 4);
      if ( long( (1<<sf) / (getSignalBandwidth()/1000)) > 16) {
        // set auto AGC and LowDataRateOptimize
        writeRegister(REG_MODEM_CONFIG_3, (1<<3)|(1<<2));
      } else {
        // set auto AGC
        writeRegister(REG_MODEM_CONFIG_3, (1<<2));
      }
  #elif MODEM == SX1262
      _ldro = 1;
      setModulationParams(_sf, _bw, _cr, _ldro);
  #endif
}

void LoRaClass::optimizeModemSensitivity(){
  #if MODEM == SX1276 || MODEM == SX1278
      byte bw = (readRegister(REG_MODEM_CONFIG_1) >> 4);
      uint32_t freq = getFrequency();

      if (bw == 9 && (410E6 <= freq) && (freq <= 525E6)) {
        writeRegister(REG_HIGH_BW_OPTIMIZE_1, 0x02);
        writeRegister(REG_HIGH_BW_OPTIMIZE_2, 0x7f);
      } else if (bw == 9 && (820E6 <= freq) && (freq <= 1020E6)) {
        writeRegister(REG_HIGH_BW_OPTIMIZE_1, 0x02);
        writeRegister(REG_HIGH_BW_OPTIMIZE_2, 0x64);
      } else {
        writeRegister(REG_HIGH_BW_OPTIMIZE_1, 0x03);
      }
  #elif MODEM == SX1262
      // todo: check if there's anything the sx1262 can do here
  #endif
}

void LoRaClass::setSignalBandwidth(long sbw)
{
  #if MODEM == SX1276 || MODEM == SX1278
      int bw;

      if (sbw <= 7.8E3) {
        bw = 0;
      } else if (sbw <= 10.4E3) {
        bw = 1;
      } else if (sbw <= 15.6E3) {
        bw = 2;
      } else if (sbw <= 20.8E3) {
        bw = 3;
      } else if (sbw <= 31.25E3) {
        bw = 4;
      } else if (sbw <= 41.7E3) {
        bw = 5;
      } else if (sbw <= 62.5E3) {
        bw = 6;
      } else if (sbw <= 125E3) {
        bw = 7;
      } else if (sbw <= 250E3) {
        bw = 8;
      } else /*if (sbw <= 250E3)*/ {
        bw = 9;
      }

      writeRegister(REG_MODEM_CONFIG_1, (readRegister(REG_MODEM_CONFIG_1) & 0x0f) | (bw << 4));

  #elif MODEM == SX1262
      uint8_t bw;

      if (sbw <= 7.8E3) {
        bw = 0x00;
      } else if (sbw <= 10.4E3) {
        bw = 0x08;
      } else if (sbw <= 15.6E3) {
        bw = 0x01;
      } else if (sbw <= 20.8E3) {
        bw = 0x09;
      } else if (sbw <= 31.25E3) {
        bw = 0x02;
      } else if (sbw <= 41.7E3) {
        bw = 0x0A;
      } else if (sbw <= 62.5E3) {
        bw = 0x03;
      } else if (sbw <= 125E3) {
        bw = 0x04;
      } else if (sbw <= 250E3) {
        bw = 0x05;
      } else /*if (sbw <= 250E3)*/ {
        bw = 0x06;
      }

      setModulationParams(_sf, bw, _cr, _ldro);
  #endif

  handleLowDataRate();
  optimizeModemSensitivity();
}

void LoRaClass::setCodingRate4(int denominator)
{
  if (denominator < 5) {
    denominator = 5;
  } else if (denominator > 8) {
    denominator = 8;
  }

  int cr = denominator - 4;

  #if MODEM == SX1276 || MODEM == SX1278
      writeRegister(REG_MODEM_CONFIG_1, (readRegister(REG_MODEM_CONFIG_1) & 0xf1) | (cr << 1));
  #elif MODEM == SX1262
      setModulationParams(_sf, _bw, cr, _ldro);
  #endif
}

void LoRaClass::setPreambleLength(long length)
{
  #if MODEM == SX1276 || MODEM == SX1278
      writeRegister(REG_PREAMBLE_MSB, (uint8_t)(length >> 8));
      writeRegister(REG_PREAMBLE_LSB, (uint8_t)(length >> 0));
  #elif MODEM == SX1262
      setPacketParams(length, _implicitHeaderMode, _payloadLength, _crcMode);
  #endif
}

void LoRaClass::setSyncWord(int sw)
{
  #if MODEM == SX1276 || MODEM == SX1278
    writeRegister(REG_SYNC_WORD, sw);
  #elif MODEM == SX1262
    writeRegister(REG_SYNC_WORD_MSB, sw & 0xFF00);
    writeRegister(REG_SYNC_WORD_LSB, sw & 0x00FF);
  #endif
}

void LoRaClass::enableCrc()
{
  #if MODEM == SX1276 || MODEM == SX1278
      writeRegister(REG_MODEM_CONFIG_2, readRegister(REG_MODEM_CONFIG_2) | 0x04);
  #elif MODEM == SX1262
      _crcMode = 1;
      setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
  #endif
}

void LoRaClass::disableCrc()
{
  #if MODEM == SX1276 || MODEM == SX1278
    writeRegister(REG_MODEM_CONFIG_2, readRegister(REG_MODEM_CONFIG_2) & 0xfb);
  #elif MODEM == SX1262
    _crcMode = 0;
    setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
  #endif
}

byte LoRaClass::random()
{
  #if MODEM == SX1276 || MODEM == SX1278
    return readRegister(REG_RSSI_WIDEBAND);
  #elif MODEM == SX1262
    return readRegister(REG_RANDOM_GEN);
  #endif
}

void LoRaClass::setPins(int ss, int reset, int dio0, int rxen, int busy)
{
  _ss = ss;
  _reset = reset;
  _dio0 = dio0;
  _rxen = rxen;
  _busy = busy;
}

void LoRaClass::setSPIFrequency(uint32_t frequency)
{
  _spiSettings = SPISettings(frequency, MSBFIRST, SPI_MODE0);
}

void LoRaClass::dumpRegisters(Stream& out)
{
  for (int i = 0; i < 128; i++) {
    out.print("0x");
    out.print(i, HEX);
    out.print(": 0x");
    out.println(readRegister(i), HEX);
  }
}

void LoRaClass::explicitHeaderMode()
{
  _implicitHeaderMode = 0;

  #if MODEM == SX1276 || MODEM == SX1278
      writeRegister(REG_MODEM_CONFIG_1, readRegister(REG_MODEM_CONFIG_1) & 0xfe);
  #elif MODEM == SX1262
      setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
  #endif
}

void LoRaClass::implicitHeaderMode()
{
  _implicitHeaderMode = 1;

  #if MODEM == SX1276 || MODEM == SX1278
      writeRegister(REG_MODEM_CONFIG_1, readRegister(REG_MODEM_CONFIG_1) | 0x01);
  #elif MODEM == SX1262
      setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
  #endif
}


void ISR_VECT LoRaClass::handleDio0Rise()
{
  #if MODEM == SX1276 || MODEM == SX1278
    int irqFlags = readRegister(REG_IRQ_FLAGS);

    // clear IRQ's
    writeRegister(REG_IRQ_FLAGS, irqFlags);
    if ((irqFlags & IRQ_PAYLOAD_CRC_ERROR_MASK) == 0) {
  #elif MODEM == SX1262
    uint8_t buf[2];

    buf[0] = 0x00;
    buf[1] = 0x00;

    executeOpcodeRead(OP_GET_IRQ_STATUS, buf, 2);

    executeOpcode(OP_CLEAR_IRQ_STATUS, buf, 2);


    if ((buf[1] & IRQ_PAYLOAD_CRC_ERROR_MASK) == 0) {
  #endif


  // received a packet
  _packetIndex = 0;

    // read packet length
  #if MODEM == SX1276 || MODEM == SX1278
    int packetLength = _implicitHeaderMode ? readRegister(REG_PAYLOAD_LENGTH) : readRegister(REG_RX_NB_BYTES);

    // set FIFO address to current RX address
    writeRegister(REG_FIFO_ADDR_PTR, readRegister(REG_FIFO_RX_CURRENT_ADDR));
  #elif MODEM == SX1262
    uint8_t rxbuf[2] = {0};
    executeOpcodeRead(OP_RX_BUFFER_STATUS, rxbuf, 2);
    int packetLength = rxbuf[0];
  #endif

  if (_onReceive) {
    _onReceive(packetLength);
  }

  #if MODEM == SX1276 || MODEM == SX1278
    // reset FIFO address
    writeRegister(REG_FIFO_ADDR_PTR, 0);
  #endif
  }
}

void ISR_VECT LoRaClass::onDio0Rise()
{
  LoRa.handleDio0Rise();
}

LoRaClass LoRa;
