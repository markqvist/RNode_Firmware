// E-Ink display driver for LCMEN2R13EFC1
// Adapted from Meshtastic NicheGraphics driver

#include "LCMEN2R13EFC1.h"

// Look-up tables for FAST refresh
static const uint8_t LUT_FAST_VCOMDC[] = {
    0x01, 0x06, 0x03, 0x02, 0x01, 0x01, 0x01,
    0x01, 0x06, 0x02, 0x01, 0x01, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t LUT_FAST_WW[] = {
    0x01, 0x06, 0x03, 0x02, 0x81, 0x01, 0x01,
    0x01, 0x06, 0x02, 0x01, 0x01, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t LUT_FAST_BW[] = {
    0x01, 0x86, 0x83, 0x82, 0x81, 0x01, 0x01,
    0x01, 0x86, 0x82, 0x01, 0x01, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t LUT_FAST_WB[] = {
    0x01, 0x46, 0x43, 0x02, 0x01, 0x01, 0x01,
    0x01, 0x46, 0x42, 0x01, 0x01, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t LUT_FAST_BB[] = {
    0x01, 0x06, 0x03, 0x42, 0x41, 0x01, 0x01,
    0x01, 0x06, 0x02, 0x01, 0x01, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

LCMEN2R13EFC1::LCMEN2R13EFC1() 
    : _spiSettings(6000000, MSBFIRST, SPI_MODE0)
{
    // Calculate buffer size (8 pixels per byte)
    _bufferRowSize = ((DISPLAY_WIDTH - 1) / 8) + 1;  // 122px -> 16 bytes
    _bufferSize = _bufferRowSize * DISPLAY_HEIGHT;    // 16 * 250 = 4000 bytes
}

void LCMEN2R13EFC1::begin(SPIClass *spi, uint8_t pin_dc, uint8_t pin_cs, uint8_t pin_busy, uint8_t pin_rst) {
    _spi = spi;
    _pin_dc = pin_dc;
    _pin_cs = pin_cs;
    _pin_busy = pin_busy;
    _pin_rst = pin_rst;
    
    pinMode(_pin_dc, OUTPUT);
    pinMode(_pin_cs, OUTPUT);
    pinMode(_pin_busy, INPUT);
    pinMode(_pin_rst, INPUT_PULLUP);  // Active low, hold high
    
    reset();
}

void LCMEN2R13EFC1::reset() {
    pinMode(_pin_rst, OUTPUT);
    digitalWrite(_pin_rst, LOW);
    delay(10);
    pinMode(_pin_rst, INPUT_PULLUP);
    wait();
    
    sendCommand(0x12);  // Software reset
    wait();
}

void LCMEN2R13EFC1::wait() {
    // Busy when LOW
    while (digitalRead(_pin_busy) == LOW) {
        yield();
    }
}

bool LCMEN2R13EFC1::isBusy() {
    return (digitalRead(_pin_busy) == LOW);
}

void LCMEN2R13EFC1::sendCommand(const uint8_t command) {
    _spi->beginTransaction(_spiSettings);
    digitalWrite(_pin_dc, LOW);  // DC low = command
    digitalWrite(_pin_cs, LOW);
    _spi->transfer(command);
    digitalWrite(_pin_cs, HIGH);
    digitalWrite(_pin_dc, HIGH);
    _spi->endTransaction();
}

void LCMEN2R13EFC1::sendData(uint8_t data) {
    sendData(&data, 1);
}

void LCMEN2R13EFC1::sendData(const uint8_t *data, uint32_t size) {
    _spi->beginTransaction(_spiSettings);
    digitalWrite(_pin_dc, HIGH);  // DC high = data
    digitalWrite(_pin_cs, LOW);
    
    #if defined(ARCH_ESP32) || defined(ESP32)
        _spi->transferBytes(data, NULL, size);
    #else
        for (uint32_t i = 0; i < size; i++) {
            _spi->transfer(data[i]);
        }
    #endif
    
    digitalWrite(_pin_cs, HIGH);
    digitalWrite(_pin_dc, HIGH);
    _spi->endTransaction();
}

void LCMEN2R13EFC1::configFull() {
    sendCommand(0x00);  // Panel setting register
    sendData(0b11 << 6   // Display resolution
             | 1 << 4    // B&W only
             | 1 << 3    // Vertical scan direction
             | 1 << 2    // Horizontal scan direction
             | 1 << 1    // Shutdown: no
             | 1 << 0    // Reset: no
    );
    
    sendCommand(0x50);   // VCOM and data interval setting
    sendData(0b10 << 6   // Border driven white
             | 0b11 << 4 // Invert image colors: no
             | 0b0111    // Interval between VCOM on and image data
    );
}

void LCMEN2R13EFC1::configFast() {
    sendCommand(0x00);  // Panel setting register
    sendData(0b11 << 6   // Display resolution
             | 1 << 5    // LUT from registers (set below)
             | 1 << 4    // B&W only
             | 1 << 3    // Vertical scan direction
             | 1 << 2    // Horizontal scan direction
             | 1 << 1    // Shutdown: no
             | 1 << 0    // Reset: no
    );
    
    sendCommand(0x50);   // VCOM and data interval setting
    sendData(0b11 << 6   // Border floating
             | 0b01 << 4 // Invert image colors: no
             | 0b0111    // Interval between VCOM on and image data
    );
    
    // Load LUT tables
    sendCommand(0x20);  // VCOM
    sendData(LUT_FAST_VCOMDC, sizeof(LUT_FAST_VCOMDC));
    
    sendCommand(0x21);  // White -> White
    sendData(LUT_FAST_WW, sizeof(LUT_FAST_WW));
    
    sendCommand(0x22);  // Black -> White
    sendData(LUT_FAST_BW, sizeof(LUT_FAST_BW));
    
    sendCommand(0x23);  // White -> Black
    sendData(LUT_FAST_WB, sizeof(LUT_FAST_WB));
    
    sendCommand(0x24);  // Black -> Black
    sendData(LUT_FAST_BB, sizeof(LUT_FAST_BB));
}

void LCMEN2R13EFC1::writeNewImage() {
    sendCommand(0x13);
    sendData(_buffer, _bufferSize);
}

void LCMEN2R13EFC1::writeOldImage() {
    sendCommand(0x10);
    sendData(_buffer, _bufferSize);
}

void LCMEN2R13EFC1::update(uint8_t *imageData, UpdateType type) {
    _updateType = type;
    _buffer = imageData;
    
    reset();
    
    // Configure display
    if (_updateType == UPDATE_FULL) {
        configFull();
    } else {
        configFast();
    }
    
    // Transfer image data
    if (_updateType == UPDATE_FULL) {
        writeNewImage();
        writeOldImage();
    } else {
        writeNewImage();
    }
    
    // Power on and start refresh
    sendCommand(0x04);  // Power on panel voltage
    wait();
    
    sendCommand(0x12);  // Begin executing update
    wait();
    
    // Power off
    sendCommand(0x02);
    wait();
    
    // Update "old memory" for next differential refresh
    if (_updateType != UPDATE_FULL) {
        writeOldImage();
        wait();
    }
}

void LCMEN2R13EFC1::powerOn() {
    sendCommand(0x04);
    wait();
}

void LCMEN2R13EFC1::powerOff() {
    sendCommand(0x02);
    wait();
}
