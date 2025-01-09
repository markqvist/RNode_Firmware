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

#ifndef ROM_H
  #define ROM_H
  #define CHECKSUMMED_SIZE  0x0B

  // ROM address map ///////////////
  #define ADDR_PRODUCT   0x00
  #define ADDR_MODEL     0x01
  #define ADDR_HW_REV    0x02
  #define ADDR_SERIAL    0x03
  #define ADDR_MADE      0x07
  #define ADDR_CHKSUM    0x0B
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
  #define ADDR_CONF_DROT 0xB8
  #define ADDR_CONF_PSET 0xB5
  #define ADDR_CONF_PINT 0xB6
  #define ADDR_CONF_BSET 0xB7
  #define ADDR_CONF_DIA  0xB9

  #define INFO_LOCK_BYTE 0x73
  #define CONF_OK_BYTE   0x73
  #define BT_ENABLE_BYTE 0x73

  #define EEPROM_RESERVED 200
  //////////////////////////////////

#endif
