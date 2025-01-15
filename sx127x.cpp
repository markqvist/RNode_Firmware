// Copyright Sandeep Mistry, Mark Qvist and Jacob Eva.
// Licensed under the MIT license.

#include "Boards.h"

#if MODEM == SX1276
#include "sx127x.h"

#if MCU_VARIANT == MCU_ESP32
  #if MCU_VARIANT == MCU_ESP32 and !defined(CONFIG_IDF_TARGET_ESP32S3)
    #include "hal/wdt_hal.h"
  #endif
  #define ISR_VECT IRAM_ATTR
#else
  #define ISR_VECT
#endif

// Registers
#define REG_FIFO_7X                   0x00
#define REG_OP_MODE_7X                0x01
#define REG_FRF_MSB_7X                0x06
#define REG_FRF_MID_7X                0x07
#define REG_FRF_LSB_7X                0x08
#define REG_PA_CONFIG_7X              0x09
#define REG_OCP_7X                    0x0b
#define REG_LNA_7X                    0x0c
#define REG_FIFO_ADDR_PTR_7X          0x0d
#define REG_FIFO_TX_BASE_ADDR_7X      0x0e
#define REG_FIFO_RX_BASE_ADDR_7X      0x0f
#define REG_FIFO_RX_CURRENT_ADDR_7X   0x10
#define REG_IRQ_FLAGS_7X              0x12
#define REG_RX_NB_BYTES_7X            0x13
#define REG_MODEM_STAT_7X             0x18
#define REG_PKT_SNR_VALUE_7X          0x19
#define REG_PKT_RSSI_VALUE_7X         0x1a
#define REG_RSSI_VALUE_7X             0x1b
#define REG_MODEM_CONFIG_1_7X         0x1d
#define REG_MODEM_CONFIG_2_7X         0x1e
#define REG_PREAMBLE_MSB_7X           0x20
#define REG_PREAMBLE_LSB_7X           0x21
#define REG_PAYLOAD_LENGTH_7X         0x22
#define REG_MODEM_CONFIG_3_7X         0x26
#define REG_FREQ_ERROR_MSB_7X         0x28
#define REG_FREQ_ERROR_MID_7X         0x29
#define REG_FREQ_ERROR_LSB_7X         0x2a
#define REG_RSSI_WIDEBAND_7X          0x2c
#define REG_DETECTION_OPTIMIZE_7X     0x31
#define REG_HIGH_BW_OPTIMIZE_1_7X     0x36
#define REG_DETECTION_THRESHOLD_7X    0x37
#define REG_SYNC_WORD_7X              0x39
#define REG_HIGH_BW_OPTIMIZE_2_7X     0x3a
#define REG_DIO_MAPPING_1_7X          0x40
#define REG_VERSION_7X                0x42
#define REG_TCXO_7X                   0x4b
#define REG_PA_DAC_7X                 0x4d

// Modes
#define MODE_LONG_RANGE_MODE_7X       0x80
#define MODE_SLEEP_7X                 0x00
#define MODE_STDBY_7X                 0x01
#define MODE_TX_7X                    0x03
#define MODE_RX_CONTINUOUS_7X         0x05
#define MODE_RX_SINGLE_7X             0x06

// PA config
#define PA_BOOST_7X                   0x80

// IRQ masks
#define IRQ_TX_DONE_MASK_7X           0x08
#define IRQ_RX_DONE_MASK_7X           0x40
#define IRQ_PAYLOAD_CRC_ERROR_MASK_7X 0x20

#define SYNC_WORD_7X                  0x12
#define MAX_PKT_LENGTH                255

extern SPIClass SPI;

sx127x::sx127x() :
  _spiSettings(8E6, MSBFIRST, SPI_MODE0),
  _ss(LORA_DEFAULT_SS_PIN), _reset(LORA_DEFAULT_RESET_PIN), _dio0(LORA_DEFAULT_DIO0_PIN),
  _frequency(0), _packetIndex(0), _preinit_done(false), _onReceive(NULL) { setTimeout(0); }

void sx127x::setSPIFrequency(uint32_t frequency) { _spiSettings = SPISettings(frequency, MSBFIRST, SPI_MODE0); }
void sx127x::setPins(int ss, int reset, int dio0, int busy) { _ss = ss; _reset = reset; _dio0 = dio0; _busy = busy; }
uint8_t ISR_VECT sx127x::readRegister(uint8_t address) { return singleTransfer(address & 0x7f, 0x00); }
void sx127x::writeRegister(uint8_t address, uint8_t value) { singleTransfer(address | 0x80, value); }
void sx127x::standby() { writeRegister(REG_OP_MODE_7X, MODE_LONG_RANGE_MODE_7X | MODE_STDBY_7X); }
void sx127x::sleep() { writeRegister(REG_OP_MODE_7X, MODE_LONG_RANGE_MODE_7X | MODE_SLEEP_7X); }
void sx127x::setSyncWord(uint8_t sw) { writeRegister(REG_SYNC_WORD_7X, sw); }
void sx127x::enableCrc() { writeRegister(REG_MODEM_CONFIG_2_7X, readRegister(REG_MODEM_CONFIG_2_7X) | 0x04); }
void sx127x::disableCrc() { writeRegister(REG_MODEM_CONFIG_2_7X, readRegister(REG_MODEM_CONFIG_2_7X) & 0xfb); }
void sx127x::enableTCXO() { uint8_t tcxo_reg = readRegister(REG_TCXO_7X); writeRegister(REG_TCXO_7X, tcxo_reg | 0x10); }
void sx127x::disableTCXO() { uint8_t tcxo_reg = readRegister(REG_TCXO_7X); writeRegister(REG_TCXO_7X, tcxo_reg & 0xEF); }
void sx127x::explicitHeaderMode() { _implicitHeaderMode = 0; writeRegister(REG_MODEM_CONFIG_1_7X, readRegister(REG_MODEM_CONFIG_1_7X) & 0xfe); }
void sx127x::implicitHeaderMode() { _implicitHeaderMode = 1; writeRegister(REG_MODEM_CONFIG_1_7X, readRegister(REG_MODEM_CONFIG_1_7X) | 0x01); }
byte sx127x::random() { return readRegister(REG_RSSI_WIDEBAND_7X); }
void sx127x::flush() { }

bool sx127x::preInit() {
  pinMode(_ss, OUTPUT);
  digitalWrite(_ss, HIGH);
  
  #if BOARD_MODEL == BOARD_T3S3
    SPI.begin(pin_sclk, pin_miso, pin_mosi, pin_cs);
  #else
    SPI.begin();
  #endif

  // Check modem version
  uint8_t version;
  long start = millis();
  while (((millis() - start) < 500) && (millis() >= start)) {
      version = readRegister(REG_VERSION_7X);
      if (version == 0x12) { break; }
      delay(100);
  }

  if (version != 0x12) { return false; }
  _preinit_done = true;
  return true;
}

uint8_t ISR_VECT sx127x::singleTransfer(uint8_t address, uint8_t value) {
  uint8_t response;

  digitalWrite(_ss, LOW);
  SPI.beginTransaction(_spiSettings);
  SPI.transfer(address);
  response = SPI.transfer(value);
  SPI.endTransaction();
  digitalWrite(_ss, HIGH);

  return response;
}

int sx127x::begin(long frequency) {
  if (_reset != -1) {
    pinMode(_reset, OUTPUT);
    digitalWrite(_reset, LOW);
    delay(10);
    digitalWrite(_reset, HIGH);
    delay(10);
  }

  if (_busy != -1) { pinMode(_busy, INPUT); }
  if (!_preinit_done) { if (!preInit()) { return false; } }

  sleep();
  setFrequency(frequency);

  // Set base addresses
  writeRegister(REG_FIFO_TX_BASE_ADDR_7X, 0);
  writeRegister(REG_FIFO_RX_BASE_ADDR_7X, 0);

  // Set LNA boost and auto AGC
  writeRegister(REG_LNA_7X, readRegister(REG_LNA_7X) | 0x03);
  writeRegister(REG_MODEM_CONFIG_3_7X, 0x04);

  setSyncWord(SYNC_WORD_7X);
  enableCrc();
  setTxPower(2);

  standby();

  return 1;
}

void sx127x::end() { sleep(); SPI.end(); _preinit_done = false; }

int sx127x::beginPacket(int implicitHeader) {
  standby();

  if (implicitHeader) { implicitHeaderMode(); }
  else { explicitHeaderMode(); }

  // Reset FIFO address and payload length
  writeRegister(REG_FIFO_ADDR_PTR_7X, 0);
  writeRegister(REG_PAYLOAD_LENGTH_7X, 0);

  return 1;
}

int sx127x::endPacket() {
  // Enter TX mode
  writeRegister(REG_OP_MODE_7X, MODE_LONG_RANGE_MODE_7X | MODE_TX_7X);

  // Wait for TX completion
  while ((readRegister(REG_IRQ_FLAGS_7X) & IRQ_TX_DONE_MASK_7X) == 0) {
    yield();
  }

  // Clear TX complete IRQ
  writeRegister(REG_IRQ_FLAGS_7X, IRQ_TX_DONE_MASK_7X);
  return 1;
}

bool sx127x::dcd() {
  bool carrier_detected = false;
  uint8_t status = readRegister(REG_MODEM_STAT_7X);
  if ((status & SIG_DETECT) == SIG_DETECT) { carrier_detected = true; }
  if ((status & SIG_SYNCED) == SIG_SYNCED) { carrier_detected = true; }
  return carrier_detected;
}

uint8_t sx127x::currentRssiRaw() {
    uint8_t rssi = readRegister(REG_RSSI_VALUE_7X);
    return rssi;
}

int ISR_VECT sx127x::currentRssi() {
    int rssi = (int)readRegister(REG_RSSI_VALUE_7X) - RSSI_OFFSET;
    if (_frequency < 820E6) rssi -= 7;
    return rssi;
}

uint8_t sx127x::packetRssiRaw() {
    uint8_t pkt_rssi_value = readRegister(REG_PKT_RSSI_VALUE_7X);
    return pkt_rssi_value;
}

int ISR_VECT sx127x::packetRssi(uint8_t pkt_snr_raw) {
  int pkt_rssi = (int)readRegister(REG_PKT_RSSI_VALUE_7X) - RSSI_OFFSET;
  int pkt_snr = ((int8_t)pkt_snr_raw)*0.25;

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
}

int ISR_VECT sx127x::packetRssi() {
  int pkt_rssi = (int)readRegister(REG_PKT_RSSI_VALUE_7X) - RSSI_OFFSET;
  int pkt_snr = packetSnr();

  if (_frequency < 820E6) pkt_rssi -= 7;

  if (pkt_snr < 0) { pkt_rssi += pkt_snr; }
  else {
      // Slope correction is (16/15)*pkt_rssi,
      // this estimation looses one floating point
      // operation, and should be precise enough.
      pkt_rssi = (int)(1.066 * pkt_rssi);
  }
  return pkt_rssi;
}

uint8_t ISR_VECT sx127x::packetSnrRaw() { return readRegister(REG_PKT_SNR_VALUE_7X); }

float ISR_VECT sx127x::packetSnr() { return ((int8_t)readRegister(REG_PKT_SNR_VALUE_7X)) * 0.25; }

long sx127x::packetFrequencyError() {
  int32_t freqError = 0;
  freqError = static_cast<int32_t>(readRegister(REG_FREQ_ERROR_MSB_7X) & B111);
  freqError <<= 8L;
  freqError += static_cast<int32_t>(readRegister(REG_FREQ_ERROR_MID_7X));
  freqError <<= 8L;
  freqError += static_cast<int32_t>(readRegister(REG_FREQ_ERROR_LSB_7X));

  if (readRegister(REG_FREQ_ERROR_MSB_7X) & B1000) { // Sign bit is on
      freqError -= 524288; // B1000'0000'0000'0000'0000
  }

  const float fXtal = 32E6; // FXOSC: crystal oscillator (XTAL) frequency (2.5. Chip Specification, p. 14)
  const float fError = ((static_cast<float>(freqError) * (1L << 24)) / fXtal) * (getSignalBandwidth() / 500000.0f);

  return static_cast<long>(fError);
}

size_t sx127x::write(uint8_t byte) { return write(&byte, sizeof(byte)); }

size_t sx127x::write(const uint8_t *buffer, size_t size) {
  int currentLength = readRegister(REG_PAYLOAD_LENGTH_7X);
  if ((currentLength + size) > MAX_PKT_LENGTH) { size = MAX_PKT_LENGTH - currentLength; }

  for (size_t i = 0; i < size; i++) { writeRegister(REG_FIFO_7X, buffer[i]); }
  writeRegister(REG_PAYLOAD_LENGTH_7X, currentLength + size);

  return size;
}

int ISR_VECT sx127x::available() { return (readRegister(REG_RX_NB_BYTES_7X) - _packetIndex); }

int ISR_VECT sx127x::read() {
  if (!available()) { return -1; }
  _packetIndex++;
  return readRegister(REG_FIFO_7X);
}

int sx127x::peek() {
  if (!available()) { return -1; }

  // Remember current FIFO address, read, and then reset address
  int currentAddress = readRegister(REG_FIFO_ADDR_PTR_7X);
  uint8_t b = readRegister(REG_FIFO_7X);
  writeRegister(REG_FIFO_ADDR_PTR_7X, currentAddress);

  return b;
}

void sx127x::onReceive(void(*callback)(int)) {
  _onReceive = callback;

  if (callback) {
    pinMode(_dio0, INPUT);
    writeRegister(REG_DIO_MAPPING_1_7X, 0x00);
    
    #ifdef SPI_HAS_NOTUSINGINTERRUPT
      SPI.usingInterrupt(digitalPinToInterrupt(_dio0));
    #endif
    
    attachInterrupt(digitalPinToInterrupt(_dio0), sx127x::onDio0Rise, RISING);
  
  } else {
    detachInterrupt(digitalPinToInterrupt(_dio0));
    
    #ifdef SPI_HAS_NOTUSINGINTERRUPT
      SPI.notUsingInterrupt(digitalPinToInterrupt(_dio0));
    #endif
  }
}

void sx127x::receive(int size) {
  if (size > 0) {
    implicitHeaderMode();
    writeRegister(REG_PAYLOAD_LENGTH_7X, size & 0xff);
  } else { explicitHeaderMode(); }

  writeRegister(REG_OP_MODE_7X, MODE_LONG_RANGE_MODE_7X | MODE_RX_CONTINUOUS_7X);
}

void sx127x::setTxPower(int level, int outputPin) {
  // Setup according to RFO or PA_BOOST output pin
  if (PA_OUTPUT_RFO_PIN == outputPin) {
    if (level < 0) { level = 0; }
    else if (level > 14) { level = 14; }

    writeRegister(REG_PA_DAC_7X, 0x84);
    writeRegister(REG_PA_CONFIG_7X, 0x70 | level);

  } else {
    if (level < 2) { level = 2; }
    else if (level > 17) { level = 17; }

    writeRegister(REG_PA_DAC_7X, 0x84);
    writeRegister(REG_PA_CONFIG_7X, PA_BOOST_7X | (level - 2));
  }
}

uint8_t sx127x::getTxPower() { byte txp = readRegister(REG_PA_CONFIG_7X); return txp; }

void sx127x::setFrequency(unsigned long frequency) {
  _frequency = frequency;
  uint32_t frf = ((uint64_t)frequency << 19) / 32000000;

  writeRegister(REG_FRF_MSB_7X, (uint8_t)(frf >> 16));
  writeRegister(REG_FRF_MID_7X, (uint8_t)(frf >> 8));
  writeRegister(REG_FRF_LSB_7X, (uint8_t)(frf >> 0));

  optimizeModemSensitivity();
}

uint32_t sx127x::getFrequency() {
  uint8_t msb = readRegister(REG_FRF_MSB_7X);
  uint8_t mid = readRegister(REG_FRF_MID_7X);
  uint8_t lsb = readRegister(REG_FRF_LSB_7X);

  uint32_t frf = ((uint32_t)msb << 16) | ((uint32_t)mid << 8) | (uint32_t)lsb;
  uint64_t frm = (uint64_t)frf*32000000;
  uint32_t frequency = (frm >> 19);

  return frequency;
}

void sx127x::setSpreadingFactor(int sf) {
  if (sf < 6) { sf = 6; }
  else if (sf > 12) { sf = 12; }

  if (sf == 6) {
    writeRegister(REG_DETECTION_OPTIMIZE_7X, 0xc5);
    writeRegister(REG_DETECTION_THRESHOLD_7X, 0x0c);
  } else {
    writeRegister(REG_DETECTION_OPTIMIZE_7X, 0xc3);
    writeRegister(REG_DETECTION_THRESHOLD_7X, 0x0a);
  }

  writeRegister(REG_MODEM_CONFIG_2_7X, (readRegister(REG_MODEM_CONFIG_2_7X) & 0x0f) | ((sf << 4) & 0xf0));
  handleLowDataRate();
}

long sx127x::getSignalBandwidth() {
  byte bw = (readRegister(REG_MODEM_CONFIG_1_7X) >> 4);
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
    case 9: return 500E3; }

  return 0;
}

void sx127x::setSignalBandwidth(long sbw) {
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

  writeRegister(REG_MODEM_CONFIG_1_7X, (readRegister(REG_MODEM_CONFIG_1_7X) & 0x0f) | (bw << 4));
  handleLowDataRate();
  optimizeModemSensitivity();
}

void sx127x::setCodingRate4(int denominator) {
  if (denominator < 5) { denominator = 5; }
  else if (denominator > 8) { denominator = 8; }
  int cr = denominator - 4;
  writeRegister(REG_MODEM_CONFIG_1_7X, (readRegister(REG_MODEM_CONFIG_1_7X) & 0xf1) | (cr << 1));
}

void sx127x::setPreambleLength(long preamble_symbols) {
  long length = preamble_symbols - 4;
  writeRegister(REG_PREAMBLE_MSB_7X, (uint8_t)(length >> 8));
  writeRegister(REG_PREAMBLE_LSB_7X, (uint8_t)(length >> 0));
}

extern bool lora_low_datarate;
void sx127x::handleLowDataRate() {
  int sf = (readRegister(REG_MODEM_CONFIG_2_7X) >> 4);
  if ( long( (1<<sf) / (getSignalBandwidth()/1000)) > 16) {
    // Set auto AGC and LowDataRateOptimize
    writeRegister(REG_MODEM_CONFIG_3_7X, (1<<3)|(1<<2));
    lora_low_datarate = true;
  } else {
    // Only set auto AGC
    writeRegister(REG_MODEM_CONFIG_3_7X, (1<<2));
    lora_low_datarate = false;
  }
}

void sx127x::optimizeModemSensitivity() {
  byte bw = (readRegister(REG_MODEM_CONFIG_1_7X) >> 4);
  uint32_t freq = getFrequency();

  if (bw == 9 && (410E6 <= freq) && (freq <= 525E6)) {
    writeRegister(REG_HIGH_BW_OPTIMIZE_1_7X, 0x02);
    writeRegister(REG_HIGH_BW_OPTIMIZE_2_7X, 0x7f);
  } else if (bw == 9 && (820E6 <= freq) && (freq <= 1020E6)) {
    writeRegister(REG_HIGH_BW_OPTIMIZE_1_7X, 0x02);
    writeRegister(REG_HIGH_BW_OPTIMIZE_2_7X, 0x64);
  } else {
    writeRegister(REG_HIGH_BW_OPTIMIZE_1_7X, 0x03);
  }
}

void ISR_VECT sx127x::handleDio0Rise() {
  int irqFlags = readRegister(REG_IRQ_FLAGS_7X);

  // Clear IRQs
  writeRegister(REG_IRQ_FLAGS_7X, irqFlags);
  if ((irqFlags & IRQ_PAYLOAD_CRC_ERROR_MASK_7X) == 0) {
    _packetIndex = 0;
    int packetLength = _implicitHeaderMode ? readRegister(REG_PAYLOAD_LENGTH_7X) : readRegister(REG_RX_NB_BYTES_7X);
    writeRegister(REG_FIFO_ADDR_PTR_7X, readRegister(REG_FIFO_RX_CURRENT_ADDR_7X));
    if (_onReceive) { _onReceive(packetLength); }
    writeRegister(REG_FIFO_ADDR_PTR_7X, 0);
  }
}

void ISR_VECT sx127x::onDio0Rise() { sx127x_modem.handleDio0Rise(); }

sx127x sx127x_modem;

#endif