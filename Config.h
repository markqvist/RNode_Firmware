#include "ROM.h"

#ifndef CONFIG_H
	#define CONFIG_H

	#define MAJ_VERS  0x01
	#define MIN_VERS  0x20

	#define PLATFORM_AVR   0x90
    #define PLATFORM_ESP32 0x80

	#define MCU_1284P 0x91
	#define MCU_2560  0x92
	#define MCU_ESP32 0x81

	#define BOARD_RNODE         0x31
	#define BOARD_HMBRW         0x32
	#define BOARD_TBEAM         0x33
	#define BOARD_HUZZAH32      0x34
	#define BOARD_GENERIC_ESP32 0x35
	#define BOARD_LORA32_V2_0   0x36
	#define BOARD_LORA32_V2_1   0x37

	#define MODE_HOST 0x11
	#define MODE_TNC  0x12

	#if defined(__AVR_ATmega1284P__)
	    #define PLATFORM PLATFORM_AVR
	    #define MCU_VARIANT MCU_1284P
	#elif defined(__AVR_ATmega2560__)
	    #define PLATFORM PLATFORM_AVR
	    #define MCU_VARIANT MCU_2560
	#elif defined(ESP32)
	    #define PLATFORM PLATFORM_ESP32
	    #define MCU_VARIANT MCU_ESP32
	#else
	    #error "The firmware cannot be compiled for the selected MCU variant"
	#endif

	#define MTU   	   500
	#define SINGLE_MTU 255
	#define HEADER_L   1
	#define MIN_L	   1

	#define CMD_L      4

	// MCU dependent configuration parameters

	#if MCU_VARIANT == MCU_1284P
		const int pin_cs = 4;
		const int pin_reset = 3;
		const int pin_dio = 2;
		const int pin_led_rx = 12;
		const int pin_led_tx = 13;

		#define BOARD_MODEL BOARD_RNODE

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

		#define CONFIG_UART_BUFFER_SIZE 768
		#define CONFIG_QUEUE_SIZE 5120
		#define CONFIG_QUEUE_MAX_LENGTH 24

		#define EEPROM_SIZE 4096
		#define EEPROM_OFFSET EEPROM_SIZE-EEPROM_RESERVED

	#elif MCU_VARIANT == MCU_ESP32

		#if BOARD_MODEL == BOARD_GENERIC_ESP32
			const int pin_cs = 4;
			const int pin_reset = 36;
			const int pin_dio = 39;
			const int pin_led_rx = 14;
			const int pin_led_tx = 32;
		#elif BOARD_MODEL == BOARD_TBEAM
			const int pin_cs = 18;
			const int pin_reset = 23;
			const int pin_dio = 26;
			const int pin_led_rx = 2;
			const int pin_led_tx = 4;
		#elif BOARD_MODEL == BOARD_HUZZAH32
			const int pin_cs = 4;
			const int pin_reset = 36;
			const int pin_dio = 39;
			const int pin_led_rx = 14;
			const int pin_led_tx = 32;
		#elif BOARD_MODEL == BOARD_LORA32_V2_0
			// TODO: Add correct pins here
			// const int pin_cs = 18;
			// const int pin_reset = 23;
			// const int pin_dio = 26;
			// const int pin_led_rx = 2;
			// const int pin_led_tx = 25;
		#elif BOARD_MODEL == BOARD_LORA32_V2_1
			const int pin_cs = 18;
			const int pin_reset = 23;
			const int pin_dio = 26;
			#if defined(EXTERNAL_LEDS)
				const int pin_led_rx = 15;
				const int pin_led_tx = 4;
			#else
				const int pin_led_rx = 25;
				const int pin_led_tx = 25;
			#endif
		#else
			#error An unsupported board was selected. Cannot compile RNode firmware.
		#endif

		#define CONFIG_UART_BUFFER_SIZE 6144
		#define CONFIG_QUEUE_SIZE 6144
		#define CONFIG_QUEUE_MAX_LENGTH 200

		#define EEPROM_SIZE 1024
		#define EEPROM_OFFSET EEPROM_SIZE-EEPROM_RESERVED

		#define GPS_BAUD_RATE 9600
		#define PIN_GPS_TX 12
		#define PIN_GPS_RX 34
	#endif

	#if BOARD_MODEL == BOARD_TBEAM
		#define I2C_SDA 21
		#define I2C_SCL 22
		#define PMU_IRQ 35
	#endif

	#define eeprom_addr(a) (a+EEPROM_OFFSET)

	// MCU independent configuration parameters
	const long serial_baudrate  = 115200;
	const int lora_rx_turnaround_ms = 50;

	// SX1276 RSSI offset to get dBm value from
	// packet RSSI register
	const int  rssi_offset = 157;

	// Default LoRa settings
	int  lora_sf   	   = 0;
	int  lora_cr       = 5;
	int  lora_txp      = 0xFF;
	uint32_t lora_bw   = 0;
	uint32_t lora_freq = 0;

	// Operational variables
	bool radio_locked  = true;
	bool radio_online  = false;
	bool hw_ready      = false;
	bool promisc       = false;
	bool implicit      = false;
	uint8_t implicit_l = 0;

	uint8_t op_mode   = MODE_HOST;
	uint8_t model     = 0x00;
	uint8_t hwrev     = 0x00;

	int		last_rssi		= -292;
	uint8_t last_rssi_raw   = 0x00;
	uint8_t last_snr_raw	= 0x00;
	uint8_t seq				= 0xFF;
	uint16_t read_len		= 0;

	// Incoming packet buffer
	uint8_t pbuf[MTU];

	// KISS command buffer
	uint8_t cbuf[CMD_L];

	// LoRa transmit buffer
	uint8_t tbuf[MTU];

	uint32_t stat_rx		= 0;
	uint32_t stat_tx		= 0;

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

	// Boot flags
	#define START_FROM_BOOTLOADER 0x01
	#define START_FROM_POWERON 0x02
	#define START_FROM_BROWNOUT 0x03
	#define START_FROM_JTAG 0x04

#endif