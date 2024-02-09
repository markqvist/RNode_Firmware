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

#include "ROM.h"
#include "Boards.h"
#include "Modem.h"

#ifndef CONFIG_H
	#define CONFIG_H

	#define MAJ_VERS  0x01
	#define MIN_VERS  0x46

	#define MODE_HOST 0x11
	#define MODE_TNC  0x12

	#define CABLE_STATE_DISCONNECTED 0x00
	#define CABLE_STATE_CONNECTED    0x01
	uint8_t cable_state = CABLE_STATE_DISCONNECTED;
	
	#define BT_STATE_NA        0xff
	#define BT_STATE_OFF       0x00
	#define BT_STATE_ON        0x01
	#define BT_STATE_PAIRING   0x02
	#define BT_STATE_CONNECTED 0x03
	uint8_t bt_state = BT_STATE_NA;
	uint32_t bt_ssp_pin = 0;
	bool bt_ready = false;
	bool bt_enabled = false;
	bool bt_allow_pairing = false;

	#define M_FRQ_S 27388122
	#define M_FRQ_R 27388061
	bool console_active = false;
	bool modem_installed = false;

	#if defined(__AVR_ATmega1284P__)
	    #define PLATFORM PLATFORM_AVR
	    #define MCU_VARIANT MCU_1284P
	#elif defined(__AVR_ATmega2560__)
	    #define PLATFORM PLATFORM_AVR
	    #define MCU_VARIANT MCU_2560
	#elif defined(ESP32)
	    #define PLATFORM PLATFORM_ESP32
	    #define MCU_VARIANT MCU_ESP32
    #elif defined(NRF52840_XXAA)
        #define PLATFORM PLATFORM_NRF52
        #define MCU_VARIANT MCU_NRF52
	#else
	    #error "The firmware cannot be compiled for the selected MCU variant"
	#endif

	#define MTU   	   508
	#define SINGLE_MTU 255
	#define HEADER_L   1
	#define MIN_L	   1

	#define CMD_L      64

	// MCU dependent configuration parameters

    #define HAS_DISPLAY false
    #define HAS_BLUETOOTH false
    #define HAS_TCXO false
    #define HAS_PMU false
    #define HAS_NP false
    #define HAS_EEPROM false

    #if defined(ENABLE_TCXO)
        #define HAS_TCXO true
    #endif

	#if MCU_VARIANT == MCU_1284P
		const int pin_cs = 4;
		const int pin_reset = 3;
		const int pin_dio = 2;
		const int pin_led_rx = 12;
		const int pin_led_tx = 13;

		#define BOARD_MODEL BOARD_RNODE

        #define HAS_EEPROM true

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

        #define HAS_EEPROM true

		#define CONFIG_UART_BUFFER_SIZE 768
		#define CONFIG_QUEUE_SIZE 5120
		#define CONFIG_QUEUE_MAX_LENGTH 24

		#define EEPROM_SIZE 4096
		#define EEPROM_OFFSET EEPROM_SIZE-EEPROM_RESERVED

	#elif MCU_VARIANT == MCU_ESP32

		// Board models for ESP32 based builds are
		// defined by the build target in the makefile.
		// If you are not using make to compile this
		// firmware, you can manually define model here.
		//
		// #define BOARD_MODEL BOARD_GENERIC_ESP32
		#define CONFIG_UART_BUFFER_SIZE 6144
		#define CONFIG_QUEUE_SIZE 6144
		#define CONFIG_QUEUE_MAX_LENGTH 200

		#define EEPROM_SIZE 1024
		#define EEPROM_OFFSET EEPROM_SIZE-EEPROM_RESERVED

		#define GPS_BAUD_RATE 9600
		#define PIN_GPS_TX 12
		#define PIN_GPS_RX 34

		#if BOARD_MODEL == BOARD_GENERIC_ESP32
			const int pin_cs = 4;
			const int pin_reset = 36;
			const int pin_dio = 39;
			const int pin_led_rx = 14;
			const int pin_led_tx = 32;
            #define HAS_BLUETOOTH true
            #define HAS_CONSOLE true
            #define HAS_EEPROM true
		#elif BOARD_MODEL == BOARD_TBEAM
			const int pin_cs = 18;
			const int pin_reset = 23;
			const int pin_dio = 26;
			const int pin_led_rx = 2;
			const int pin_led_tx = 4;
            #define HAS_DISPLAY true
            #define HAS_PMU true
            #define HAS_BLUETOOTH true
            #define HAS_CONSOLE true
            #define HAS_SD false
            #define HAS_EEPROM true
		#elif BOARD_MODEL == BOARD_HUZZAH32
			const int pin_cs = 4;
			const int pin_reset = 36;
			const int pin_dio = 39;
			const int pin_led_rx = 14;
			const int pin_led_tx = 32;
			#define HAS_BLUETOOTH true
            #define HAS_CONSOLE true
            #define HAS_EEPROM true
		#elif BOARD_MODEL == BOARD_LORA32_V1_0
			const int pin_cs = 18;
			const int pin_reset = 14;
			const int pin_dio = 26;
			#if defined(EXTERNAL_LEDS)
				const int pin_led_rx = 25;
				const int pin_led_tx = 2;
			#else
				const int pin_led_rx = 2;
				const int pin_led_tx = 2;
			#endif
            #define HAS_DISPLAY true
            #define HAS_BLUETOOTH true
            #define HAS_CONSOLE true
            #define HAS_EEPROM true
		#elif BOARD_MODEL == BOARD_LORA32_V2_0
			const int pin_cs = 18;
			const int pin_reset = 12;
			const int pin_dio = 26;
			#if defined(EXTERNAL_LEDS)
				const int pin_led_rx = 2;
				const int pin_led_tx = 0;
			#else
				const int pin_led_rx = 22;
				const int pin_led_tx = 22;
			#endif
            #define HAS_DISPLAY true
            #define HAS_BLUETOOTH true
            #define HAS_CONSOLE true
            #define HAS_EEPROM true
		#elif BOARD_MODEL == BOARD_LORA32_V2_1
			const int pin_cs = 18;
			const int pin_reset = 23;
			const int pin_dio = 26;
			#if HAS_TCXO == true
				const int pin_tcxo_enable = 33;
			#endif
			#if defined(EXTERNAL_LEDS)
				const int pin_led_rx = 15;
				const int pin_led_tx = 4;
			#else
				const int pin_led_rx = 25;
				const int pin_led_tx = 25;
			#endif
            #define HAS_DISPLAY true
            #define HAS_BLUETOOTH true
            #define HAS_PMU true
            #define HAS_CONSOLE true
            #define HAS_EEPROM true
		#elif BOARD_MODEL == BOARD_HELTEC32_V2
			const int pin_cs = 18;
			const int pin_reset = 23;
			const int pin_dio = 26;
			#if defined(EXTERNAL_LEDS)
				const int pin_led_rx = 36;
				const int pin_led_tx = 37;
			#else
				const int pin_led_rx = 25;
				const int pin_led_tx = 25;
			#endif
            #define HAS_DISPLAY true
			#define HAS_BLUETOOTH true
            #define HAS_CONSOLE true
            #define HAS_EEPROM true
		#elif BOARD_MODEL == BOARD_RNODE_NG_20
            #define HAS_DISPLAY true
            #define HAS_BLUETOOTH true
			#define HAS_NP true
            #define HAS_CONSOLE true
            #define HAS_EEPROM true
			const int pin_cs = 18;
			const int pin_reset = 12;
			const int pin_dio = 26;
			const int pin_np = 4;
			#if HAS_NP == false
				#if defined(EXTERNAL_LEDS)
					const int pin_led_rx = 2;
					const int pin_led_tx = 0;
				#else
					const int pin_led_rx = 22;
					const int pin_led_tx = 22;
				#endif
			#endif
		#elif BOARD_MODEL == BOARD_RNODE_NG_21
            #define HAS_DISPLAY true
            #define HAS_BLUETOOTH true
			#define HAS_CONSOLE true
            #define HAS_PMU true
			#define HAS_NP true
			#define HAS_SD false
            #define HAS_EEPROM true
			const int pin_cs = 18;
			const int pin_reset = 23;
			const int pin_dio = 26;
			const int pin_np = 12;
			const int pin_dac = 25;
			const int pin_adc = 34;
			const int SD_MISO = 2;
			const int SD_MOSI = 15;
			const int SD_CLK = 14;
			const int SD_CS = 13;
			#if HAS_NP == false
				#if defined(EXTERNAL_LEDS)
					const int pin_led_rx = 12;
					const int pin_led_tx = 4;
				#else
					const int pin_led_rx = 25;
					const int pin_led_tx = 25;
				#endif
			#endif
        #elif BOARD_MODEL == BOARD_RNODE_NG_22
            #define HAS_DISPLAY true
            #define HAS_BLUETOOTH false // TODO: Implement
			#define HAS_CONSOLE false   // TODO: Implement
            #define HAS_PMU true
			#define HAS_NP true
			#define HAS_SD false
            #define HAS_EEPROM true
			const int pin_cs = 7;
			const int pin_reset = 8;
			const int pin_dio = 33;
			const int pin_np = 38;
			const int pin_dac = 25;
			const int pin_adc = 1;
			const int SD_MISO = 2;
			const int SD_MOSI = 11;
			const int SD_CLK = 14;
			const int SD_CS = 13;
			#if HAS_NP == false
				#if defined(EXTERNAL_LEDS)
					const int pin_led_rx = 37;
					const int pin_led_tx = 37;
				#else
					const int pin_led_rx = 37;
					const int pin_led_tx = 37;
				#endif
			#endif
        #endif
    #elif PLATFORM == PLATFORM_NRF52
        #if BOARD_MODEL == BOARD_RAK4630
            #define HAS_EEPROM false
            #define HAS_DISPLAY false // set for debugging
            #define HAS_BLUETOOTH false
            #define HAS_CONSOLE false
            #define HAS_PMU false
            #define HAS_NP false
            #define HAS_SD false
            #define HAS_TCXO true
            #define HAS_RF_SWITCH_RX_TX true
            #define HAS_BUSY true
            #define MODEM SX1280
            
            #define CONFIG_UART_BUFFER_SIZE 6144
            #define CONFIG_QUEUE_SIZE 6144
            #define CONFIG_QUEUE_MAX_LENGTH 200
            #define EEPROM_SIZE 4096
            #define EEPROM_OFFSET EEPROM_SIZE+0xED000-EEPROM_RESERVED

            // following pins are for the sx1262
            /*const int pin_rxen = 37;
            const int pin_reset = 38;
            const int pin_cs = 42;
            const int pin_sclk = 43;
            const int pin_mosi = 44;
            const int pin_miso = 45;
            const int pin_busy = 46;
            const int pin_dio = 47;*/
            const int pin_led_rx = LED_BLUE;
            const int pin_led_tx = LED_GREEN;
            const int pin_tcxo_enable = -1;
        #endif
	#else
		#error An unsupported board was selected. Cannot compile RNode firmware.
	#endif

    bool mw_radio_online = false;

	#if BOARD_MODEL == BOARD_TBEAM
		#define I2C_SDA 21
		#define I2C_SCL 22
		#define PMU_IRQ 35
	#endif

	#define eeprom_addr(a) (a+EEPROM_OFFSET)

    #ifndef HAS_RF_SWITCH_RX_TX
        const int pin_rxen = -1;
        const int pin_txen = -1;
    #endif

    #ifndef HAS_BUSY
        const int pin_busy = -1;
    #endif

    #if (MODEM == SX1262 || MODEM == SX1280) && defined(NRF52840_XXAA)
        SPIClass spiModem(NRF_SPIM2, pin_miso, pin_sclk, pin_mosi);
    #endif

	// MCU independent configuration parameters
	const long serial_baudrate  = 115200;

	// SX1276 RSSI offset to get dBm value from
	// packet RSSI register
	const int  rssi_offset = 157;

	// Default LoRa settings
	const int lora_rx_turnaround_ms = 66;
	const int lora_post_tx_yield_slots = 6;
	uint32_t post_tx_yield_timeout = 0;
	#define LORA_PREAMBLE_SYMBOLS_HW  4
	#define LORA_PREAMBLE_SYMBOLS_MIN 18
	#define LORA_PREAMBLE_TARGET_MS   15
	#define LORA_CAD_SYMBOLS 3
	int csma_slot_ms = 50;
	float csma_p_min = 0.1;
	float csma_p_max = 0.8;
	uint8_t csma_p = 0;

	int  lora_sf   	           = 0;
	int  lora_cr               = 5;
	int  lora_txp              = 0xFF;
	uint32_t lora_bw           = 0;
	uint32_t lora_freq         = 0;
	uint32_t lora_bitrate      = 0;
	long lora_preamble_symbols = 6;
	float lora_symbol_time_ms  = 0.0;
	float lora_symbol_rate     = 0.0;
	float lora_us_per_byte     = 0.0;

	// Operational variables
	bool radio_locked  = true;
	bool radio_online  = false;
	bool community_fw  = true;
	bool hw_ready      = false;
	bool radio_error   = false;
	bool disp_ready    = false;
	bool pmu_ready     = false;
	bool promisc       = false;
	bool implicit      = false;
	uint8_t implicit_l = 0;

	uint8_t op_mode   = MODE_HOST;
	uint8_t model     = 0x00;
	uint8_t hwrev     = 0x00;

    int     current_rssi    = -292;
	int		last_rssi		= -292;
	uint8_t last_rssi_raw   = 0x00;
	uint8_t last_snr_raw	= 0x80;
	uint8_t seq				= 0xFF;
	uint16_t read_len		= 0;

	// Incoming packet buffer
	uint8_t pbuf[MTU];

	// KISS command buffer
	uint8_t cmdbuf[CMD_L];

	// LoRa transmit buffer
	uint8_t tbuf[MTU];

	uint32_t stat_rx		= 0;
	uint32_t stat_tx		= 0;

	#define STATUS_INTERVAL_MS 3
	#if MCU_VARIANT == MCU_ESP32 || MCU_VARIANT == MCU_NRF52
	  #define DCD_SAMPLES 2500
		#define UTIL_UPDATE_INTERVAL_MS 1000
		#define UTIL_UPDATE_INTERVAL (UTIL_UPDATE_INTERVAL_MS/STATUS_INTERVAL_MS)
		#define AIRTIME_LONGTERM 3600
		#define AIRTIME_LONGTERM_MS (AIRTIME_LONGTERM*1000)
		#define AIRTIME_BINLEN_MS (STATUS_INTERVAL_MS*DCD_SAMPLES)
		#define AIRTIME_BINS ((AIRTIME_LONGTERM*1000)/AIRTIME_BINLEN_MS)
		bool util_samples[DCD_SAMPLES];
		uint16_t airtime_bins[AIRTIME_BINS];
		float longterm_bins[AIRTIME_BINS];
		int dcd_sample = 0;
		float local_channel_util = 0.0;
		float total_channel_util = 0.0;
		float longterm_channel_util = 0.0;
		float airtime = 0.0;
		float longterm_airtime = 0.0;
		#define current_airtime_bin(void) (millis()%AIRTIME_LONGTERM_MS)/AIRTIME_BINLEN_MS
	#endif
	float st_airtime_limit = 0.0;
	float lt_airtime_limit = 0.0;
	bool airtime_lock = false;

	bool stat_signal_detected   = false;
	bool stat_signal_synced     = false;
	bool stat_rx_ongoing        = false;
	bool dcd                    = false;
	bool dcd_led                = false;
	bool dcd_waiting            = false;
	long dcd_wait_until         = 0;
	uint16_t dcd_count          = 0;
	uint16_t dcd_threshold      = 2;

	uint32_t status_interval_ms = STATUS_INTERVAL_MS;
	uint32_t last_status_update = 0;
	uint32_t last_dcd = 0;

	// Status flags
	const uint8_t SIG_DETECT = 0x01;
	const uint8_t SIG_SYNCED = 0x02;
	const uint8_t RX_ONGOING = 0x04;

    // Power management
    #define BATTERY_STATE_DISCHARGING 0x01
    #define BATTERY_STATE_CHARGING 0x02
    #define BATTERY_STATE_CHARGED 0x03
    bool battery_installed = false;
    bool battery_indeterminate = false;
    bool external_power = false;
    bool battery_ready = false;
    float battery_voltage = 0.0;
    float battery_percent = 0.0;
    uint8_t battery_state = 0x00;
    uint8_t display_intensity = 0xFF;
    uint8_t display_addr = 0xFF;
    bool display_diagnostics = true;    
    bool device_init_done = false;
    bool eeprom_ok = false;
    bool firmware_update_mode = false;

	// Boot flags
	#define START_FROM_BOOTLOADER 0x01
	#define START_FROM_POWERON 0x02
	#define START_FROM_BROWNOUT 0x03
	#define START_FROM_JTAG 0x04

#endif
