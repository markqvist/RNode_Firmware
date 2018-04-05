#ifndef CONFIG_H
	#define CONFIG_H

	#define MCU_328P 0x90
	#define MCU_1284P 0x91

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
	#endif

	#if MCU_VARIANT == MCU_1284P
		const int pin_cs = 4;
		const int pin_reset = 3;
		const int pin_dio = 2;
		const int pin_led_rx = 12;
		const int pin_led_tx = 13;
	#endif

	// MCU independent configuration parameters
	const long serial_baudrate = 115200;
	const int  rssi_offset     = 164;

	// Default LoRa settings
	int  lora_sf   = 0;
	int  lora_txp  = 0xFF;
	uint32_t lora_bw   = 0;
	uint32_t lora_freq = 0;

	// Operational variables
	bool radio_locked = true;
	bool radio_online = false;
	
	int		last_rssi		= -164;
	size_t	read_len		= 0;
	uint8_t seq				= 0xFF;
	uint8_t pbuf[MTU];
	uint8_t sbuf[MTU];
	uint8_t cbuf[CMD_L];

	uint32_t stat_rx		= 0;
	uint32_t stat_tx		= 0;

#endif