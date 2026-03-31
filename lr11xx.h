// Copyright 2025
// Licensed under the MIT license.

#ifndef LR11XX_H
#define LR11XX_H

#include <Arduino.h>
#include <SPI.h>
#include "Modem.h"

#define LORA_DEFAULT_SS_PIN    10
#define LORA_DEFAULT_RESET_PIN 9
#define LORA_DEFAULT_DIO0_PIN  2
#define LORA_DEFAULT_RXEN_PIN  -1
#define LORA_DEFAULT_BUSY_PIN  -1
#define LORA_MODEM_TIMEOUT_MS 20E3

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
  void reset(void);

  bool preInit();
  uint8_t getTxPower();
  void setTxPower(int level, int outputPin = PA_OUTPUT_PA_BOOST_PIN);
  uint32_t getFrequency();
  void setFrequency(long frequency);
  void setSpreadingFactor(int sf);
  long getSignalBandwidth();
  void setSignalBandwidth(long sbw);
  void setCodingRate4(int denominator);
  void setPreambleLength(long preamble_symbols);
  void setSyncWord(uint8_t sw);
  bool dcd();
  void enableCrc();
  void disableCrc();
  void enableTCXO();
  void disableTCXO();

  void loraMode();
  void waitOnBusy();

  // LR11xx SPI layer (2-byte opcodes, two-phase reads)
  void executeOpcode(uint16_t opcode, uint8_t *buffer, uint8_t size);
  void executeOpcodeRead(uint16_t opcode, uint8_t *buffer, uint8_t size);
  void writeBuffer(const uint8_t* buffer, size_t size);
  void readBuffer(uint8_t* buffer, size_t size);
  void setPacketParams(long preamble_symbols, uint8_t headermode, uint8_t payload_length, uint8_t crc);
  void setModulationParams(uint8_t sf, uint8_t bw, uint8_t cr, int ldro);

  byte random();

  void setPins(int ss = LORA_DEFAULT_SS_PIN, int reset = LORA_DEFAULT_RESET_PIN, int dio0 = LORA_DEFAULT_DIO0_PIN, int busy = LORA_DEFAULT_BUSY_PIN, int rxen = LORA_DEFAULT_RXEN_PIN);
  void setSPIFrequency(uint32_t frequency);

  void dumpRegisters(Stream& out);

private:
  void explicitHeaderMode();
  void implicitHeaderMode();
  void handleDio0Rise();
  static void onDio0Rise();

  void handleLowDataRate();
  void calibrate(void);
  void calibrateImage(long frequency);
  void configureRfSwitch();
  void applyHighAcpWorkaround();
  void setRxBoosted(bool enable);
  void clearIrqFlags(uint32_t mask);

  // LR11xx register access (32-bit addresses)
  uint32_t readRegister32(uint32_t address);
  void writeRegister32(uint32_t address, uint32_t value);

private:
  SPISettings _spiSettings;
  int _ss;
  int _reset;
  int _dio0;      // DIO9 on LR1121 (interrupt pin)
  int _rxen;
  int _busy;      // DIO0 on LR1121 (busy indicator)
  long _frequency;
  int _txp;
  uint8_t _sf;
  uint8_t _bw;
  uint8_t _cr;
  uint8_t _ldro;
  int _packetIndex;
  int _preambleLength;
  int _implicitHeaderMode;
  int _payloadLength;
  int _crcMode;
  int _fifo_rx_addr_ptr;
  uint8_t _packet[255];
  bool _preinit_done;
  uint8_t _lastMiso[6];       // Inline MISO capture from write commands
  uint32_t _preamble_detected_at;
  void (*_onReceive)(int);
};

extern lr11xx lr11xx_modem;

#endif
