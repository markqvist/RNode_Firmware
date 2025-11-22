// Copyright Sandeep Mistry, Mark Qvist and Jacob Eva.
// Licensed under the MIT license.

#ifndef SX1276_H
#define SX1276_H

#include <Arduino.h>
#include <SPI.h>
#include "Modem.h"

#define LORA_DEFAULT_SS_PIN    10
#define LORA_DEFAULT_RESET_PIN 9
#define LORA_DEFAULT_DIO0_PIN  2
#define LORA_DEFAULT_BUSY_PIN  -1

#define PA_OUTPUT_RFO_PIN      0
#define PA_OUTPUT_PA_BOOST_PIN 1

#define RSSI_OFFSET 157

// Modem status flags
#define SIG_DETECT 0x01
#define SIG_SYNCED 0x02
#define RX_ONGOING 0x04

class sx127x : public Stream {
public:
  sx127x();

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

  bool preInit();
  uint8_t getTxPower();
  void setTxPower(int level, int outputPin = PA_OUTPUT_PA_BOOST_PIN);
  uint32_t getFrequency();
  void setFrequency(unsigned long frequency);
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

  byte random();

  void setPins(int ss = LORA_DEFAULT_SS_PIN, int reset = LORA_DEFAULT_RESET_PIN, int dio0 = LORA_DEFAULT_DIO0_PIN, int busy = LORA_DEFAULT_BUSY_PIN);
  void setSPIFrequency(uint32_t frequency);

private:
  void explicitHeaderMode();
  void implicitHeaderMode();

  void handleDio0Rise();

  uint8_t readRegister(uint8_t address);
  void writeRegister(uint8_t address, uint8_t value);
  uint8_t singleTransfer(uint8_t address, uint8_t value);

  static void onDio0Rise();

  void handleLowDataRate();
  void optimizeModemSensitivity();

private:
  SPISettings _spiSettings;
  int _ss;
  int _reset;
  int _dio0;
  int _busy;
  long _frequency;
  int _packetIndex;
  int _implicitHeaderMode;
  bool _preinit_done;
  void (*_onReceive)(int);
};

extern sx127x sx127x_modem;

#endif
