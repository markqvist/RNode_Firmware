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

	#define PRODUCT_RNODE  0x03
	#define PRODUCT_HMBRW  0xF0
	#define PRODUCT_TBEAM  0xE0
	#define PRODUCT_T32_10 0xB2
	#define PRODUCT_T32_20 0xB0
	#define PRODUCT_T32_21 0xB1
	#define PRODUCT_H32_V2 0xC0
	#define PRODUCT_H32_V3 0xC1
    #define PRODUCT_RAK4631 0x10
    #define MODEL_11 0x11
    #define MODEL_12 0x12
	#define MODEL_A1 0xA1
	#define MODEL_A6 0xA6
	#define MODEL_A4 0xA4
	#define MODEL_A9 0xA9
	#define MODEL_A3 0xA3
	#define MODEL_A8 0xA8
	#define MODEL_A2 0xA2
	#define MODEL_A7 0xA7
	#define MODEL_B3 0xB3
	#define MODEL_B8 0xB8
	#define MODEL_B4 0xB4
	#define MODEL_B9 0xB9
	#define MODEL_BA 0xBA
	#define MODEL_BB 0xBB
	#define MODEL_C4 0xC4
	#define MODEL_C9 0xC9
	#define MODEL_C5 0xC5
	#define MODEL_CA 0xCA
	#define MODEL_E4 0xE4
	#define MODEL_E9 0xE9
	#define MODEL_E3 0xE3
	#define MODEL_E8 0xE8
	#define MODEL_FE 0xFE
	#define MODEL_FF 0xFF

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

	#define INFO_LOCK_BYTE 0x73
	#define CONF_OK_BYTE   0x73
	#define BT_ENABLE_BYTE 0x73

	#define EEPROM_RESERVED 200

#endif
