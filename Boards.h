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

#import "Modem.h"

#ifndef BOARDS_H
  #define BOARDS_H
  
    #define PLATFORM_AVR   0x90
    #define PLATFORM_ESP32 0x80
    #define PLATFORM_NRF52 0x70

    #define MCU_1284P 0x91
    #define MCU_2560  0x92
    #define MCU_ESP32 0x81
    #define MCU_NRF52 0x71

    #define BOARD_RNODE         0x31
    #define BOARD_HMBRW         0x32
    #define BOARD_TBEAM         0x33
    #define BOARD_HUZZAH32      0x34
    #define BOARD_GENERIC_ESP32 0x35
    #define BOARD_LORA32_V2_0   0x36
    #define BOARD_LORA32_V2_1   0x37
    #define BOARD_LORA32_V1_0   0x39
    #define BOARD_HELTEC32_V2   0x38
    #define BOARD_RNODE_NG_20   0x40
    #define BOARD_RNODE_NG_21   0x41
    #define BOARD_RNODE_NG_22   0x42
    #define BOARD_GENERIC_NRF52 0x50
    #define BOARD_RAK4630       0x51

    #if BOARD_MODEL == BOARD_RAK4630
      #define MODEM SX1262
    #elif BOARD_MODEL == BOARD_RNODE_NG_22
      #define MODEM SX1262
    #elif BOARD_MODEL == BOARD_GENERIC_NRF52
      #define MODEM SX1262
    #else
      #define MODEM SX1276
    #endif

#endif