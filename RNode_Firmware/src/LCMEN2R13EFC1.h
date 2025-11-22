// E-Ink display driver for LCMEN2R13EFC1 (Heltec VisionMaster E213)
// Adapted from Meshtastic firmware for RNode use
// Controller IC: Fitipower JD79656
// Resolution: 122x250 pixels (width x height)
// Supports FAST (partial) and FULL refresh

#ifndef LCMEN2R13EFC1_H
#define LCMEN2R13EFC1_H

#include <Arduino.h>
#include <SPI.h>

class LCMEN2R13EFC1 {
public:
    // Display properties
    static constexpr uint16_t DISPLAY_WIDTH = 122;
    static constexpr uint16_t DISPLAY_HEIGHT = 250;
    
    // Update types
    enum UpdateType {
        UPDATE_FULL = 0,  // Full refresh (slower, ~3.6s)
        UPDATE_FAST = 1   // Partial refresh (faster, ~720ms)
    };
    
    LCMEN2R13EFC1();
    
    void begin(SPIClass *spi, uint8_t pin_dc, uint8_t pin_cs, uint8_t pin_busy, uint8_t pin_rst);
    void update(uint8_t *imageData, UpdateType type);
    bool isBusy();
    void powerOn();
    void powerOff();
    
private:
    SPIClass *_spi;
    uint8_t _pin_dc;
    uint8_t _pin_cs;
    uint8_t _pin_busy;
    uint8_t _pin_rst;
    
    SPISettings _spiSettings;
    
    uint8_t *_buffer;
    uint16_t _bufferRowSize;
    uint32_t _bufferSize;
    UpdateType _updateType;
    
    void reset();
    void wait();
    void sendCommand(const uint8_t command);
    void sendData(uint8_t data);
    void sendData(const uint8_t *data, uint32_t size);
    
    void configFull();
    void configFast();
    void writeNewImage();
    void writeOldImage();
};

#endif // LCMEN2R13EFC1_H
