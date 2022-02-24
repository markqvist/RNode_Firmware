#ifndef PLATFORM_H
#define PLATFORM_H

// Determine the platform, MCU, and C library we are building for.

#define PLATFORM_AVR   0x90
#define PLATFORM_ESP32 0x80
#define PLATFORM_LINUX 0x70

#define MCU_1284P 0x91
#define MCU_2560  0x92
#define MCU_ESP32 0x81
#define MCU_LINUX 0x71

#define LIBRARY_ARDUINO     0x1
#define LIBRARY_C           0x2

#if defined(__AVR_ATmega1284P__)
    #define PLATFORM PLATFORM_AVR
    #define MCU_VARIANT MCU_1284P
    #define LIBRARY_TYPE LIBRARY_ARDUINO
#elif defined(__AVR_ATmega2560__)
    #define PLATFORM PLATFORM_AVR
    #define MCU_VARIANT MCU_2560
    #define LIBRARY_TYPE LIBRARY_ARDUINO
#elif defined(ESP32)
    #define PLATFORM PLATFORM_ESP32
    #define MCU_VARIANT MCU_ESP32
    #define LIBRARY_TYPE LIBRARY_ARDUINO
#elif defined(__unix__)
    #define PLATFORM PLATFORM_LINUX
    #define MCU_VARIANT MCU_LINUX
    #define LIBRARY_TYPE LIBRARY_C
#else
    #error "The firmware cannot be compiled for the selected MCU variant"
#endif

#ifndef MCU_VARIANT
  #error No MCU variant defined, cannot compile
#endif

#endif
