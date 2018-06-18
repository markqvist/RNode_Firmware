#include <Arduino.h>
#include <EEPROM.h>
#include "Config.h"
#include "Framing.h"

#define ADDR_PRODUCT   0x00
#define ADDR_MODEL     0x01
#define ADDR_HW_REV    0x02
#define ADDR_SERIAL    0x03
#define ADDR_MADE	   0x06
#define ADDR_CHKSUM	   0x0A
#define ADDR_SIGNATURE 0x1A
#define ADDR_INFO_LOCK 0x9A
#define INFO_LOCK_BYTE 0x73

#define ADDR_CONF_SF   0x74
#define ADDR_CONF_CR   0x75
#define ADDR_CONF_TXP  0x76
#define ADDR_CONF_BW   0x77
#define ADDR_CONF_FREQ 0x7B
#define ADDR_CONF_OK   0x7F
#define CONF_OK_BYTE   0x73

void eeprom_dump_info() {
	for (int addr = ADDR_PRODUCT; addr <= ADDR_INFO_LOCK; addr++) {
		uint8_t rom_byte = EEPROM.read(addr);
		Serial.write(rom_byte);
	}
}

void eeprom_dump_config() {
	for (int addr = ADDR_CONF_SF; addr <= ADDR_CONF_OK; addr++) {
		uint8_t rom_byte = EEPROM.read(addr);
		Serial.write(rom_byte);
	}
}