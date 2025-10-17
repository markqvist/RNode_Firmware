// Copyright (c) Sandeep Mistry. All rights reserved.
// Licensed under the MIT license.

// Modifications and additions copyright 2023 by Mark Qvist
// Obviously still under the MIT license.

// LR1110 Modifications and additions copyright 2024 by Kevin Brosius
// MIT license.

#ifndef LR1110_H
#define LR1110_H

#include <Arduino.h>
#include <SPI.h>
#include "Modem.h"

#define LORA_DEFAULT_SS_PIN    10
#define LORA_DEFAULT_RESET_PIN 9
#define LORA_DEFAULT_DIO0_PIN  2
#define LORA_DEFAULT_RXEN_PIN  -1
#define LORA_DEFAULT_TXEN_PIN  -1
#define LORA_DEFAULT_BUSY_PIN  -1

#define PA_OUTPUT_RFO_PIN      0
#define PA_OUTPUT_PA_BOOST_PIN 1

#define RSSI_OFFSET 157

class lr11xx : public Stream {
public:
  lr11xx();

  int begin(long frequency);
  void end();

  int beginPacket(int implicitHeader = false);
  int endPacket();

  int parsePacket(int size = 0);
  int packetRssi();
  int currentRssi();
  bool dcd();
  uint8_t packetRssiRaw();
  uint8_t currentRssiRaw();
  uint8_t packetSnrRaw();
  float packetSnr();
  long packetFrequencyError();

  // from Print
  virtual size_t write(uint8_t byte);
  virtual size_t write(const uint8_t *buffer, size_t size);

  // from Stream
  virtual int available();
  virtual int available(uint8_t *size, uint8_t *rxbufstart);
  virtual int read();
  virtual int peek();
  virtual void flush();

  void onReceive(void(*callback)(int));

  void receive(int size = 0);
  void standby();
  void sleep();

  bool preInit();
  uint8_t getTxPower();
  void setTxPower(int level, int outputPin = PA_OUTPUT_PA_BOOST_PIN);
  uint32_t getFrequency();
  void setFrequency(long frequency);
  void setSpreadingFactor(int sf);
  long getSignalBandwidth();
  void setSignalBandwidth(long sbw);
  void setCodingRate4(int denominator);
  void setPreambleLength(long length);
  void setSyncWord(uint16_t sw);
  uint8_t modemStatus();
  void enableCrc();
  void disableCrc();
  void enableTCXO();
  void disableTCXO();

  void rxAntEnable();
  void loraMode();
  void waitOnBusy(uint8_t doYield = 1);
  void executeOpcode(uint8_t opcode, uint8_t *buffer, uint8_t size);
  void executeOpcodeRead(uint8_t opcode, uint8_t *buffer, uint8_t size);
  void writeBuffer(const uint8_t* buffer, size_t size);
  void readBuffer(uint8_t* buffer, size_t size);
  void setPacketParams(long preamble, uint8_t headermode, uint8_t length, uint8_t crc);

  void setModulationParams(uint8_t sf, uint8_t bw, uint8_t cr, int ldro);

  // deprecated
  void crc() { enableCrc(); }
  void noCrc() { disableCrc(); }

  byte random();

  void setPins(int ss = LORA_DEFAULT_SS_PIN, int reset = LORA_DEFAULT_RESET_PIN, int dio0 = LORA_DEFAULT_DIO0_PIN, int busy = LORA_DEFAULT_BUSY_PIN /*, int rxen = LORA_DEFAULT_RXEN_PIN */);
  void setSPIFrequency(uint32_t frequency);

  void dumpRegisters(Stream& out);

  void foreground(void);

private:
  void explicitHeaderMode();
  void implicitHeaderMode();

  void handleDio0Rise();

  uint8_t readRegister(uint16_t address);
  void writeRegister(uint16_t address, uint8_t value);
  uint8_t singleTransfer(uint8_t opcode, uint16_t address, uint8_t value);
uint16_t cmdTransfer(const char *prt, uint8_t *cmd);
uint16_t cmdTransfer(const char *prt, uint8_t *cmd, bool );
uint16_t A32Transfer(uint16_t opcode, uint16_t address, uint16_t value);
void getErrors(void);
int decode_stat(uint8_t stat);
int decode_stat(const char *prt,uint8_t stat1,uint8_t stat2,uint8_t print_it);
int decode_stat(uint8_t stat, uint8_t stat2);
void getErrors2(void);
void doSetup(void);
//void dioStat(void);
void dioStatInternal(uint8_t *stat1, uint8_t *int2, uint8_t *int3, uint8_t *int4);
void dioStat(uint8_t *stat1, uint8_t *int2, uint8_t *int3, uint8_t *int4);
uint8_t dumpRx(void);
void handleIntStatus(void);
void checkOpState(void);
void checkTXstat(void);
void print_state(void);


  static void onDio0Rise();

  void handleLowDataRate();
  void optimizeModemSensitivity();

  void reset(void);
  void calibrate(uint8_t);
  void calibrate_image(long frequency);

private:
  SPISettings _spiSettings;
  int _ss;
  int _reset;
  int _dio0;
  int _rxen;
  int _busy;
  long _frequency;
  int _txp;
  uint8_t _sf;
  uint8_t _bw;
  uint8_t _cr;
  uint8_t _ldro;
  int _packetIndex;
  int _packetIndexRX;
  int _local_rx_buffer;
  int _preambleLength;
  int _implicitHeaderMode;
  int _payloadLength;
  int _crcMode;
  int _fifo_tx_addr_ptr;
  int _fifo_rx_addr_ptr;
  uint8_t _packet[255];
  uint8_t _packetRX[255];
  bool _preinit_done;
  void (*_onReceive)(int);
  int modulationDirty;
  int packetParamsDirty;
};

extern lr11xx lr11xx_modem;

#endif
