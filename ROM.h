// Copyright (C) 2023, Mark Qvist

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

#ifndef ROM_H
	#define ROM_H

	#define CHECKSUMMED_SIZE 0x0B

	#define PRODUCT_RNODE    0x03  // Board code 0x31
	#define MODEL_A4 0xA4          // RNode v1.0, 433 MHz
	#define MODEL_A9 0xA9          // RNode v1.0, 868 MHz

                                   // Board code 0x40
	#define MODEL_A3 0xA3          // RNode v2.0, 433 MHz
	#define MODEL_A8 0xA8          // RNode v2.0, 868 MHz

                                   // Board code 0x41
	#define MODEL_A2 0xA2          // RNode v2.1, 433 MHz
	#define MODEL_A7 0xA7          // RNode v2.1, 868 MHz

                                   // Board code 0x42
	#define MODEL_A1 0xA1          // RNode v2.2, 433 MHz
	#define MODEL_A6 0xA6          // RNode v2.2, 868 MHz
	
	#define PRODUCT_TBEAM    0xE0  // Board code 0x33
	#define MODEL_E4 0xE4          // T-Beam SX1278, 433 Mhz
	#define MODEL_E9 0xE9          // T-Beam SX1276, 868 Mhz
	#define MODEL_E3 0xE3          // T-Beam SX1268, 433 Mhz
	#define MODEL_E8 0xE8          // T-Beam SX1262, 868 Mhz
	
	#define PRODUCT_TDECK_V1 0xD0  // Board code 0x3B
	#define MODEL_D4 0xD4          // LilyGO T-Deck, 433 MHz
	#define MODEL_D9 0xD9          // LilyGO T-Deck, 868 MHz
	
	#define PRODUCT_T32_10   0xB2  // Board code 0x39
	#define MODEL_BA 0xBA          // LilyGO T3 v1.0, 433 MHz
	#define MODEL_BB 0xBB          // LilyGO T3 v1.0, 868 MHz
	
	#define PRODUCT_T32_20   0xB0  // Board code 0x36
	#define MODEL_B3 0xB3          // LilyGO T3 v2.0, 433 MHz
	#define MODEL_B8 0xB8          // LilyGO T3 v2.0, 868 MHz

	#define PRODUCT_T32_21   0xB1  // Board code 0x37
	#define MODEL_B4 0xB4          // LilyGO T3 v2.1, 433 MHz
	#define MODEL_B9 0xB9          // LilyGO T3 v2.1, 868 MHz
	
	#define PRODUCT_H32_V2   0xC0  // Board code 0x38
	#define MODEL_C4 0xC4          // Heltec Lora32 v2, 433 MHz
	#define MODEL_C9 0xC9          // Heltec Lora32 v2, 868 MHz

	#define PRODUCT_H32_V3   0xC1  // Board code 0x3A
	#define MODEL_C5 0xC5          // Heltec Lora32 v3, 433 MHz
	#define MODEL_CA 0xCA          // Heltec Lora32 v3, 868 MHz

    #define PRODUCT_RAK4631  0x10  // Board code 0x51
    #define MODEL_11 0x11          // RAK4631, 433 Mhz
    #define MODEL_12 0x12          // RAK4631, 868 Mhz

	#define PRODUCT_HMBRW    0xF0  // Board code 0x32
	#define MODEL_FE 0xFE          // Homebrew board, max 17dBm output power
	#define MODEL_FF 0xFF          // Homebrew board, max 14dBm output power


	#define ADDR_PRODUCT   0x00
	#define ADDR_MODEL     0x01
	#define ADDR_HW_REV    0x02
	#define ADDR_SERIAL    0x03
	#define ADDR_MADE      0x07
	#define ADDR_CHKSUM	   0x0B
	#define ADDR_SIGNATURE 0x1B
	#define ADDR_INFO_LOCK 0x9B

	#define ADDR_CONF_SF   0x9C
	#define ADDR_CONF_CR   0x9D
	#define ADDR_CONF_TXP  0x9E
	#define ADDR_CONF_BW   0x9F
	#define ADDR_CONF_FREQ 0xA3
	#define ADDR_CONF_OK   0xA7
	
	#define ADDR_CONF_BT   0xB0
	#define ADDR_CONF_DSET 0xB1
	#define ADDR_CONF_DINT 0xB2
	#define ADDR_CONF_DADR 0xB3
	#define ADDR_CONF_DBLK 0xB4
	#define ADDR_CONF_PSET 0xB5
	#define ADDR_CONF_PINT 0xB6
	#define ADDR_CONF_BSET 0xB7

	#define INFO_LOCK_BYTE 0x73
	#define CONF_OK_BYTE   0x73
	#define BT_ENABLE_BYTE 0x73

	#define EEPROM_RESERVED 200

#endif
