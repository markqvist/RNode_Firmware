// E-Ink display chip detection for VME213
// Detects between LCMEN213EFC1 (V1) and E0213A367 (V1.1)
// Based on Meshtastic einkDetect.h

#ifndef EINK_DETECT_VME213_H
#define EINK_DETECT_VME213_H

#include <Arduino.h>

enum EInkChipType {
    EINK_LCMEN213EFC1 = 0,  // Initial version (Fitipower JD79656)
    EINK_E0213A367 = 1       // V1.1+ (Solomon Systech SSD1682)
};

// Detect E-Ink controller IC type by BUSY pin logic
// Fitipower: BUSY=LOW when busy
// Solomon Systech: BUSY=HIGH when busy
inline EInkChipType detectEInkChip(uint8_t pin_reset, uint8_t pin_busy) {
    // Force display BUSY by holding reset pin active
    pinMode(pin_reset, OUTPUT);
    digitalWrite(pin_reset, LOW);
    
    delay(10);
    
    // Read BUSY pin logic while display is busy
    pinMode(pin_busy, INPUT);
    bool busyLogic = digitalRead(pin_busy);
    
    // Release reset pin
    pinMode(pin_reset, INPUT);
    
    // Fitipower = LOW when busy
    // Solomon = HIGH when busy
    if (busyLogic == LOW) {
        return EINK_LCMEN213EFC1;
    } else {
        return EINK_E0213A367;
    }
}

#endif // EINK_DETECT_VME213_H
