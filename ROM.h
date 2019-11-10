#ifndef ROM_H
	#define ROM_H

	#define CHECKSUMMED_SIZE 0x0B

	#define PRODUCT_RNODE 0x03
	#define MODEL_A4 0xA4
	#define MODEL_A9 0xA9
  #define MODEL_B1 0xB1

	#define ADDR_PRODUCT   0x00
	#define ADDR_MODEL     0x01
	#define ADDR_HW_REV    0x02
	#define ADDR_SERIAL    0x03
	#define ADDR_MADE	   0x07
	#define ADDR_CHKSUM	   0x0B
	#define ADDR_SIGNATURE 0x1B
	#define ADDR_INFO_LOCK 0x9B

	#define ADDR_CONF_SF   0x9C
	#define ADDR_CONF_CR   0x9D
	#define ADDR_CONF_TXP  0x9E
	#define ADDR_CONF_BW   0x9F
	#define ADDR_CONF_FREQ 0xA3
	#define ADDR_CONF_OK   0xA7

	#define INFO_LOCK_BYTE 0x73
	#define CONF_OK_BYTE   0x73

	#define EEPROM_RESERVED 200
#endif
