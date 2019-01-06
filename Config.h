#include "ROM.h"

#ifndef CONFIG_H
	#define CONFIG_H

	#define MAJ_VERS  0x01
	#define MIN_VERS  0x0A

	#define MCU_328P  0x90
	#define MCU_1284P 0x91

	#define MODE_HOST 0x11
	#define MODE_TNC  0x12

	#if defined(__AVR_ATmega328P__)
		#define MCU_VARIANT MCU_328P
		#warning "Firmware is being compiled for atmega328p based boards"
	#elif defined(__AVR_ATmega1284P__)
		#define MCU_VARIANT MCU_1284P
		#warning "Firmware is being compiled for atmega1284p based boards"
	#else
		#error "The firmware cannot be compiled for the selected MCU variant"
	#endif

	#define MTU   	   500
	#define SINGLE_MTU 255
	#define HEADER_L   1
	#define CMD_L      4

	// MCU dependent configuration parameters
	#if MCU_VARIANT == MCU_328P
		const int pin_cs = 7;
		const int pin_reset = 6;
		const int pin_dio = 2;
		const int pin_led_rx = 5;
		const int pin_led_tx = 4;

		#define FLOW_CONTROL_ENABLED true
		#define QUEUE_SIZE 0

		#define EEPROM_SIZE 512
		#define EEPROM_OFFSET EEPROM_SIZE-EEPROM_RESERVED
	#endif

	#if MCU_VARIANT == MCU_1284P
		const int pin_cs = 4;
		const int pin_reset = 3;
		const int pin_dio = 2;
		const int pin_led_rx = 12;
		const int pin_led_tx = 13;

		#define FLOW_CONTROL_ENABLED true
		#define QUEUE_SIZE 24
		#define QUEUE_BUF_SIZE (QUEUE_SIZE+1)
		#define QUEUE_MEM QUEUE_BUF_SIZE * MTU

		#define EEPROM_SIZE 4096
		#define EEPROM_OFFSET EEPROM_SIZE-EEPROM_RESERVED
	#endif

	#define eeprom_addr(a) (a+EEPROM_OFFSET)

	// MCU independent configuration parameters
	const long serial_baudrate  = 115200;
	const int  rssi_offset      = 292;

	const int lora_rx_turnaround_ms = 50;

	// Default LoRa settings
	int  lora_sf   	   = 0;
	int  lora_cr       = 5;
	int  lora_txp      = 0xFF;
	uint32_t lora_bw   = 0;
	uint32_t lora_freq = 0;

	// Operational variables
	bool radio_locked = true;
	bool radio_online = false;
	bool hw_ready     = false;
	bool promisc      = false;

	uint8_t op_mode   = MODE_HOST;
	uint8_t model     = 0x00;
	uint8_t hwrev     = 0x00;
	
	int		last_rssi		= -292;
	uint8_t last_rssi_raw   = 0x00;
	size_t	read_len		= 0;
	uint8_t seq				= 0xFF;
	uint8_t pbuf[MTU];
	uint8_t sbuf[MTU];
	uint8_t cbuf[CMD_L];

	#if QUEUE_SIZE > 0
		uint8_t tbuf[MTU];
		uint8_t qbuf[QUEUE_MEM];
		size_t  queued_lengths[QUEUE_BUF_SIZE];
	#endif

	uint32_t stat_rx		= 0;
	uint32_t stat_tx		= 0;

	bool outbound_ready       = false;
	size_t queue_head        = 0;
	size_t queue_tail		  = 0;

	bool stat_signal_detected = false;
	bool stat_signal_synced   = false;
	bool stat_rx_ongoing      = false;
	bool dcd				  = false;
	bool dcd_led              = false;
	bool dcd_waiting          = false;
	uint16_t dcd_count        = 0;
	uint16_t dcd_threshold    = 15;

	uint32_t status_interval_ms = 3;
	uint32_t last_status_update = 0;

	// Status flags
	const uint8_t SIG_DETECT = 0x01;
	const uint8_t SIG_SYNCED = 0x02;
	const uint8_t RX_ONGOING = 0x04;

#endif