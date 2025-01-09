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

#ifndef FRAMING_H
  #define FRAMING_H

  #define FEND            0xC0
  #define FESC            0xDB
  #define TFEND           0xDC
  #define TFESC           0xDD

  #define CMD_UNKNOWN     0xFE
  #define CMD_DATA        0x00
  #define CMD_FREQUENCY   0x01
  #define CMD_BANDWIDTH   0x02
  #define CMD_TXPOWER     0x03
  #define CMD_SF          0x04
  #define CMD_CR          0x05
  #define CMD_RADIO_STATE 0x06
  #define CMD_RADIO_LOCK  0x07
  #define CMD_DETECT      0x08
  #define CMD_IMPLICIT    0x09
  #define CMD_LEAVE       0x0A
  #define CMD_ST_ALOCK    0x0B
  #define CMD_LT_ALOCK    0x0C
  #define CMD_PROMISC     0x0E
  #define CMD_READY       0x0F

  #define CMD_STAT_RX     0x21
  #define CMD_STAT_TX     0x22
  #define CMD_STAT_RSSI   0x23
  #define CMD_STAT_SNR    0x24
  #define CMD_STAT_CHTM   0x25
  #define CMD_STAT_PHYPRM 0x26
  #define CMD_STAT_BAT    0x27
  #define CMD_STAT_CSMA   0x28
  #define CMD_BLINK       0x30
  #define CMD_RANDOM      0x40

  #define CMD_FB_EXT      0x41
  #define CMD_FB_READ     0x42
  #define CMD_FB_WRITE    0x43
  #define CMD_FB_READL    0x44
  #define CMD_DISP_READ   0x66
  #define CMD_DISP_INT    0x45
  #define CMD_DISP_ADDR   0x63
  #define CMD_DISP_BLNK   0x64
  #define CMD_DISP_ROT    0x67
  #define CMD_DISP_RCND   0x68
  #define CMD_NP_INT      0x65
  #define CMD_BT_CTRL     0x46
  #define CMD_BT_PIN      0x62
  #define CMD_DIS_IA      0x69

  #define CMD_BOARD       0x47
  #define CMD_PLATFORM    0x48
  #define CMD_MCU         0x49
  #define CMD_FW_VERSION  0x50
  #define CMD_ROM_READ    0x51
  #define CMD_ROM_WRITE   0x52
  #define CMD_CONF_SAVE   0x53
  #define CMD_CONF_DELETE 0x54
  #define CMD_DEV_HASH    0x56
  #define CMD_DEV_SIG     0x57
  #define CMD_FW_HASH     0x58
  #define CMD_HASHES      0x60
  #define CMD_FW_UPD      0x61
  #define CMD_UNLOCK_ROM  0x59
  #define ROM_UNLOCK_BYTE 0xF8
  #define CMD_RESET       0x55
  #define CMD_RESET_BYTE  0xF8

  #define DETECT_REQ      0x73
  #define DETECT_RESP     0x46

  #define RADIO_STATE_OFF 0x00
  #define RADIO_STATE_ON  0x01

  #define NIBBLE_SEQ      0xF0
  #define NIBBLE_FLAGS    0x0F
  #define FLAG_SPLIT      0x01
  #define SEQ_UNSET       0xFF

  #define CMD_ERROR           0x90
  #define ERROR_INITRADIO     0x01
  #define ERROR_TXFAILED      0x02
  #define ERROR_EEPROM_LOCKED 0x03
  #define ERROR_QUEUE_FULL    0x04
  #define ERROR_MEMORY_LOW    0x05
  #define ERROR_MODEM_TIMEOUT 0x06

  // Serial framing variables
  size_t frame_len;
  bool IN_FRAME = false;
  bool ESCAPE = false;
  uint8_t command = CMD_UNKNOWN;

#endif