// Copyright 2025
// Licensed under the MIT license.

// LR11xx radio driver for RNode Firmware
// Supports LR1121 (and potentially LR1110/LR1120)
// Uses 2-byte opcode SPI protocol with two-phase reads

#include "lr11xx.h"

#if PLATFORM == PLATFORM_ESP32
  #if defined(ESP32) and !defined(CONFIG_IDF_TARGET_ESP32S3)
    #include "soc/rtc_wdt.h"
  #endif
  #define ISR_VECT IRAM_ATTR
#else
  #define ISR_VECT
#endif

// ---- LR11xx System Opcodes (0x01xx) ----
#define OP_GET_STATUS_11XX           0x0100
#define OP_GET_VERSION_11XX          0x0101
#define OP_WRITE_REG_MEM_11XX        0x0105
#define OP_READ_REG_MEM_11XX         0x0106
#define OP_WRITE_BUFFER_11XX         0x0109
#define OP_READ_BUFFER_11XX          0x010A
#define OP_GET_ERRORS_11XX           0x010D
#define OP_CLEAR_ERRORS_11XX         0x010E
#define OP_CALIBRATE_11XX            0x010F
#define OP_SET_REG_MODE_11XX         0x0110
#define OP_CALIBRATE_IMAGE_11XX      0x0111
#define OP_SET_DIO_AS_RF_SWITCH_11XX 0x0112
#define OP_SET_DIO_IRQ_PARAMS_11XX   0x0113
#define OP_CLEAR_IRQ_11XX            0x0114
#define OP_SET_TCXO_MODE_11XX        0x0117
#define OP_SET_SLEEP_11XX            0x011B
#define OP_SET_STANDBY_11XX          0x011C
#define OP_SET_FS_11XX               0x011D

// ---- LR11xx Radio Opcodes (0x02xx) ----
#define OP_GET_RX_BUFFER_STATUS_11XX  0x0203
#define OP_GET_PACKET_STATUS_11XX     0x0204
#define OP_GET_RSSI_INST_11XX         0x0205
#define OP_SET_RX_11XX                0x0209
#define OP_SET_TX_11XX                0x020A
#define OP_SET_RF_FREQUENCY_11XX      0x020B
#define OP_SET_CAD_PARAMS_11XX        0x020D
#define OP_SET_PACKET_TYPE_11XX       0x020E
#define OP_SET_MODULATION_PARAMS_11XX 0x020F
#define OP_SET_PACKET_PARAMS_11XX     0x0210
#define OP_SET_TX_PARAMS_11XX         0x0211
#define OP_SET_RX_TX_FALLBACK_11XX    0x0213
#define OP_SET_PA_CONFIG_11XX         0x0215
#define OP_SET_CAD_11XX               0x0218
#define OP_SET_RX_BOOSTED_11XX        0x0227
#define OP_SET_LORA_SYNC_WORD_11XX    0x022B

// ---- LR11xx IRQ Masks (32-bit) ----
#define IRQ_TX_DONE_11XX            0x00000004  // bit 2
#define IRQ_RX_DONE_11XX            0x00000008  // bit 3
#define IRQ_PREAMBLE_DET_11XX       0x00000010  // bit 4
#define IRQ_SYNC_HEADER_VALID_11XX  0x00000020  // bit 5
#define IRQ_HEADER_ERR_11XX         0x00000040  // bit 6
#define IRQ_CRC_ERR_11XX            0x00000080  // bit 7
#define IRQ_CAD_DONE_11XX           0x00000100  // bit 8
#define IRQ_CAD_DETECTED_11XX       0x00000200  // bit 9
#define IRQ_TIMEOUT_11XX            0x00000400  // bit 10
// Only documented IRQ bits (2-11, 21-25)
#define IRQ_ALL_11XX                0x03E00FFC

// ---- LR11xx Mode Constants ----
#define MODE_STDBY_RC_11XX          0x00
#define MODE_STDBY_XOSC_11XX       0x01
#define MODE_PACKET_TYPE_LORA_11XX  0x02
#define MODE_FALLBACK_STDBY_RC_11XX 0x01

// ---- LR11xx PA Constants ----
#define PA_SEL_LP_11XX              0x00
#define PA_SEL_HP_11XX              0x01
#define PA_SEL_HF_11XX              0x02
#define PA_REG_SUPPLY_VREG_11XX     0x00
#define PA_REG_SUPPLY_VBAT_11XX     0x01

// ---- LR11xx TCXO Voltage ----
#define MODE_TCXO_3_0V_11XX        0x06

// ---- LR11xx Register Addresses ----
#define REG_HIGH_ACP_11XX           0x00F30054

// ---- LR11xx Device Types (from GetVersion) ----
#define DEVICE_LR1121               0x03

// ---- LR11xx Sync Word ----
#define SYNC_WORD_PRIVATE_11XX      0x12

lr11xx::lr11xx() :
  _spiSettings(8E6, MSBFIRST, SPI_MODE0),
  _ss(LORA_DEFAULT_SS_PIN), _reset(LORA_DEFAULT_RESET_PIN),
  _dio0(LORA_DEFAULT_DIO0_PIN), _rxen(LORA_DEFAULT_RXEN_PIN),
  _busy(LORA_DEFAULT_BUSY_PIN),
  _frequency(0), _txp(17), _sf(0x07), _bw(0x04), _cr(0x01), _ldro(0x00),
  _packetIndex(0), _preambleLength(18), _implicitHeaderMode(0),
  _payloadLength(255), _crcMode(1), _fifo_rx_addr_ptr(0),
  _preinit_done(false), _preamble_detected_at(0), _onReceive(NULL)
{
}

// --- SPI Primitives (2-byte opcodes, two-phase reads) ---

void lr11xx::waitOnBusy() {
    unsigned long time = millis();
    if (_busy != -1) {
        while (digitalRead(_busy) == HIGH) {
            if (millis() >= (time + 100)) { break; }
        }
    }
}

void lr11xx::executeOpcode(uint16_t opcode, uint8_t *buffer, uint8_t size) {
    waitOnBusy();
    digitalWrite(_ss, LOW);
    SPI.beginTransaction(_spiSettings);
    // Capture MISO: during writes the chip returns Stat1, Stat2, IrqStatus
    // inline (per user manual Section 3.1). Store for use by handleDio0Rise().
    int misoIdx = 0;
    _lastMiso[misoIdx++] = SPI.transfer((opcode >> 8) & 0xFF);
    _lastMiso[misoIdx++] = SPI.transfer(opcode & 0xFF);
    for (int i = 0; i < size; i++) {
        if (misoIdx < 6) {
            _lastMiso[misoIdx++] = SPI.transfer(buffer[i]);
        } else {
            SPI.transfer(buffer[i]);
        }
    }
    SPI.endTransaction();
    digitalWrite(_ss, HIGH);
}

void lr11xx::executeOpcodeRead(uint16_t opcode, uint8_t *buffer, uint8_t size) {
    waitOnBusy();

    // Phase 1: send command
    digitalWrite(_ss, LOW);
    SPI.beginTransaction(_spiSettings);
    SPI.transfer((opcode >> 8) & 0xFF);
    SPI.transfer(opcode & 0xFF);
    SPI.endTransaction();
    digitalWrite(_ss, HIGH);

    // Wait for chip to finish processing the command
    waitOnBusy();

    // Phase 2: read response
    digitalWrite(_ss, LOW);
    SPI.beginTransaction(_spiSettings);
    SPI.transfer(0x00);  // dummy/status byte
    for (int i = 0; i < size; i++) {
        buffer[i] = SPI.transfer(0x00);
    }
    SPI.endTransaction();
    digitalWrite(_ss, HIGH);
}

uint32_t lr11xx::readRegister32(uint32_t address) {
    uint8_t addr_buf[5];
    addr_buf[0] = (address >> 24) & 0xFF;
    addr_buf[1] = (address >> 16) & 0xFF;
    addr_buf[2] = (address >> 8) & 0xFF;
    addr_buf[3] = address & 0xFF;
    addr_buf[4] = 0x01;

    waitOnBusy();
    digitalWrite(_ss, LOW);
    SPI.beginTransaction(_spiSettings);
    SPI.transfer((OP_READ_REG_MEM_11XX >> 8) & 0xFF);
    SPI.transfer(OP_READ_REG_MEM_11XX & 0xFF);
    for (int i = 0; i < 5; i++) SPI.transfer(addr_buf[i]);
    SPI.endTransaction();
    digitalWrite(_ss, HIGH);

    waitOnBusy();

    uint8_t val[4];
    digitalWrite(_ss, LOW);
    SPI.beginTransaction(_spiSettings);
    SPI.transfer(0x00);
    for (int i = 0; i < 4; i++) val[i] = SPI.transfer(0x00);
    SPI.endTransaction();
    digitalWrite(_ss, HIGH);

    return ((uint32_t)val[0] << 24) | ((uint32_t)val[1] << 16) |
           ((uint32_t)val[2] << 8) | val[3];
}

void lr11xx::writeRegister32(uint32_t address, uint32_t value) {
    uint8_t buf[8];
    buf[0] = (address >> 24) & 0xFF;
    buf[1] = (address >> 16) & 0xFF;
    buf[2] = (address >> 8) & 0xFF;
    buf[3] = address & 0xFF;
    buf[4] = (value >> 24) & 0xFF;
    buf[5] = (value >> 16) & 0xFF;
    buf[6] = (value >> 8) & 0xFF;
    buf[7] = value & 0xFF;
    executeOpcode(OP_WRITE_REG_MEM_11XX, buf, 8);
}

void lr11xx::writeBuffer(const uint8_t* buffer, size_t size) {
    waitOnBusy();
    digitalWrite(_ss, LOW);
    SPI.beginTransaction(_spiSettings);
    SPI.transfer((OP_WRITE_BUFFER_11XX >> 8) & 0xFF);
    SPI.transfer(OP_WRITE_BUFFER_11XX & 0xFF);
    for (size_t i = 0; i < size; i++) {
        SPI.transfer(buffer[i]);
    }
    SPI.endTransaction();
    digitalWrite(_ss, HIGH);
}

void lr11xx::readBuffer(uint8_t* buffer, size_t size) {
    uint8_t cmd_buf[2];
    cmd_buf[0] = _fifo_rx_addr_ptr;
    cmd_buf[1] = (uint8_t)size;

    waitOnBusy();
    digitalWrite(_ss, LOW);
    SPI.beginTransaction(_spiSettings);
    SPI.transfer((OP_READ_BUFFER_11XX >> 8) & 0xFF);
    SPI.transfer(OP_READ_BUFFER_11XX & 0xFF);
    SPI.transfer(cmd_buf[0]);
    SPI.transfer(cmd_buf[1]);
    SPI.endTransaction();
    digitalWrite(_ss, HIGH);

    waitOnBusy();
    digitalWrite(_ss, LOW);
    SPI.beginTransaction(_spiSettings);
    SPI.transfer(0x00);
    for (size_t i = 0; i < size; i++) {
        buffer[i] = SPI.transfer(0x00);
    }
    SPI.endTransaction();
    digitalWrite(_ss, HIGH);
}

void lr11xx::clearIrqFlags(uint32_t mask) {
    uint8_t buf[4];
    buf[0] = (mask >> 24) & 0xFF;
    buf[1] = (mask >> 16) & 0xFF;
    buf[2] = (mask >> 8)  & 0xFF;
    buf[3] =  mask & 0xFF;
    executeOpcode(OP_CLEAR_IRQ_11XX, buf, 4);
}

// --- Lifecycle ---

void lr11xx::setPins(int ss, int reset, int dio0, int busy, int rxen) {
    _ss = ss;
    _reset = reset;
    _dio0 = dio0;
    _busy = busy;
    _rxen = rxen;
}

void lr11xx::reset() {
    if (_reset != -1) {
        pinMode(_reset, OUTPUT);
        digitalWrite(_reset, LOW);
        delay(1);
        digitalWrite(_reset, HIGH);
        delay(10);
    }
}

bool lr11xx::preInit() {
    pinMode(_ss, OUTPUT);
    digitalWrite(_ss, HIGH);

    #if BOARD_MODEL == BOARD_TBEAM_S_LR_V1
      SPI.begin(pin_sclk, pin_miso, pin_mosi, pin_cs);
    #else
      SPI.begin();
    #endif

    reset();

    if (_busy != -1) { pinMode(_busy, INPUT); }

    long start = millis();
    uint8_t device_type = 0;
    while (((millis() - start) < 2000) && (millis() >= start)) {
        uint8_t version_buf[4] = {0};
        executeOpcodeRead(OP_GET_VERSION_11XX, version_buf, 4);
        device_type = version_buf[1];
        if (device_type == DEVICE_LR1121) {
            break;
        }
        delay(100);
    }

    if (device_type != DEVICE_LR1121) {
        return false;
    }

    _preinit_done = true;
    return true;
}

int lr11xx::begin(long frequency) {
    _frequency = frequency;

    reset();

    if (_busy != -1) { pinMode(_busy, INPUT); }

    if (!_preinit_done) {
        if (!preInit()) {
            return 0;
        }
    }

    standby();

    // DC-DC regulator
    uint8_t reg_mode = 0x01;
    executeOpcode(OP_SET_REG_MODE_11XX, &reg_mode, 1);

    configureRfSwitch();
    enableTCXO();

    uint8_t clear_buf[4] = {0};
    executeOpcode(OP_CLEAR_ERRORS_11XX, clear_buf, 4);
    calibrate();
    calibrateImage(frequency);

    loraMode();
    setFrequency(frequency);
    setSyncWord(SYNC_WORD_PRIVATE_11XX);

    uint8_t fallback = MODE_FALLBACK_STDBY_RC_11XX;
    executeOpcode(OP_SET_RX_TX_FALLBACK_11XX, &fallback, 1);

    setModulationParams(_sf, _bw, _cr, _ldro);
    setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
    setTxPower(_txp);
    setRxBoosted(true);
    clearIrqFlags(IRQ_ALL_11XX);

    return 1;
}

void lr11xx::end() {
    sleep();
    SPI.end();
    _preinit_done = false;
}

// --- TX Path ---

int lr11xx::beginPacket(int implicitHeader) {
    standby();
    if (implicitHeader) {
        implicitHeaderMode();
    } else {
        explicitHeaderMode();
    }
    _payloadLength = 0;
    setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
    return 1;
}

int lr11xx::endPacket() {
    setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);

    // Write accumulated packet data to TX buffer in one shot
    writeBuffer(_packet, _payloadLength);

    applyHighAcpWorkaround();

    uint8_t timeout[3] = {0x00, 0x00, 0x00};
    executeOpcode(OP_SET_TX_11XX, timeout, 3);

    // Poll for TX_DONE
    // GetStatus Phase 2 returns: [stat] [IRQ3] [IRQ2] [IRQ1] [IRQ0]
    bool timed_out = false;
    // Use a simple timeout based on payload length
    uint32_t w_timeout = millis() + LORA_MODEM_TIMEOUT_MS;

    while (millis() < w_timeout) {
        uint8_t irq_buf[5] = {0};
        executeOpcodeRead(OP_GET_STATUS_11XX, irq_buf, 5);
        uint32_t irq_status = ((uint32_t)irq_buf[1] << 24) | ((uint32_t)irq_buf[2] << 16) |
                              ((uint32_t)irq_buf[3] << 8)  | irq_buf[4];
        if (irq_status & IRQ_TX_DONE_11XX) break;
        yield();
    }

    if (millis() >= w_timeout) { timed_out = true; }

    clearIrqFlags(IRQ_ALL_11XX);

    return !timed_out;
}

size_t lr11xx::write(uint8_t byte) {
    return write(&byte, sizeof(byte));
}

size_t lr11xx::write(const uint8_t *buffer, size_t size) {
    if ((_payloadLength + size) > 255) {
        size = 255 - _payloadLength;
    }
    // Buffer locally in _packet. LR11xx WriteBuffer8 always writes from offset 0
    // (no auto-incrementing FIFO pointer like SX126x), so we accumulate here and
    // write all at once in endPacket().
    memcpy(_packet + _payloadLength, buffer, size);
    _payloadLength += size;
    return size;
}

// --- RX Path ---

int lr11xx::parsePacket(int size) {
    // Not used in RNode firmware (uses onReceive callback instead)
    return 0;
}

int ISR_VECT lr11xx::available() {
    uint8_t buf[2] = {0};
    executeOpcodeRead(OP_GET_RX_BUFFER_STATUS_11XX, buf, 2);
    return buf[0] - _packetIndex;
}

int ISR_VECT lr11xx::read() {
    if (!available()) { return -1; }

    if (_packetIndex == 0) {
        uint8_t rxbuf[2] = {0};
        executeOpcodeRead(OP_GET_RX_BUFFER_STATUS_11XX, rxbuf, 2);
        int size = rxbuf[0];
        _fifo_rx_addr_ptr = rxbuf[1];
        readBuffer(_packet, size);
    }

    uint8_t byte = _packet[_packetIndex];
    _packetIndex++;
    return byte;
}

int lr11xx::peek() {
    if (!available()) { return -1; }

    if (_packetIndex == 0) {
        uint8_t rxbuf[2] = {0};
        executeOpcodeRead(OP_GET_RX_BUFFER_STATUS_11XX, rxbuf, 2);
        int size = rxbuf[0];
        _fifo_rx_addr_ptr = rxbuf[1];
        readBuffer(_packet, size);
    }

    return _packet[_packetIndex];
}

void lr11xx::flush() {
}

void lr11xx::onReceive(void(*callback)(int)) {
    _onReceive = callback;

    if (callback) {
        pinMode(_dio0, INPUT);

        // Route only RX_DONE to DIO9
        uint8_t irq_buf[8];
        uint32_t irq_mask = IRQ_RX_DONE_11XX;
        irq_buf[0] = (irq_mask >> 24) & 0xFF;
        irq_buf[1] = (irq_mask >> 16) & 0xFF;
        irq_buf[2] = (irq_mask >> 8)  & 0xFF;
        irq_buf[3] =  irq_mask & 0xFF;
        irq_buf[4] = (irq_mask >> 24) & 0xFF;
        irq_buf[5] = (irq_mask >> 16) & 0xFF;
        irq_buf[6] = (irq_mask >> 8)  & 0xFF;
        irq_buf[7] =  irq_mask & 0xFF;
        executeOpcode(OP_SET_DIO_IRQ_PARAMS_11XX, irq_buf, 8);

        attachInterrupt(digitalPinToInterrupt(_dio0), lr11xx::onDio0Rise, RISING);
    } else {
        detachInterrupt(digitalPinToInterrupt(_dio0));
    }
}

void lr11xx::receive(int size) {
    if (size > 0) {
        implicitHeaderMode();
        _payloadLength = size;
    } else {
        explicitHeaderMode();
        _payloadLength = 0;
    }
    setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);

    clearIrqFlags(IRQ_ALL_11XX);

    uint8_t mode[3] = {0xFF, 0xFF, 0xFF};  // continuous RX
    executeOpcode(OP_SET_RX_11XX, mode, 3);
}

// Named handleDio0Rise for consistency with the sx126x driver convention.
// On the LR1121 this actually handles DIO9 (the primary interrupt pin),
// not DIO0 (which is the BUSY signal on LR11xx).
void ISR_VECT lr11xx::handleDio0Rise() {
    // ClearIrq is a single-phase write command (ISR-safe). The chip returns
    // Stat1, Stat2, and IrqStatus inline on MISO during the transaction
    // (per user manual Section 3.1), captured by executeOpcode into _lastMiso.
    clearIrqFlags(IRQ_RX_DONE_11XX | IRQ_CRC_ERR_11XX | IRQ_HEADER_ERR_11XX);

    uint32_t irq = ((uint32_t)_lastMiso[2] << 24) | ((uint32_t)_lastMiso[3] << 16) |
                   ((uint32_t)_lastMiso[4] << 8)  | _lastMiso[5];

    // Reject if header CRC (bit 6) or payload CRC (bit 7) failed
    if ((irq & (IRQ_CRC_ERR_11XX | IRQ_HEADER_ERR_11XX)) == 0) {
        // Reset read position so read()/peek() fetch from the start of the new packet
        _packetIndex = 0;

        uint8_t rxbuf[2] = {0};
        executeOpcodeRead(OP_GET_RX_BUFFER_STATUS_11XX, rxbuf, 2);
        int packetLength = rxbuf[0];
        _fifo_rx_addr_ptr = rxbuf[1];

        // Guard against spurious interrupts delivering zero-length packets
        if (packetLength > 0 && _onReceive) {
            _onReceive(packetLength);
        }
    }

}

void ISR_VECT lr11xx::onDio0Rise() {
    lr11xx_modem.handleDio0Rise();
}

// --- Modem Control ---

void lr11xx::standby() {
    uint8_t mode = MODE_STDBY_RC_11XX;
    executeOpcode(OP_SET_STANDBY_11XX, &mode, 1);
}

void lr11xx::sleep() {
    uint8_t buf[5] = {0};
    buf[0] = 0x01;
    executeOpcode(OP_SET_SLEEP_11XX, buf, 5);
}

// --- RF Configuration ---

uint32_t lr11xx::getFrequency() {
    return _frequency;
}

void lr11xx::setFrequency(long frequency) {
    _frequency = frequency;
    uint8_t buf[4];
    buf[0] = ((uint32_t)frequency >> 24) & 0xFF;
    buf[1] = ((uint32_t)frequency >> 16) & 0xFF;
    buf[2] = ((uint32_t)frequency >> 8) & 0xFF;
    buf[3] = (uint32_t)frequency & 0xFF;
    executeOpcode(OP_SET_RF_FREQUENCY_11XX, buf, 4);
}

void lr11xx::setSpreadingFactor(int sf) {
    if (sf < 5) sf = 5;
    else if (sf > 12) sf = 12;
    _sf = sf;
    handleLowDataRate();
    setModulationParams(sf, _bw, _cr, _ldro);
}

long lr11xx::getSignalBandwidth() {
    switch (_bw) {
        case 0x03: return 62.5E3;
        case 0x04: return 125E3;
        case 0x05: return 250E3;
        case 0x06: return 500E3;
    }
    return 0;
}

void lr11xx::setSignalBandwidth(long sbw) {
    if (sbw <= 62.5E3) {
        _bw = 0x03;
    } else if (sbw <= 125E3) {
        _bw = 0x04;
    } else if (sbw <= 250E3) {
        _bw = 0x05;
    } else {
        _bw = 0x06;
    }
    handleLowDataRate();
    setModulationParams(_sf, _bw, _cr, _ldro);
}

void lr11xx::setCodingRate4(int denominator) {
    if (denominator < 5) denominator = 5;
    else if (denominator > 8) denominator = 8;
    _cr = denominator - 4;
    setModulationParams(_sf, _bw, _cr, _ldro);
}

void lr11xx::setPreambleLength(long length) {
    _preambleLength = length;
    setPacketParams(length, _implicitHeaderMode, _payloadLength, _crcMode);
}

// --- Power Control ---

uint8_t lr11xx::getTxPower() {
    return _txp;
}

void lr11xx::setTxPower(int level, int outputPin) {
    uint8_t pa_buf[4];
    pa_buf[0] = PA_SEL_HP_11XX;
    pa_buf[1] = PA_REG_SUPPLY_VBAT_11XX;
    pa_buf[2] = 0x04;
    pa_buf[3] = 0x07;
    executeOpcode(OP_SET_PA_CONFIG_11XX, pa_buf, 4);

    if (level > 22) level = 22;
    else if (level < -9) level = -9;
    _txp = level;

    uint8_t tx_buf[2];
    tx_buf[0] = (uint8_t)level;
    tx_buf[1] = 0x02;
    executeOpcode(OP_SET_TX_PARAMS_11XX, tx_buf, 2);
}

// --- Signal Quality ---

uint8_t lr11xx::currentRssiRaw() {
    uint8_t byte = 0;
    executeOpcodeRead(OP_GET_RSSI_INST_11XX, &byte, 1);
    return byte;
}

int lr11xx::currentRssi() {
    uint8_t byte = 0;
    executeOpcodeRead(OP_GET_RSSI_INST_11XX, &byte, 1);
    return -(int(byte)) / 2;
}

uint8_t lr11xx::packetRssiRaw() {
    uint8_t buf[3] = {0};
    executeOpcodeRead(OP_GET_PACKET_STATUS_11XX, buf, 3);
    return buf[0];
}

int lr11xx::packetRssi() {
    uint8_t buf[3] = {0};
    executeOpcodeRead(OP_GET_PACKET_STATUS_11XX, buf, 3);
    return -buf[0] / 2;
}

int lr11xx::packetRssi(uint8_t pkt_snr_raw) {
    return packetRssi();
}

uint8_t ISR_VECT lr11xx::packetSnrRaw() {
    uint8_t buf[3] = {0};
    executeOpcodeRead(OP_GET_PACKET_STATUS_11XX, buf, 3);
    return buf[1];
}

float lr11xx::packetSnr() {
    uint8_t buf[3] = {0};
    executeOpcodeRead(OP_GET_PACKET_STATUS_11XX, buf, 3);
    return float((int8_t)buf[1]) * 0.25;
}

long lr11xx::packetFrequencyError() {
    return 0;
}

// --- Channel Monitoring ---

bool lr11xx::dcd() {
    uint8_t irq_buf[5] = {0};
    executeOpcodeRead(OP_GET_STATUS_11XX, irq_buf, 5);
    // GetStatus returns extra status byte at position 0, IRQ flags at bytes 1-4
    uint32_t irq = ((uint32_t)irq_buf[1] << 24) | ((uint32_t)irq_buf[2] << 16) |
                   ((uint32_t)irq_buf[3] << 8)  | irq_buf[4];
    uint32_t now = millis();

    bool preamble = irq & IRQ_PREAMBLE_DET_11XX;
    bool header   = irq & IRQ_SYNC_HEADER_VALID_11XX;
    bool carrier_detected = false;
    bool false_preamble = false;

    // Header without preamble is a stranded flag from a previous detection
    // where ClearIrq cleared the preamble but not the header. Clear it to
    // prevent permanent carrier detection deadlock.
    if (header && !preamble) {
        clearIrqFlags(IRQ_SYNC_HEADER_VALID_11XX);
    } else if (header && preamble) {
        carrier_detected = true;
    }

    if (preamble) {
        carrier_detected = true;
        if (_preamble_detected_at == 0) { _preamble_detected_at = now; }
        if (now - _preamble_detected_at > (uint32_t)(_preambleLength * (1 << _sf) / (getSignalBandwidth() / 1000) + 8 * (1 << _sf) / (getSignalBandwidth() / 1000))) {
            _preamble_detected_at = 0;
            if (!header) {
                false_preamble = true;
                clearIrqFlags(IRQ_PREAMBLE_DET_11XX | IRQ_SYNC_HEADER_VALID_11XX);
            } else {
                clearIrqFlags(IRQ_PREAMBLE_DET_11XX);
            }
        }
    }

    // Note: unlike SX126x, we don't call receive() on false preamble.
    // The LR11xx stays in continuous RX mode and DIO9 is re-armed in
    // handleDio0Rise() via SetRx. Calling receive() here would disrupt
    // any in-progress reception because dcd() runs inside a critical
    // section during active packet reception.

    return carrier_detected;
}

// --- CRC ---

void lr11xx::enableCrc() {
    _crcMode = 1;
    setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
}

void lr11xx::disableCrc() {
    _crcMode = 0;
    setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
}

// --- TCXO ---

void lr11xx::enableTCXO() {
    uint8_t buf[4];
    buf[0] = MODE_TCXO_3_0V_11XX;
    buf[1] = 0x00;
    buf[2] = 0x00;
    buf[3] = 0xFF;
    executeOpcode(OP_SET_TCXO_MODE_11XX, buf, 4);
}

void lr11xx::disableTCXO() {
}

// --- Sync Word ---

void lr11xx::setSyncWord(uint8_t sw) {
    executeOpcode(OP_SET_LORA_SYNC_WORD_11XX, &sw, 1);
}

// --- Misc ---

byte lr11xx::random() {
    return currentRssiRaw();
}

void lr11xx::setSPIFrequency(uint32_t frequency) {
    _spiSettings = SPISettings(frequency, MSBFIRST, SPI_MODE0);
}

void lr11xx::dumpRegisters(Stream& out) {
}

// --- LR11xx-specific internal methods ---

void lr11xx::loraMode() {
    uint8_t mode = MODE_PACKET_TYPE_LORA_11XX;
    executeOpcode(OP_SET_PACKET_TYPE_11XX, &mode, 1);
}

void lr11xx::setModulationParams(uint8_t sf, uint8_t bw, uint8_t cr, int ldro) {
    uint8_t buf[4];
    buf[0] = sf;
    buf[1] = bw;
    buf[2] = cr;
    buf[3] = ldro;
    executeOpcode(OP_SET_MODULATION_PARAMS_11XX, buf, 4);
}

void lr11xx::setPacketParams(long preamble, uint8_t headermode, uint8_t length, uint8_t crc) {
    uint8_t buf[6];
    buf[0] = (preamble >> 8) & 0xFF;
    buf[1] = preamble & 0xFF;
    buf[2] = headermode;
    buf[3] = length;
    buf[4] = crc;
    buf[5] = 0x00;
    executeOpcode(OP_SET_PACKET_PARAMS_11XX, buf, 6);
}

void lr11xx::handleLowDataRate() {
    if (long((1 << _sf) / (getSignalBandwidth() / 1000)) > 16) {
        _ldro = 0x01;
    } else {
        _ldro = 0x00;
    }
}

void lr11xx::calibrate() {
    uint8_t mode = MODE_STDBY_RC_11XX;
    executeOpcode(OP_SET_STANDBY_11XX, &mode, 1);
    uint8_t cal = 0x3F;
    executeOpcode(OP_CALIBRATE_11XX, &cal, 1);
    delay(5);
    waitOnBusy();
}

void lr11xx::calibrateImage(long frequency) {
    uint8_t image_freq[2] = {0};
    if      (frequency >= 430E6 && frequency <= 440E6) { image_freq[0] = 0x6B; image_freq[1] = 0x6F; }
    else if (frequency >= 470E6 && frequency <= 510E6) { image_freq[0] = 0x75; image_freq[1] = 0x81; }
    else if (frequency >= 779E6 && frequency <= 787E6) { image_freq[0] = 0xC1; image_freq[1] = 0xC5; }
    else if (frequency >= 863E6 && frequency <= 870E6) { image_freq[0] = 0xD7; image_freq[1] = 0xDB; }
    else if (frequency >= 902E6 && frequency <= 928E6) { image_freq[0] = 0xE1; image_freq[1] = 0xE9; }
    executeOpcode(OP_CALIBRATE_IMAGE_11XX, image_freq, 2);
    waitOnBusy();
}

void lr11xx::configureRfSwitch() {
    uint8_t buf[8];
    buf[0] = 0x03;  // enable DIO5 (bit 0) + DIO6 (bit 1)
    buf[1] = 0x00;  // standby
    buf[2] = 0x01;  // RX: DIO5=HIGH, DIO6=LOW
    buf[3] = 0x02;  // TX: DIO5=LOW, DIO6=HIGH
    buf[4] = 0x02;  // TX_HP
    buf[5] = 0x00;  // TX_HF
    buf[6] = 0x00;  // GNSS
    buf[7] = 0x00;  // WiFi
    executeOpcode(OP_SET_DIO_AS_RF_SWITCH_11XX, buf, 8);
}

void lr11xx::applyHighAcpWorkaround() {
    uint32_t val = readRegister32(REG_HIGH_ACP_11XX);
    val &= ~(1UL << 30);
    writeRegister32(REG_HIGH_ACP_11XX, val);
}

void lr11xx::setRxBoosted(bool enable) {
    uint8_t val = enable ? 0x01 : 0x00;
    executeOpcode(OP_SET_RX_BOOSTED_11XX, &val, 1);
}

void lr11xx::explicitHeaderMode() {
    _implicitHeaderMode = 0;
    setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
}

void lr11xx::implicitHeaderMode() {
    _implicitHeaderMode = 1;
    setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
}

lr11xx lr11xx_modem;
