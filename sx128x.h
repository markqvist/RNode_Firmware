// Copyright Sandeep Mistry, Mark Qvist and Jacob Eva.
// Licensed under the MIT license.

#ifndef SX128X_H
#define SX128X_H

#include <Arduino.h>
#include <SPI.h>
#include "Modem.h"

#define LORA_DEFAULT_SS_PIN     10
#define LORA_DEFAULT_RESET_PIN  9
#define LORA_DEFAULT_DIO0_PIN   2
#define LORA_DEFAULT_RXEN_PIN  -1
#define LORA_DEFAULT_TXEN_PIN  -1
#define LORA_DEFAULT_BUSY_PIN  -1
#define LORA_MODEM_TIMEOUT_MS   15E3
#define PA_OUTPUT_RFO_PIN       0
#define PA_OUTPUT_PA_BOOST_PIN  1
#define RSSI_OFFSET             157

class sx128x : public Stream {
public:
  sx128x();

  int begin(unsigned long frequency);
  void end();
  void reset();

  int beginPacket(int implicitHeader = false);
  int endPacket();

  int parsePacket(int size = 0);
  int packetRssi();
  int packetRssi(uint8_t pkt_snr_raw);
  int currentRssi();
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
  void setFrequency(uint32_t frequency);
  void setSpreadingFactor(int sf);
  uint8_t getSpreadingFactor();
  uint32_t getSignalBandwidth();
  void setSignalBandwidth(uint32_t sbw);
  void setCodingRate4(int denominator);
  uint8_t getCodingRate4();
  void setPreambleLength(long preamble_symbols);
  void setSyncWord(int sw);
  bool dcd();
  void clearIRQStatus();
  void enableCrc();
  void disableCrc();
  void enableTCXO();
  void disableTCXO();

  void txAntEnable();
  void rxAntEnable();
  void loraMode();
  void waitOnBusy();
  void executeOpcode(uint8_t opcode, uint8_t *buffer, uint8_t size);
  void executeOpcodeRead(uint8_t opcode, uint8_t *buffer, uint8_t size);
  void writeBuffer(const uint8_t* buffer, size_t size);
  void readBuffer(uint8_t* buffer, size_t size);
  void setPacketParams(uint32_t target_preamble_symbols, uint8_t headermode, uint8_t payload_length, uint8_t crc);
  void setModulationParams(uint8_t sf, uint8_t bw, uint8_t cr);

  void crc() { enableCrc(); }
  void noCrc() { disableCrc(); }

  void setPins(int ss = LORA_DEFAULT_SS_PIN, int reset = LORA_DEFAULT_RESET_PIN, int dio0 = LORA_DEFAULT_DIO0_PIN, int busy = LORA_DEFAULT_BUSY_PIN, int rxen = LORA_DEFAULT_RXEN_PIN, int txen = LORA_DEFAULT_TXEN_PIN);
  void setSPIFrequency(uint32_t frequency);

  void dumpRegisters(Stream& out);

private:
  void explicitHeaderMode();
  void implicitHeaderMode();

  bool getPacketValidity();
  void handleDio0Rise();

  uint8_t readRegister(uint16_t address);
  void writeRegister(uint16_t address, uint8_t value);
  uint8_t singleTransfer(uint8_t opcode, uint16_t address, uint8_t value);

  static void onDio0Rise();

  void handleLowDataRate();
  void optimizeModemSensitivity();

private:
  SPISettings _spiSettings;
  int _ss;
  int _reset;
  int _dio0;
  int _rxen;
  int _txen;
  int _busy;
  int _modem;
  unsigned long _frequency;
  int _txp;
  uint8_t _sf;
  uint8_t _bw;
  uint8_t _cr;
  int _packetIndex;
  uint32_t _preambleLength;
  int _implicitHeaderMode;
  int _payloadLength;
  int _crcMode;
  int _fifo_tx_addr_ptr;
  int _fifo_rx_addr_ptr;
  uint8_t _packet[256];
  bool _preinit_done;
  bool _tcxo;
  bool _radio_online;
  int _rxPacketLength;
  uint32_t _bitrate;
  void (*_receive_callback)(int);
};

extern sx128x sx128x_modem;

#endif
