// Copyright (C) 2024, Mark Qvist

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "Modem.h"

#ifndef BOARDS_H
  #define BOARDS_H

  #define PLATFORM_AVR        0x90
  #define PLATFORM_ESP32      0x80
  #define PLATFORM_NRF52      0x70

  #define MCU_1284P           0x91
  #define MCU_2560            0x92
  #define MCU_ESP32           0x81
  #define MCU_NRF52           0x71

  // Products, boards and models ////
  #define PRODUCT_RNODE       0x03 // RNode devices
  #define BOARD_RNODE         0x31 // Original v1.0 RNode
  #define MODEL_A4            0xA4 // RNode v1.0, 433 MHz
  #define MODEL_A9            0xA9 // RNode v1.0, 868 MHz

  #define BOARD_RNODE_NG_20   0x40 // RNode hardware revision v2.0
  #define MODEL_A3            0xA3 // RNode v2.0, 433 MHz
  #define MODEL_A8            0xA8 // RNode v2.0, 868 MHz

  #define BOARD_RNODE_NG_21   0x41 // RNode hardware revision v2.1
  #define MODEL_A2            0xA2 // RNode v2.1, 433 MHz
  #define MODEL_A7            0xA7 // RNode v2.1, 868 MHz

  #define BOARD_T3S3          0x42 // T3S3 devices
  #define MODEL_A1            0xA1 // T3S3, 433 MHz with SX1268
  #define MODEL_A5            0xA5 // T3S3, 433 MHz with SX1278
  #define MODEL_A6            0xA6 // T3S3, 868 MHz with SX1262
  #define MODEL_AA            0xAA // T3S3, 868 MHz with SX1276
  #define MODEL_AC            0xAC // T3S3, 2.4 GHz with SX1280 and PA

  #define PRODUCT_TBEAM       0xE0 // T-Beam devices
  #define BOARD_TBEAM         0x33
  #define MODEL_E4            0xE4 // T-Beam SX1278, 433 Mhz
  #define MODEL_E9            0xE9 // T-Beam SX1276, 868 Mhz
  #define MODEL_E3            0xE3 // T-Beam SX1268, 433 Mhz
  #define MODEL_E8            0xE8 // T-Beam SX1262, 868 Mhz

  #define PRODUCT_TDECK_V1    0xD0
  #define BOARD_TDECK         0x3B
  #define MODEL_D4            0xD4 // LilyGO T-Deck, 433 MHz
  #define MODEL_D9            0xD9 // LilyGO T-Deck, 868 MHz

  #define PRODUCT_TBEAM_S_V1  0xEA
  #define BOARD_TBEAM_S_V1    0x3D
  #define MODEL_DB            0xDB // LilyGO T-Beam Supreme, 433 MHz
  #define MODEL_DC            0xDC // LilyGO T-Beam Supreme, 868 MHz

  #define PRODUCT_T32_10      0xB2
  #define BOARD_LORA32_V1_0   0x39
  #define MODEL_BA            0xBA // LilyGO T3 v1.0, 433 MHz
  #define MODEL_BB            0xBB // LilyGO T3 v1.0, 868 MHz

  #define PRODUCT_T32_20      0xB0
  #define BOARD_LORA32_V2_0   0x36
  #define MODEL_B3            0xB3 // LilyGO T3 v2.0, 433 MHz
  #define MODEL_B8            0xB8 // LilyGO T3 v2.0, 868 MHz

  #define PRODUCT_T32_21      0xB1
  #define BOARD_LORA32_V2_1   0x37
  #define MODEL_B4            0xB4  // LilyGO T3 v2.1, 433 MHz
  #define MODEL_B9            0xB9  // LilyGO T3 v2.1, 868 MHz

  #define PRODUCT_H32_V2      0xC0  // Board code 0x38
  #define BOARD_HELTEC32_V2   0x38
  #define MODEL_C4            0xC4  // Heltec Lora32 v2, 433 MHz
  #define MODEL_C9            0xC9  // Heltec Lora32 v2, 868 MHz

  #define PRODUCT_H32_V3      0xC1
  #define BOARD_HELTEC32_V3   0x3A
  #define MODEL_C5            0xC5 // Heltec Lora32 v3, 433 MHz
  #define MODEL_CA            0xCA // Heltec Lora32 v3, 868 MHz

  #define PRODUCT_HELTEC_T114 0xC2 // Heltec Mesh Node T114
  #define BOARD_HELTEC_T114   0x3C
  #define MODEL_C6            0xC6 // Heltec Mesh Node T114, 470-510 MHz
  #define MODEL_C7            0xC7 // Heltec Mesh Node T114, 863-928 MHz

  #define PRODUCT_TECHO       0x15 // LilyGO T-Echo devices
  #define BOARD_TECHO         0x44
  #define MODEL_16            0x16 // T-Echo 433 MHz
  #define MODEL_17            0x17 // T-Echo 868/915 MHz

  #define PRODUCT_RAK4631     0x10
  #define BOARD_RAK4631       0x51
  #define MODEL_11            0x11 // RAK4631, 433 Mhz
  #define MODEL_12            0x12 // RAK4631, 868 Mhz

  #define PRODUCT_HMBRW       0xF0
  #define BOARD_HMBRW         0x32
  #define BOARD_HUZZAH32      0x34
  #define BOARD_GENERIC_ESP32 0x35
  #define BOARD_GENERIC_NRF52 0x50
  #define MODEL_FE            0xFE // Homebrew board, max 17dBm output power
  #define MODEL_FF            0xFF // Homebrew board, max 14dBm output power

  #if defined(__AVR_ATmega1284P__)
    #define PLATFORM PLATFORM_AVR
    #define MCU_VARIANT MCU_1284P
  #elif defined(__AVR_ATmega2560__)
    #define PLATFORM PLATFORM_AVR
    #define MCU_VARIANT MCU_2560
  #elif defined(ESP32)
    #define PLATFORM PLATFORM_ESP32
    #define MCU_VARIANT MCU_ESP32
  #elif defined(NRF52840_XXAA)
    #include <variant.h>
    #define PLATFORM PLATFORM_NRF52
    #define MCU_VARIANT MCU_NRF52
  #else
      #error "The firmware cannot be compiled for the selected MCU variant"
  #endif

  #ifndef MODEM
    #if BOARD_MODEL == BOARD_RAK4631
      #define MODEM SX1262
    #elif BOARD_MODEL == BOARD_GENERIC_NRF52
      #define MODEM SX1262
    #else
      #define MODEM SX1276
    #endif
  #endif

  #define HAS_DISPLAY false
  #define HAS_BLUETOOTH false
  #define HAS_BLE false
  #define HAS_TCXO false
  #define HAS_PMU false
  #define HAS_NP false
  #define HAS_EEPROM false
  #define HAS_INPUT false
  #define HAS_SLEEP false
  #define PIN_DISP_SLEEP -1
  #define VALIDATE_FIRMWARE true

  #if defined(ENABLE_TCXO)
      #define HAS_TCXO true
  #endif

  #if MCU_VARIANT == MCU_1284P
    const int pin_cs = 4;
    const int pin_reset = 3;
    const int pin_dio = 2;
    const int pin_led_rx = 12;
    const int pin_led_tx = 13;

    #define BOARD_MODEL BOARD_RNODE
    #define HAS_EEPROM true
    #define CONFIG_UART_BUFFER_SIZE 6144
    #define CONFIG_QUEUE_SIZE 6144
    #define CONFIG_QUEUE_MAX_LENGTH 200
    #define EEPROM_SIZE 4096
    #define EEPROM_OFFSET EEPROM_SIZE-EEPROM_RESERVED
  
  #elif MCU_VARIANT == MCU_2560
    const int pin_cs = 5;
    const int pin_reset = 4;
    const int pin_dio = 2;
    const int pin_led_rx = 12;
    const int pin_led_tx = 13;

    #define BOARD_MODEL BOARD_HMBRW
    #define HAS_EEPROM true
    #define CONFIG_UART_BUFFER_SIZE 768
    #define CONFIG_QUEUE_SIZE 5120
    #define CONFIG_QUEUE_MAX_LENGTH 24
    #define EEPROM_SIZE 4096
    #define EEPROM_OFFSET EEPROM_SIZE-EEPROM_RESERVED

  #elif MCU_VARIANT == MCU_ESP32

    // Board models for ESP32 based builds are
    // defined by the build target in the makefile.
    // If you are not using make to compile this
    // firmware, you can manually define model here.
    //
    // #define BOARD_MODEL BOARD_GENERIC_ESP32
    #define CONFIG_UART_BUFFER_SIZE 6144
    #define CONFIG_QUEUE_SIZE 6144
    #define CONFIG_QUEUE_MAX_LENGTH 200

    #define EEPROM_SIZE 1024
    #define EEPROM_OFFSET EEPROM_SIZE-EEPROM_RESERVED

    #define GPS_BAUD_RATE 9600
    #define PIN_GPS_TX 12
    #define PIN_GPS_RX 34

    #if BOARD_MODEL == BOARD_GENERIC_ESP32
      #define HAS_BLUETOOTH true
      #define HAS_CONSOLE true
      #define HAS_EEPROM true
      const int pin_cs = 4;
      const int pin_reset = 36;
      const int pin_dio = 39;
      const int pin_led_rx = 14;
      const int pin_led_tx = 32;

    #elif BOARD_MODEL == BOARD_TBEAM
      #define HAS_DISPLAY true
      #define HAS_PMU true
      #define HAS_BLUETOOTH true
      #define HAS_CONSOLE true
      #define HAS_SD false
      #define HAS_EEPROM true
      #define I2C_SDA 21
      #define I2C_SCL 22
      #define PMU_IRQ 35

      #define HAS_INPUT true
      const int pin_btn_usr1 = 38;

      const int pin_cs = 18;
      const int pin_reset = 23;
      const int pin_led_rx = 2;
      const int pin_led_tx = 4;

      #if MODEM == SX1262
        #define HAS_TCXO true
        #define HAS_BUSY true
        #define DIO2_AS_RF_SWITCH true
        #define OCP_TUNED 0x38
        const int pin_busy = 32;
        const int pin_dio = 33;
        const int pin_tcxo_enable = -1;
      #else
        const int pin_dio = 26;
      #endif

    #elif BOARD_MODEL == BOARD_HUZZAH32
      #define HAS_BLUETOOTH true
      #define HAS_CONSOLE true
      #define HAS_EEPROM true
      const int pin_cs = 4;
      const int pin_reset = 36;
      const int pin_dio = 39;
      const int pin_led_rx = 14;
      const int pin_led_tx = 32;

    #elif BOARD_MODEL == BOARD_LORA32_V1_0
      #define HAS_DISPLAY true
      #define HAS_BLUETOOTH true
      #define HAS_CONSOLE true
      #define HAS_EEPROM true
      const int pin_cs = 18;
      const int pin_reset = 14;
      const int pin_dio = 26;
      #if defined(EXTERNAL_LEDS)
        const int pin_led_rx = 25;
        const int pin_led_tx = 2;
      #else
        const int pin_led_rx = 2;
        const int pin_led_tx = 2;
      #endif

    #elif BOARD_MODEL == BOARD_LORA32_V2_0
      #define HAS_DISPLAY true
      #define HAS_BLUETOOTH true
      #define HAS_CONSOLE true
      #define HAS_EEPROM true
      const int pin_cs = 18;
      const int pin_reset = 12;
      const int pin_dio = 26;
      #if defined(EXTERNAL_LEDS)
        const int pin_led_rx = 2;
        const int pin_led_tx = 0;
      #else
        const int pin_led_rx = 22;
        const int pin_led_tx = 22;
      #endif

    #elif BOARD_MODEL == BOARD_LORA32_V2_1
      #define HAS_DISPLAY true
      #define HAS_BLUETOOTH true
      #define HAS_PMU true
      #define HAS_CONSOLE true
      #define HAS_EEPROM true
      const int pin_cs = 18;
      const int pin_reset = 23;
      const int pin_dio = 26;
      #if HAS_TCXO == true
        const int pin_tcxo_enable = 33;
      #endif
      #if defined(EXTERNAL_LEDS)
        const int pin_led_rx = 15;
        const int pin_led_tx = 4;
      #else
        const int pin_led_rx = 25;
        const int pin_led_tx = 25;
      #endif

    #elif BOARD_MODEL == BOARD_HELTEC32_V2
      #define HAS_DISPLAY true
      #define HAS_BLUETOOTH true
      #define HAS_CONSOLE true
      #define HAS_EEPROM true
      #define HAS_INPUT true
      #define HAS_SLEEP true
      #define PIN_WAKEUP GPIO_NUM_0
      #define WAKEUP_LEVEL 0

      const int pin_btn_usr1 = 0;

      const int pin_cs = 18;
      const int pin_reset = 14;
      const int pin_dio = 26;
      #if defined(EXTERNAL_LEDS)
        const int pin_led_rx = 36;
        const int pin_led_tx = 37;
      #else
        const int pin_led_rx = 25;
        const int pin_led_tx = 25;
      #endif

    #elif BOARD_MODEL == BOARD_HELTEC32_V3
      #define IS_ESP32S3 true
      #define HAS_DISPLAY true
      #define HAS_BLUETOOTH false
      #define HAS_BLE true
      #define HAS_PMU true
      #define HAS_CONSOLE true
      #define HAS_EEPROM true
      #define HAS_INPUT true
      #define HAS_SLEEP true
      #define PIN_WAKEUP GPIO_NUM_0
      #define WAKEUP_LEVEL 0
      #define OCP_TUNED 0x38

      const int pin_btn_usr1 = 0;

      #if defined(EXTERNAL_LEDS)
        const int pin_led_rx = 13;
        const int pin_led_tx = 14;
      #else
        const int pin_led_rx = 35;
        const int pin_led_tx = 35;
      #endif

      #define MODEM SX1262
      #define HAS_TCXO true
      const int pin_tcxo_enable = -1;
      #define HAS_BUSY true
      #define DIO2_AS_RF_SWITCH true

      // Following pins are for the SX1262
      const int pin_cs = 8;
      const int pin_busy = 13;
      const int pin_dio = 14;
      const int pin_reset = 12;
      const int pin_mosi = 10;
      const int pin_miso = 11;
      const int pin_sclk = 9;

    #elif BOARD_MODEL == BOARD_RNODE_NG_20
      #define HAS_DISPLAY true
      #define HAS_BLUETOOTH true
      #define HAS_NP true
      #define HAS_CONSOLE true
      #define HAS_EEPROM true
      const int pin_cs = 18;
      const int pin_reset = 12;
      const int pin_dio = 26;
      const int pin_np = 4;
      #if HAS_NP == false
        #if defined(EXTERNAL_LEDS)
          const int pin_led_rx = 2;
          const int pin_led_tx = 0;
        #else
          const int pin_led_rx = 22;
          const int pin_led_tx = 22;
        #endif
      #endif

    #elif BOARD_MODEL == BOARD_RNODE_NG_21
      #define HAS_DISPLAY true
      #define HAS_BLUETOOTH true
      #define HAS_CONSOLE true
      #define HAS_PMU true
      #define HAS_NP true
      #define HAS_SD false
      #define HAS_EEPROM true
      const int pin_cs = 18;
      const int pin_reset = 23;
      const int pin_dio = 26;
      const int pin_np = 12;
      const int pin_dac = 25;
      const int pin_adc = 34;
      const int SD_MISO = 2;
      const int SD_MOSI = 15;
      const int SD_CLK = 14;
      const int SD_CS = 13;
      #if HAS_NP == false
        #if defined(EXTERNAL_LEDS)
          const int pin_led_rx = 12;
          const int pin_led_tx = 4;
        #else
          const int pin_led_rx = 25;
          const int pin_led_tx = 25;
        #endif
      #endif

    #elif BOARD_MODEL == BOARD_T3S3
      #define IS_ESP32S3 true
      #define HAS_DISPLAY true
      #define HAS_CONSOLE true
      #define HAS_BLUETOOTH false
      #define HAS_BLE true
      #define HAS_PMU true
      #define HAS_NP false
      #define HAS_SD false
      #define HAS_EEPROM true

      #define HAS_INPUT true
      #define HAS_SLEEP true
      #define PIN_WAKEUP GPIO_NUM_0
      #define WAKEUP_LEVEL 0
      const int pin_btn_usr1 = 0;

      const int pin_cs = 7;
      const int pin_reset = 8;
      const int pin_sclk = 5;
      const int pin_mosi = 6;
      const int pin_miso = 3;
      
      #if MODEM == SX1262
        #define DIO2_AS_RF_SWITCH true
        #define HAS_BUSY true
        #define HAS_TCXO true
        const int pin_busy = 34;
        const int pin_dio = 33;
        const int pin_tcxo_enable = -1;
      #elif MODEM == SX1280
        #define CONFIG_QUEUE_SIZE 6144
        #define DIO2_AS_RF_SWITCH false
        #define HAS_BUSY true
        #define HAS_TCXO true
        #define HAS_PA true
        const int pa_max_input = 3;

        #define HAS_RF_SWITCH_RX_TX true
        const int pin_rxen = 21;
        const int pin_txen = 10;
        
        const int pin_busy = 36;
        const int pin_dio = 9;
        const int pin_tcxo_enable = -1;
      #else
        const int pin_dio = 9;
      #endif
      
      const int pin_np = 38;
      const int pin_dac = 25;
      const int pin_adc = 1;

      const int SD_MISO = 2;
      const int SD_MOSI = 11;
      const int SD_CLK = 14;
      const int SD_CS = 13;

      #if HAS_NP == false
        #if defined(EXTERNAL_LEDS)
          const int pin_led_rx = 37;
          const int pin_led_tx = 37;
        #else
          const int pin_led_rx = 37;
          const int pin_led_tx = 37;
        #endif
      #endif

    #elif BOARD_MODEL == BOARD_TDECK
      #define IS_ESP32S3 true
      #define MODEM SX1262
      #define DIO2_AS_RF_SWITCH true
      #define HAS_BUSY true
      #define HAS_TCXO true

      #define HAS_DISPLAY false
      #define HAS_CONSOLE false
      #define HAS_BLUETOOTH false
      #define HAS_BLE true
      #define HAS_PMU true
      #define HAS_NP false
      #define HAS_SD false
      #define HAS_EEPROM true

      #define HAS_INPUT true
      #define HAS_SLEEP true
      #define PIN_WAKEUP GPIO_NUM_0
      #define WAKEUP_LEVEL 0

      const int pin_poweron = 10;
      const int pin_btn_usr1 = 0;

      const int pin_cs = 9;
      const int pin_reset = 17;
      const int pin_sclk = 40;
      const int pin_mosi = 41;
      const int pin_miso = 38;
      const int pin_tcxo_enable = -1;
      const int pin_dio = 45;
      const int pin_busy = 13;
      
      const int SD_MISO = 38;
      const int SD_MOSI = 41;
      const int SD_CLK = 40;
      const int SD_CS = 39;

      const int DISPLAY_DC = 11;
      const int DISPLAY_CS = 12;
      const int DISPLAY_MISO = 38;
      const int DISPLAY_MOSI = 41;
      const int DISPLAY_CLK = 40;
      const int DISPLAY_BL_PIN = 42;

      #if HAS_NP == false
        #if defined(EXTERNAL_LEDS)
          const int pin_led_rx = 43;
          const int pin_led_tx = 43;
        #else
          const int pin_led_rx = 43;
          const int pin_led_tx = 43;
        #endif
      #endif

    #elif BOARD_MODEL == BOARD_TBEAM_S_V1
      #define IS_ESP32S3 true
      #define MODEM SX1262
      #define DIO2_AS_RF_SWITCH true
      #define HAS_BUSY true
      #define HAS_TCXO true
      #define OCP_TUNED 0x38

      #define HAS_DISPLAY true
      #define HAS_CONSOLE true
      #define HAS_BLUETOOTH false
      #define HAS_BLE true
      #define HAS_PMU true
      #define HAS_NP false
      #define HAS_SD false
      #define HAS_EEPROM true

      #define HAS_INPUT true
      #define HAS_SLEEP false
      
      #define PMU_IRQ 40
      #define I2C_SCL 41
      #define I2C_SDA 42

      const int pin_btn_usr1 = 0;

      const int pin_cs = 10;
      const int pin_reset = 5;
      const int pin_sclk = 12;
      const int pin_mosi = 11;
      const int pin_miso = 13;
      const int pin_tcxo_enable = -1;
      const int pin_dio = 1;
      const int pin_busy = 4;
      
      const int SD_MISO = 37;
      const int SD_MOSI = 35;
      const int SD_CLK = 36;
      const int SD_CS = 47;

      const int IMU_CS = 34;

      #if HAS_NP == false
        #if defined(EXTERNAL_LEDS)
          const int pin_led_rx = 43;
          const int pin_led_tx = 43;
        #else
          const int pin_led_rx = 43;
          const int pin_led_tx = 43;
        #endif
      #endif

    #else
      #error An unsupported ESP32 board was selected. Cannot compile RNode firmware.
    #endif
  
  #elif MCU_VARIANT == MCU_NRF52
    #if BOARD_MODEL == BOARD_RAK4631
      #define HAS_EEPROM false
      #define HAS_DISPLAY true
      #define HAS_BLUETOOTH false
      #define HAS_BLE true
      #define HAS_CONSOLE false
      #define HAS_PMU false
      #define HAS_NP false
      #define HAS_SD false
      #define HAS_TCXO true
      #define HAS_RF_SWITCH_RX_TX true
      #define HAS_BUSY true
      #define HAS_INPUT true
      #define DIO2_AS_RF_SWITCH true
      #define CONFIG_UART_BUFFER_SIZE 6144
      #define CONFIG_QUEUE_SIZE 6144
      #define CONFIG_QUEUE_MAX_LENGTH 200
      #define EEPROM_SIZE 296
      #define EEPROM_OFFSET EEPROM_SIZE-EEPROM_RESERVED
      #define BLE_MANUFACTURER "RAK Wireless"
      #define BLE_MODEL "RAK4640"

      const int pin_btn_usr1 = 9;

      // Following pins are for the sx1262
      const int pin_rxen = 37;
      const int pin_txen = -1;
      const int pin_reset = 38;
      const int pin_cs = 42;
      const int pin_sclk = 43;
      const int pin_mosi = 44;
      const int pin_miso = 45;
      const int pin_busy = 46;
      const int pin_dio = 47;
      const int pin_led_rx = LED_BLUE;
      const int pin_led_tx = LED_GREEN;
      const int pin_tcxo_enable = -1;

    #elif BOARD_MODEL == BOARD_TECHO
      #define _PINNUM(port, pin) ((port) * 32 + (pin))
      #define MODEM SX1262
      #define HAS_EEPROM false
      #define HAS_BLUETOOTH false
      #define HAS_BLE true
      #define HAS_CONSOLE false
      #define HAS_PMU true
      #define HAS_NP false
      #define HAS_SD false
      #define HAS_TCXO true
      #define HAS_BUSY true
      #define HAS_INPUT true
      #define HAS_SLEEP true
      #define BLE_MANUFACTURER "LilyGO"
      #define BLE_MODEL "T-Echo"

      #define HAS_INPUT true
      #define EEPROM_SIZE 296
      #define EEPROM_OFFSET EEPROM_SIZE-EEPROM_RESERVED

      #define CONFIG_UART_BUFFER_SIZE 32768
      #define CONFIG_QUEUE_SIZE 6144
      #define CONFIG_QUEUE_MAX_LENGTH 200

      #define HAS_DISPLAY true
      #define HAS_BACKLIGHT true
      #define DISPLAY_SCALE 1

      #define LED_ON LOW
      #define LED_OFF HIGH
      #define PIN_LED_GREEN _PINNUM(1, 1)
      #define PIN_LED_RED   _PINNUM(1, 3)
      #define PIN_LED_BLUE  _PINNUM(0, 14)
      #define PIN_VEXT_EN _PINNUM(0, 12)

      const int pin_disp_cs = 30;
      const int pin_disp_dc = 28;
      const int pin_disp_reset = 2;
      const int pin_disp_busy = 3;
      const int pin_disp_en = -1;
      const int pin_disp_sck = 31;
      const int pin_disp_mosi = 29;
      const int pin_disp_miso = -1;
      const int pin_backlight = 43;

      const int pin_btn_usr1 = _PINNUM(1, 10);
      const int pin_btn_touch = _PINNUM(0, 11);

      const int pin_reset = 25;
      const int pin_cs = 24;
      const int pin_sclk = 19;
      const int pin_mosi = 22;
      const int pin_miso = 23;
      const int pin_busy = 17;
      const int pin_dio = 20;
      const int pin_tcxo_enable = 21;
      const int pin_led_rx = PIN_LED_BLUE;
      const int pin_led_tx = PIN_LED_RED;

    #elif BOARD_MODEL == BOARD_HELTEC_T114
      #define MODEM SX1262
      #define HAS_EEPROM false
      #define HAS_DISPLAY true
      #define HAS_BLUETOOTH false
      #define HAS_BLE true
      #define HAS_CONSOLE false
      #define HAS_PMU true
      #define HAS_NP true
      #define HAS_SD false
      #define HAS_TCXO true
      #define HAS_BUSY true
      #define HAS_INPUT true
      #define HAS_SLEEP true
      #define DIO2_AS_RF_SWITCH true
      #define CONFIG_UART_BUFFER_SIZE 6144
      #define CONFIG_QUEUE_SIZE 6144
      #define CONFIG_QUEUE_MAX_LENGTH 200
      #define EEPROM_SIZE 296
      #define EEPROM_OFFSET EEPROM_SIZE-EEPROM_RESERVED
      #define BLE_MANUFACTURER "Heltec"
      #define BLE_MODEL "T114"

      #define PIN_T114_ADC_EN 6
      #define PIN_VEXT_EN 21

      // LED
      #define LED_T114_GREEN 3
      #define PIN_T114_LED 14
      #define NP_M 1
      const int pin_np = PIN_T114_LED;

      // SPI
      #define PIN_T114_MOSI 22
      #define PIN_T114_MISO 23
      #define PIN_T114_SCK  19
      #define PIN_T114_SS   24

      // SX1262
      #define PIN_T114_RST  25
      #define PIN_T114_DIO1 20
      #define PIN_T114_BUSY 17

      // TFT
      #define DISPLAY_SCALE 2
      #define PIN_T114_TFT_MOSI 9
      #define PIN_T114_TFT_MISO 11 // not connected
      #define PIN_T114_TFT_SCK 8
      #define PIN_T114_TFT_SS 11
      #define PIN_T114_TFT_DC 12
      #define PIN_T114_TFT_RST 2
      #define PIN_T114_TFT_EN 3
      #define PIN_T114_TFT_BLGT 15

      // pins for buttons on Heltec T114
      const int pin_btn_usr1 = 42;

      // pins for sx1262 on Heltec T114
      const int pin_reset = PIN_T114_RST;
      const int pin_cs = PIN_T114_SS;
      const int pin_sclk = PIN_T114_SCK;
      const int pin_mosi = PIN_T114_MOSI;
      const int pin_miso = PIN_T114_MISO;
      const int pin_busy = PIN_T114_BUSY;
      const int pin_dio = PIN_T114_DIO1;
      const int pin_led_rx = 35;
      const int pin_led_tx = 35;
      const int pin_tcxo_enable = -1;

      // pins for ST7789 display on Heltec T114
      const int DISPLAY_DC = PIN_T114_TFT_DC;
      const int DISPLAY_CS = PIN_T114_TFT_SS;
      const int DISPLAY_MISO = PIN_T114_TFT_MISO;
      const int DISPLAY_MOSI = PIN_T114_TFT_MOSI;
      const int DISPLAY_CLK = PIN_T114_TFT_SCK;
      const int DISPLAY_BL_PIN = PIN_T114_TFT_BLGT;
      const int DISPLAY_RST = PIN_T114_TFT_RST;

    #else
      #error An unsupported nRF board was selected. Cannot compile RNode firmware.
    #endif

  #endif

  #ifndef DISPLAY_SCALE
    #define DISPLAY_SCALE 1
  #endif

  #ifndef HAS_RF_SWITCH_RX_TX
    const int pin_rxen = -1;
    const int pin_txen = -1;
  #endif

  #ifndef HAS_BUSY
    const int pin_busy = -1;
  #endif

  #ifndef LED_ON
    #define LED_ON HIGH
  #endif
  
  #ifndef LED_OFF
    #define LED_OFF LOW
  #endif

  #ifndef DIO2_AS_RF_SWITCH
    #define DIO2_AS_RF_SWITCH false
  #endif

  // Default OCP value if not specified
  // in board configuration
  #ifndef OCP_TUNED
    #define OCP_TUNED 0x38
  #endif

  #ifndef NP_M
    #define NP_M 0.15
  #endif

#endif
