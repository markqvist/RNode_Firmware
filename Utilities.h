#if LIBRARY_TYPE == LIBRARY_ARDUINO
  #include <EEPROM.h>
#endif
#include <stddef.h>
#include "Config.h"
#include "LoRa.h"
#include "ROM.h"
#include "Framing.h"
#include "MD5.h"

#if LIBRARY_TYPE == LIBRARY_C
  #include <time.h>
  #include <poll.h>
  #include <unistd.h>
  #include <pty.h>
  #include <sys/stat.h>
  #include <sys/mman.h>
  #include <fcntl.h>
  // We need a delay()
  void delay(int ms) {
    struct timespec interval;
    interval.tv_sec = ms / 1000;
    interval.tv_nsec = (ms % 1000) * 1000 * 1000;
    // TODO: handle signals interrupting sleep
    nanosleep(&interval, NULL);
  }
  
  // And millis()
  struct timespec millis_base;
  uint32_t millis() {
    // Time since first call is close enough.
    static bool base_set(false);
    if (!base_set) {
      if (clock_gettime(CLOCK_MONOTONIC, &millis_base)) {
        perror("Could not get time");
        exit(1);
      }
      base_set = true;
    }
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now)) {
      perror("Could not get time");
      exit(1);
    }
    return (now.tv_sec - millis_base.tv_sec) * 1000 + (now.tv_nsec - millis_base.tv_nsec)/(1000*1000);
  }
  
  // Serial will want to poll the EEPROM a bit for help text
  bool eeprom_info_locked();
  
  // We also need a Serial
  class SerialClass {
  public:
    void begin(int baud) {
      // Need to be rrentrant for restart
      if (_fd <= 0) {
        int other_end = 0;
        int status = openpty(&_fd, &other_end, NULL, NULL, NULL); 
        if (status) {
          perror("could not open PTY");
          exit(1);
        }
        
        std::cerr << "Listening on " << ttyname(other_end) << std::endl;
        if (!eeprom_info_locked()) {
          std::cerr << "EEPROM configuration is not initialized. You will want to flash it with something like:" << std::endl;
          std::cerr << "\trnodeconf --key" << std::endl;
          std::cerr << "\trnodeconf --rom --platform " << std::hex << PLATFORM << " --product " << PRODUCT_HMBRW << " --model " << MODEL_FF << std::dec << " --hwrev 1 " << ttyname(other_end) << std::endl;
        }
      } else {
        std::cerr << "Skipping Serial reinitialization" << std::endl;
      }
    }
    
    operator bool() {
      return _fd > 0; 
    }
    void write(int b) {
      uint8_t to_write = b;
      ssize_t written = ::write(_fd, &to_write, 1);
      while (written != 1) {
        if (written < 0) {
          perror("could not write to PTY");
          exit(1);
        }
        written = ::write(_fd, &to_write, 1);
      }
    }
    void write(const char* data) {
      while(*data) {
        write(*data);
        ++data;
      }
    }
    bool available() {
      struct pollfd request;
      request.fd = _fd;
      request.events = POLLIN;
      request.revents = 0;
      
      int result = poll(&request, 1, 0);
      
      if (result == -1) {
        perror("could not poll");
        exit(1);
      }
      
      return result > 0;
    }
    uint8_t read() {
      uint8_t buffer;
      ssize_t count = ::read(_fd, &buffer, 1);
      while (count != 1) {
        if (count < 0) {
          perror("could not read from PTY");
          exit(1);
        }
        count = ::read(_fd, &buffer, 1);
      }
      return buffer;
    }
  protected:
    int _fd;
  };
  
  
  SerialClass Serial;
  
  // And random(below);
  int random(int below) {
    return rand() % below;
  }
  
  
#endif

// Log a debug message. Message should have a \r to return the cursor, if
// needed.
void debug(const char* message) {
  #if LIBRARY_TYPE == LIBRARY_C
    std::cerr << message << std::endl;
  #endif
  if (Serial) {
    Serial.write(message);
  }
}

#if MCU_VARIANT == MCU_ESP32
  #include "soc/rtc_wdt.h"
  #define ISR_VECT IRAM_ATTR
#else
  #define ISR_VECT
#endif

#if MCU_VARIANT == MCU_1284P || MCU_VARIANT == MCU_2560
	#include <avr/wdt.h>
	#include <util/atomic.h>
#endif

uint8_t boot_vector = 0x00;

#if MCU_VARIANT == MCU_1284P || MCU_VARIANT == MCU_2560
	uint8_t OPTIBOOT_MCUSR __attribute__ ((section(".noinit")));
	void resetFlagsInit(void) __attribute__ ((naked)) __attribute__ ((used)) __attribute__ ((section (".init0")));
	void resetFlagsInit(void) {
	    __asm__ __volatile__ ("sts %0, r2\n" : "=m" (OPTIBOOT_MCUSR) :);
	}
#elif MCU_VARIANT == MCU_ESP32
	// TODO: Get ESP32 boot flags
#endif

#if MCU_VARIANT == MCU_1284P || MCU_VARIANT == MCU_2560
	void led_rx_on()  { digitalWrite(pin_led_rx, HIGH); }
	void led_rx_off() {	digitalWrite(pin_led_rx, LOW); }
	void led_tx_on()  { digitalWrite(pin_led_tx, HIGH); }
	void led_tx_off() { digitalWrite(pin_led_tx, LOW); }
#elif MCU_VARIANT == MCU_ESP32
	#if BOARD_MODEL == BOARD_TBEAM
		void led_rx_on()  { digitalWrite(pin_led_rx, HIGH); }
		void led_rx_off() {	digitalWrite(pin_led_rx, LOW); }
		void led_tx_on()  { digitalWrite(pin_led_tx, LOW); }
		void led_tx_off() { digitalWrite(pin_led_tx, HIGH); }
	#elif BOARD_MODEL == BOARD_LORA32_V2_0
		#if defined(EXTERNAL_LEDS)
			void led_rx_on()  { digitalWrite(pin_led_rx, HIGH); }
			void led_rx_off() {	digitalWrite(pin_led_rx, LOW); }
			void led_tx_on()  { digitalWrite(pin_led_tx, HIGH); }
			void led_tx_off() { digitalWrite(pin_led_tx, LOW); }
		#else
			void led_rx_on()  { digitalWrite(pin_led_rx, LOW); }
			void led_rx_off() {	digitalWrite(pin_led_rx, HIGH); }
			void led_tx_on()  { digitalWrite(pin_led_tx, LOW); }
			void led_tx_off() { digitalWrite(pin_led_tx, HIGH); }
		#endif
	#elif BOARD_MODEL == BOARD_LORA32_V2_1
		void led_rx_on()  { digitalWrite(pin_led_rx, HIGH); }
		void led_rx_off() {	digitalWrite(pin_led_rx, LOW); }
		void led_tx_on()  { digitalWrite(pin_led_tx, HIGH); }
		void led_tx_off() { digitalWrite(pin_led_tx, LOW); }
	#elif BOARD_MODEL == BOARD_HUZZAH32
		void led_rx_on()  { digitalWrite(pin_led_rx, HIGH); }
		void led_rx_off() {	digitalWrite(pin_led_rx, LOW); }
		void led_tx_on()  { digitalWrite(pin_led_tx, HIGH); }
		void led_tx_off() { digitalWrite(pin_led_tx, LOW); }
	#elif BOARD_MODEL == BOARD_GENERIC_ESP32
		void led_rx_on()  { digitalWrite(pin_led_rx, HIGH); }
		void led_rx_off() {	digitalWrite(pin_led_rx, LOW); }
		void led_tx_on()  { digitalWrite(pin_led_tx, HIGH); }
		void led_tx_off() { digitalWrite(pin_led_tx, LOW); }
  #endif
#elif MCU_VARIANT == MCU_LINUX
  // No LEDs on Linux, probably. SPI only.
  void led_rx_on()  { }
  void led_rx_off() { }
  void led_tx_on()  { }
  void led_tx_off() { }
#endif

#if LIBRARY_TYPE == LIBRARY_C
  // hard_reset needs a declaration for main
  int main(int argc, char** argv);
#endif

void hard_reset(void) {
	#if MCU_VARIANT == MCU_1284P || MCU_VARIANT == MCU_2560
		wdt_enable(WDTO_15MS);
		while(true) {
			led_tx_on(); led_rx_off();
		}
	#elif MCU_VARIANT == MCU_ESP32
		ESP.restart();
  #elif MCU_VARIANT == MCU_LINUX
    // TODO: re-exec ourselves?
    #if LIBRARY_TYPE == LIBRARY_C
      std::cerr << "Restarting" << std::endl;
      exit(main(0, NULL));
    #endif
	#endif
}

void led_indicate_error(int cycles) {
#if LIBRARY_TYPE == LIBRARY_C
  std::cerr << "Indicating error" << std::endl;
#endif
	bool forever = (cycles == 0) ? true : false;
	cycles = forever ? 1 : cycles;
	while(cycles > 0) { 
    led_rx_on();
    led_tx_off();
    delay(100);
    led_rx_off();
    led_tx_on();
    delay(100);
    if (!forever) cycles--;
  }
  led_rx_off();
  led_tx_off();
}

void led_indicate_boot_error() {
#if LIBRARY_TYPE == LIBRARY_C
  std::cerr << "Indicating boot error" << std::endl;
#endif
  while (true) {
	    led_tx_on();
	    led_rx_off();
	    delay(10);
	    led_rx_on();
	    led_tx_off();
	    delay(5);
	}
}

void led_indicate_warning(int cycles) {
#if LIBRARY_TYPE == LIBRARY_C
  std::cerr << "Indicating warning" << std::endl;
#endif
	bool forever = (cycles == 0) ? true : false;
	cycles = forever ? 1 : cycles;
	led_tx_on();
	while(cycles > 0) {
        led_tx_off();
        delay(100);
        led_tx_on();
        delay(100);
        if (!forever) cycles--;
    }
   led_tx_off();
}

#if MCU_VARIANT == MCU_1284P || MCU_VARIANT == MCU_2560 || MCU_VARIANT == MCU_LINUX
	void led_indicate_info(int cycles) {
		bool forever = (cycles == 0) ? true : false;
		cycles = forever ? 1 : cycles;
		while(cycles > 0) {
	    led_rx_off();
	    delay(100);
	    led_rx_on();
	    delay(100);
	    if (!forever) cycles--;
	  }
	  led_rx_off();
	}
#elif MCU_VARIANT == MCU_ESP32
	#if BOARD_MODEL == BOARD_LORA32_V2_1
		void led_indicate_info(int cycles) {
			bool forever = (cycles == 0) ? true : false;
			cycles = forever ? 1 : cycles;
			while(cycles > 0) {
		    led_rx_off();
		    delay(100);
		    led_rx_on();
		    delay(100);
		    if (!forever) cycles--;
		  }
		  led_rx_off();
		}
	#elif BOARD_MODEL == BOARD_LORA32_V2_0
		void led_indicate_info(int cycles) {
			bool forever = (cycles == 0) ? true : false;
			cycles = forever ? 1 : cycles;
			while(cycles > 0) {
		    led_rx_off();
		    delay(100);
		    led_rx_on();
		    delay(100);
		    if (!forever) cycles--;
		  }
		  led_rx_off();
		}
	#else
		void led_indicate_info(int cycles) {
      #if LIBRARY_TYPE == LIBRARY_C
        std::cerr << "Indicating info" << std::endl;
      #endif
			bool forever = (cycles == 0) ? true : false;
			cycles = forever ? 1 : cycles;
			while(cycles > 0) {
		    led_tx_off();
		    delay(100);
		    led_tx_on();
		    delay(100);
		    if (!forever) cycles--;
		  }
		  led_tx_off();
		}
	#endif
#endif

#if MCU_VARIANT == MCU_1284P || MCU_VARIANT == MCU_2560 || MCU_VARIANT == MCU_ESP32
  unsigned long led_standby_ticks = 0;
#endif
#if MCU_VARIANT == MCU_1284P || MCU_VARIANT == MCU_2560
	uint8_t led_standby_min = 1;
	uint8_t led_standby_max = 40;
	unsigned long led_standby_wait = 11000;
#elif MCU_VARIANT == MCU_ESP32
	uint8_t led_standby_min = 200;
	uint8_t led_standby_max = 255;
	uint8_t led_notready_min = 0;
	uint8_t led_notready_max = 255;
	uint8_t led_notready_value = led_notready_min;
	int8_t  led_notready_direction = 0;
	unsigned long led_notready_ticks = 0;
	unsigned long led_standby_wait = 1768;
	unsigned long led_notready_wait = 150;
#endif
#if MCU_VARIANT == MCU_1284P || MCU_VARIANT == MCU_2560 || MCU_VARIANT == MCU_ESP32
  uint8_t led_standby_value = led_standby_min;
  int8_t  led_standby_direction = 0;
#endif

#if MCU_VARIANT == MCU_1284P || MCU_VARIANT == MCU_2560
	void led_indicate_standby() {
		led_standby_ticks++;
		if (led_standby_ticks > led_standby_wait) {
			led_standby_ticks = 0;
			if (led_standby_value <= led_standby_min) {
				led_standby_direction = 1;
			} else if (led_standby_value >= led_standby_max) {
				led_standby_direction = -1;
			}
			led_standby_value += led_standby_direction;
			analogWrite(pin_led_rx, led_standby_value);
			led_tx_off();
		}
	}
#elif MCU_VARIANT == MCU_ESP32
	void led_indicate_standby() {
		led_standby_ticks++;
		if (led_standby_ticks > led_standby_wait) {
			led_standby_ticks = 0;
			if (led_standby_value <= led_standby_min) {
				led_standby_direction = 1;
			} else if (led_standby_value >= led_standby_max) {
				led_standby_direction = -1;
			}
			led_standby_value += led_standby_direction;
			if (led_standby_value > 253) {
				led_tx_on();
			} else {
				led_tx_off();
			}
			#if BOARD_MODEL == BOARD_LORA32_V2_1
				#if defined(EXTERNAL_LEDS)
					led_rx_off();
				#endif
			#elif BOARD_MODEL == BOARD_LORA32_V2_0
				#if defined(EXTERNAL_LEDS)
					led_rx_off();
				#endif
			#else
				led_rx_off();
			#endif
		}
	}
#elif MCU_VARIANT == MCU_LINUX
  // No LEDs available.
  void led_indicate_standby() {
    #if LIBRARY_TYPE == LIBRARY_C
      static bool printed = false;
      if (!printed) {
        std::cerr << "Indicating standby" << std::endl;
        printed = true;
      }
    #endif
  }
#endif

#if MCU_VARIANT == MCU_1284P || MCU_VARIANT == MCU_2560
	void led_indicate_not_ready() {
		led_standby_ticks++;
		if (led_standby_ticks > led_standby_wait) {
			led_standby_ticks = 0;
			if (led_standby_value <= led_standby_min) {
				led_standby_direction = 1;
			} else if (led_standby_value >= led_standby_max) {
				led_standby_direction = -1;
			}
			led_standby_value += led_standby_direction;
			analogWrite(pin_led_tx, led_standby_value);
			led_rx_off();
		}
	}
#elif MCU_VARIANT == MCU_ESP32
	void led_indicate_not_ready() {
		led_notready_ticks++;
		if (led_notready_ticks > led_notready_wait) {
			led_notready_ticks = 0;
			if (led_notready_value <= led_notready_min) {
				led_notready_direction = 1;
			} else if (led_notready_value >= led_notready_max) {
				led_notready_direction = -1;
			}
			led_notready_value += led_notready_direction;
			if (led_notready_value > 128) {
				led_tx_on();
			} else {
				led_tx_off();
			}
			#if BOARD_MODEL == BOARD_LORA32_V2_1
				#if defined(EXTERNAL_LEDS)
					led_rx_off();
				#endif
			#elif BOARD_MODEL == BOARD_LORA32_V2_0
				#if defined(EXTERNAL_LEDS)
					led_rx_off();
				#endif
			#else
				led_rx_off();
			#endif
		}
	}
#elif MCU_VARIANT == MCU_LINUX
  // No LEDs available.
  void led_indicate_not_ready() {
    #if LIBRARY_TYPE == LIBRARY_C
      static bool printed = false;
      if (!printed) {
        std::cerr << "Indicating not ready" << std::endl;
        printed = true;
      }
    #endif
  }
#endif

void escapedSerialWrite(uint8_t byte) {
	if (byte == FEND) { Serial.write(FESC); byte = TFEND; }
    if (byte == FESC) { Serial.write(FESC); byte = TFESC; }
    Serial.write(byte);
}

void kiss_indicate_reset() {
	Serial.write(FEND);
	Serial.write(CMD_RESET);
	Serial.write(CMD_RESET_BYTE);
	Serial.write(FEND);
}

void kiss_indicate_error(uint8_t error_code) {
	Serial.write(FEND);
	Serial.write(CMD_ERROR);
	Serial.write(error_code);
	Serial.write(FEND);
}

void kiss_indicate_radiostate() {
	Serial.write(FEND);
	Serial.write(CMD_RADIO_STATE);
	Serial.write(radio_online);
	Serial.write(FEND);
}

void kiss_indicate_stat_rx() {
	Serial.write(FEND);
	Serial.write(CMD_STAT_RX);
	escapedSerialWrite(stat_rx>>24);
	escapedSerialWrite(stat_rx>>16);
	escapedSerialWrite(stat_rx>>8);
	escapedSerialWrite(stat_rx);
	Serial.write(FEND);
}

void kiss_indicate_stat_tx() {
	Serial.write(FEND);
	Serial.write(CMD_STAT_TX);
	escapedSerialWrite(stat_tx>>24);
	escapedSerialWrite(stat_tx>>16);
	escapedSerialWrite(stat_tx>>8);
	escapedSerialWrite(stat_tx);
	Serial.write(FEND);
}

void kiss_indicate_stat_rssi() {
	uint8_t packet_rssi_val = (uint8_t)(last_rssi+rssi_offset);
	Serial.write(FEND);
	Serial.write(CMD_STAT_RSSI);
	escapedSerialWrite(packet_rssi_val);
	Serial.write(FEND);
}

void kiss_indicate_stat_snr() {
	Serial.write(FEND);
	Serial.write(CMD_STAT_SNR);
	escapedSerialWrite(last_snr_raw);
	Serial.write(FEND);
}

void kiss_indicate_radio_lock() {
	Serial.write(FEND);
	Serial.write(CMD_RADIO_LOCK);
	Serial.write(radio_locked);
	Serial.write(FEND);
}

void kiss_indicate_spreadingfactor() {
	Serial.write(FEND);
	Serial.write(CMD_SF);
	Serial.write((uint8_t)lora_sf);
	Serial.write(FEND);
}

void kiss_indicate_codingrate() {
	Serial.write(FEND);
	Serial.write(CMD_CR);
	Serial.write((uint8_t)lora_cr);
	Serial.write(FEND);
}

void kiss_indicate_implicit_length() {
	Serial.write(FEND);
	Serial.write(CMD_IMPLICIT);
	Serial.write(implicit_l);
	Serial.write(FEND);
}

void kiss_indicate_txpower() {
	Serial.write(FEND);
	Serial.write(CMD_TXPOWER);
	Serial.write((uint8_t)lora_txp);
	Serial.write(FEND);
}

void kiss_indicate_bandwidth() {
	Serial.write(FEND);
	Serial.write(CMD_BANDWIDTH);
	escapedSerialWrite(lora_bw>>24);
	escapedSerialWrite(lora_bw>>16);
	escapedSerialWrite(lora_bw>>8);
	escapedSerialWrite(lora_bw);
	Serial.write(FEND);
}

void kiss_indicate_frequency() {
	Serial.write(FEND);
	Serial.write(CMD_FREQUENCY);
	escapedSerialWrite(lora_freq>>24);
	escapedSerialWrite(lora_freq>>16);
	escapedSerialWrite(lora_freq>>8);
	escapedSerialWrite(lora_freq);
	Serial.write(FEND);
}

void kiss_indicate_random(uint8_t byte) {
	Serial.write(FEND);
	Serial.write(CMD_RANDOM);
	Serial.write(byte);
	Serial.write(FEND);
}

void kiss_indicate_ready() {
	Serial.write(FEND);
	Serial.write(CMD_READY);
	Serial.write(0x01);
	Serial.write(FEND);
}

void kiss_indicate_not_ready() {
	Serial.write(FEND);
	Serial.write(CMD_READY);
	Serial.write(0x00);
	Serial.write(FEND);
}

void kiss_indicate_promisc() {
	Serial.write(FEND);
	Serial.write(CMD_PROMISC);
	if (promisc) {
		Serial.write(0x01);
	} else {
		Serial.write(0x00);
	}
	Serial.write(FEND);
}

void kiss_indicate_detect() {
	Serial.write(FEND);
	Serial.write(CMD_DETECT);
	Serial.write(DETECT_RESP);
	Serial.write(FEND);
}

void kiss_indicate_version() {
	Serial.write(FEND);
	Serial.write(CMD_FW_VERSION);
	Serial.write(MAJ_VERS);
	Serial.write(MIN_VERS);
	Serial.write(FEND);
}

void kiss_indicate_platform() {
	Serial.write(FEND);
	Serial.write(CMD_PLATFORM);
	Serial.write(PLATFORM);
	Serial.write(FEND);
}

void kiss_indicate_board() {
	Serial.write(FEND);
	Serial.write(CMD_BOARD);
	Serial.write(BOARD_MODEL);
	Serial.write(FEND);
}

void kiss_indicate_mcu() {
	Serial.write(FEND);
	Serial.write(CMD_MCU);
	Serial.write(MCU_VARIANT);
	Serial.write(FEND);
}

inline bool isSplitPacket(uint8_t header) {
	return (header & FLAG_SPLIT);
}

inline uint8_t packetSequence(uint8_t header) {
	return header >> 4;
}

void setSpreadingFactor() {
	if (radio_online) LoRa.setSpreadingFactor(lora_sf);
}

void setCodingRate() {
	if (radio_online) LoRa.setCodingRate4(lora_cr);
}

void set_implicit_length(uint8_t len) {
	implicit_l = len;
	if (implicit_l != 0) {
		implicit = true;
	} else {
		implicit = false;
	}
}

void setTXPower() {
	if (radio_online) {
		if (model == MODEL_A4) LoRa.setTxPower(lora_txp, PA_OUTPUT_RFO_PIN);
		if (model == MODEL_A9) LoRa.setTxPower(lora_txp, PA_OUTPUT_PA_BOOST_PIN);
	}
}


void getBandwidth() {
	if (radio_online) {
			lora_bw = LoRa.getSignalBandwidth();
	}
}

void setBandwidth() {
	if (radio_online) {
		LoRa.setSignalBandwidth(lora_bw);
		getBandwidth();
	}
}

void getFrequency() {
	if (radio_online) {
		lora_freq = LoRa.getFrequency();
	}
}

void setFrequency() {
	if (radio_online) {
		LoRa.setFrequency(lora_freq);
		getFrequency();
	}
}

uint8_t getRandom() {
	if (radio_online) {
		return LoRa.random();
	} else {
		return 0x00;
	}
}

void promisc_enable() {
	promisc = true;
}

void promisc_disable() {
	promisc = false;
}

#if MCU_VARIANT == MCU_LINUX
  // On Linux we always use memory-mapped EEPROM
  uint8_t* eeprom_mapping = NULL;
#endif
#if LIBRARY_TYPE == LIBRARY_C
  // And when using the C library we set it up from a file descriptor.
  int eeprom_fd = 0;
#endif

void eeprom_open(int size) {
  #if MCU_VARIANT == MCU_ESP32
    // This MCU needs EEPROIM to be begun
    EEPROM.begin(size);
  #elif MCU_VARIANT == MCU_LINUX
    // We need to use file-backed EEPROM emulation
    #if LIBRARY_TYPE == LIBRARY_C
      const char* eeprom_filename = "eeprom.dat";
      // We need to be reentrant for restarts
      if (eeprom_fd <= 0) {
        eeprom_fd = open(eeprom_filename, O_RDWR | O_CREAT, 0644);
        if (eeprom_fd <= 0) {
          perror("Could not open EEPROM file");
          exit(1);
        }
        int status = ftruncate(eeprom_fd, size);
        if (status != 0) {
          perror("Could not set size of EEPROM file");
          exit(1);
        }
        // Map EEPROM into RAM
        eeprom_mapping = (uint8_t*) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, eeprom_fd, 0);
        if (eeprom_mapping == NULL) {
          perror("Could not map EEPROM file");
          exit(1);
        }
        std::cerr << "Mapped " << eeprom_filename << " as FD " << eeprom_fd << " to address " << (void*)eeprom_mapping << " size " << size << std::endl;
      } else {
        std::cerr << "Skipping EEPROM reinitialization" << std::endl;
      }
    #endif
  #endif
}

uint8_t eeprom_read(uint8_t addr) {
  #if MCU_VARIANT == MCU_LINUX
    if (!eeprom_mapping) {
      throw std::runtime_error("Tried to read EEPROM before opening it!");
    }
    int mapped_address = eeprom_addr(addr);
    return eeprom_mapping[mapped_address]; 
  #else
    return EEPROM.read(eeprom_addr(addr));
  #endif
}

bool eeprom_info_locked() {
  #if MCU_VARIANT == MCU_LINUX
    if (!eeprom_mapping) {
      return false;
    }
  #endif
	uint8_t lock_byte = eeprom_read(ADDR_INFO_LOCK);
	if (lock_byte == INFO_LOCK_BYTE) {
		return true;
	} else {
		return false;
	}
}

void eeprom_update(int mapped_addr, uint8_t byte) {
	#if MCU_VARIANT == MCU_1284P || MCU_VARIANT == MCU_2560
		EEPROM.update(mapped_addr, byte);
	#elif MCU_VARIANT == MCU_ESP32
		if (EEPROM.read(mapped_addr) != byte) {
			EEPROM.write(mapped_addr, byte);
			EEPROM.commit();
		}
  #elif MCU_VARIANT == MCU_LINUX
    if (!eeprom_mapping) {
      throw std::runtime_error("Tried to write EEPROM before opening it!");
    }
    eeprom_mapping[mapped_addr] = byte;
	#endif
}

void eeprom_write(uint8_t addr, uint8_t byte) {
	if (!eeprom_info_locked() && addr >= 0 && addr < EEPROM_RESERVED) {
		eeprom_update(eeprom_addr(addr), byte);
	} else {
		kiss_indicate_error(ERROR_EEPROM_LOCKED);
	}
}

void eeprom_erase() {
	for (int addr = 0; addr < EEPROM_RESERVED; addr++) {
		eeprom_update(eeprom_addr(addr), 0xFF);
	}
	hard_reset();
}

void eeprom_dump_info() {
	for (int addr = ADDR_PRODUCT; addr <= ADDR_INFO_LOCK; addr++) {
		uint8_t byte = eeprom_read(addr);
		escapedSerialWrite(byte);
	}
}

void eeprom_dump_config() {
	for (int addr = ADDR_CONF_SF; addr <= ADDR_CONF_OK; addr++) {
		uint8_t byte = eeprom_read(addr);
		escapedSerialWrite(byte);
	}
}

void eeprom_dump_all() {
	for (int addr = 0; addr < EEPROM_RESERVED; addr++) {
		uint8_t byte = eeprom_read(addr);
		escapedSerialWrite(byte);
	}
}

void kiss_dump_eeprom() {
	Serial.write(FEND);
	Serial.write(CMD_ROM_READ);
	eeprom_dump_all();
	Serial.write(FEND);
}

bool eeprom_product_valid() {
	uint8_t rval = eeprom_read(ADDR_PRODUCT);

	#if PLATFORM == PLATFORM_AVR
	if (rval == PRODUCT_RNODE || rval == PRODUCT_HMBRW) {
	#elif PLATFORM == PLATFORM_ESP32
	if (rval == PRODUCT_RNODE || rval == PRODUCT_HMBRW || rval == PRODUCT_TBEAM || rval == PRODUCT_T32_20 || rval == PRODUCT_T32_21) {
	#elif PLATFORM == PLATFORM_LINUX
  if (rval == PRODUCT_HMBRW) {
  #else
	if (false) {
	#endif
		return true;
	} else {
    #if LIBRARY_TYPE == LIBRARY_C
      std::cerr << "Unacceptable platform: " << std::hex << "0x" << (int)rval << std::dec << std::endl;
    #endif
		return false;
	}
}

bool eeprom_model_valid() {
	model = eeprom_read(ADDR_MODEL);
	#if BOARD_MODEL == BOARD_RNODE
	if (model == MODEL_A4 || model == MODEL_A9) {
	#elif BOARD_MODEL == BOARD_HMBRW
	if (model == MODEL_FF) {
	#elif BOARD_MODEL == BOARD_TBEAM
	if (model == MODEL_E4 || model == MODEL_E9) {
	#elif BOARD_MODEL == BOARD_LORA32_V2_0
	if (model == MODEL_B3 || model == MODEL_B8) {
	#elif BOARD_MODEL == BOARD_LORA32_V2_1
	if (model == MODEL_B4 || model == MODEL_B9) {
	#elif BOARD_MODEL == BOARD_HUZZAH32
	if (model == MODEL_FF) {
	#elif BOARD_MODEL == BOARD_GENERIC_ESP32
	if (model == MODEL_FF) {
	#else
	if (false) {
	#endif
		return true;
	} else {
    #if LIBRARY_TYPE == LIBRARY_C
      std::cerr << "Unacceptable model: " << std::hex << "0x" << (int)model << std::dec << std::endl;
    #endif
		return false;
	}
}

bool eeprom_hwrev_valid() {
	hwrev = eeprom_read(ADDR_HW_REV);
	if (hwrev != 0x00 && hwrev != 0xFF) {
		return true;
	} else {
    #if LIBRARY_TYPE == LIBRARY_C
      std::cerr << "Unacceptable revision: " << std::hex << "0x" << (int)hwrev << std::dec << std::endl;
    #endif
		return false;
	}
}

bool eeprom_checksum_valid() {
	char *data = (char*)malloc(CHECKSUMMED_SIZE);
	for (uint8_t  i = 0; i < CHECKSUMMED_SIZE; i++) {
		char byte = eeprom_read(i);
		data[i] = byte;
	}
	
	unsigned char *hash = MD5::make_hash(data, CHECKSUMMED_SIZE);
	bool checksum_valid = true;
	for (uint8_t i = 0; i < 16; i++) {
		uint8_t stored_chk_byte = eeprom_read(ADDR_CHKSUM+i);
		uint8_t calced_chk_byte = (uint8_t)hash[i];
		if (stored_chk_byte != calced_chk_byte) {
			checksum_valid = false;
		}
	}

	free(hash);
	free(data);
	return checksum_valid;
}

bool eeprom_have_conf() {
	if (eeprom_read(ADDR_CONF_OK) == CONF_OK_BYTE) {
		return true;
	} else {
		return false;
	}
}

void eeprom_conf_load() {
	if (eeprom_have_conf()) {
		lora_sf = eeprom_read(ADDR_CONF_SF);
		lora_cr = eeprom_read(ADDR_CONF_CR);
		lora_txp = eeprom_read(ADDR_CONF_TXP);
		lora_freq = (uint32_t)eeprom_read(ADDR_CONF_FREQ+0x00) << 24 | (uint32_t)eeprom_read(ADDR_CONF_FREQ+0x01) << 16 | (uint32_t)eeprom_read(ADDR_CONF_FREQ+0x02) << 8 | (uint32_t)eeprom_read(ADDR_CONF_FREQ+0x03);
		lora_bw = (uint32_t)eeprom_read(ADDR_CONF_BW+0x00) << 24 | (uint32_t)eeprom_read(ADDR_CONF_BW+0x01) << 16 | (uint32_t)eeprom_read(ADDR_CONF_BW+0x02) << 8 | (uint32_t)eeprom_read(ADDR_CONF_BW+0x03);
	}
}

void eeprom_conf_save() {
	if (hw_ready && radio_online) {
		eeprom_update(eeprom_addr(ADDR_CONF_SF), lora_sf);
		eeprom_update(eeprom_addr(ADDR_CONF_CR), lora_cr);
		eeprom_update(eeprom_addr(ADDR_CONF_TXP), lora_txp);

		eeprom_update(eeprom_addr(ADDR_CONF_BW)+0x00, lora_bw>>24);
		eeprom_update(eeprom_addr(ADDR_CONF_BW)+0x01, lora_bw>>16);
		eeprom_update(eeprom_addr(ADDR_CONF_BW)+0x02, lora_bw>>8);
		eeprom_update(eeprom_addr(ADDR_CONF_BW)+0x03, lora_bw);

		eeprom_update(eeprom_addr(ADDR_CONF_FREQ)+0x00, lora_freq>>24);
		eeprom_update(eeprom_addr(ADDR_CONF_FREQ)+0x01, lora_freq>>16);
		eeprom_update(eeprom_addr(ADDR_CONF_FREQ)+0x02, lora_freq>>8);
		eeprom_update(eeprom_addr(ADDR_CONF_FREQ)+0x03, lora_freq);

		eeprom_update(eeprom_addr(ADDR_CONF_OK), CONF_OK_BYTE);
		led_indicate_info(10);
	} else {
		led_indicate_warning(10);
	}
}

void eeprom_conf_delete() {
	eeprom_update(eeprom_addr(ADDR_CONF_OK), 0x00);
}

void unlock_rom() {
	led_indicate_error(50);
	eeprom_erase();
}

typedef struct FIFOBuffer
{
  unsigned char *begin;
  unsigned char *end;
  unsigned char * volatile head;
  unsigned char * volatile tail;
} FIFOBuffer;

inline bool fifo_isempty(const FIFOBuffer *f) {
  return f->head == f->tail;
}

inline bool fifo_isfull(const FIFOBuffer *f) {
  return ((f->head == f->begin) && (f->tail == f->end)) || (f->tail == f->head - 1);
}

inline void fifo_push(FIFOBuffer *f, unsigned char c) {
  *(f->tail) = c;
  
  if (f->tail == f->end) {
    f->tail = f->begin;
  } else {
    f->tail++;
  }
}

inline unsigned char fifo_pop(FIFOBuffer *f) {
  if(f->head == f->end) {
    f->head = f->begin;
    return *(f->end);
  } else {
    return *(f->head++);
  }
}

inline void fifo_flush(FIFOBuffer *f) {
  f->head = f->tail;
}

#if SERIAL_EVENTS == SERIAL_INTERRUPT
	static inline bool fifo_isempty_locked(const FIFOBuffer *f) {
	  bool result;
	  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
	    result = fifo_isempty(f);
	  }
	  return result;
	}

	static inline bool fifo_isfull_locked(const FIFOBuffer *f) {
	  bool result;
	  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
	    result = fifo_isfull(f);
	  }
	  return result;
	}

	static inline void fifo_push_locked(FIFOBuffer *f, unsigned char c) {
	  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
	    fifo_push(f, c);
	  }
	}
#endif

/*
static inline unsigned char fifo_pop_locked(FIFOBuffer *f) {
  unsigned char c;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    c = fifo_pop(f);
  }
  return c;
}
*/

inline void fifo_init(FIFOBuffer *f, unsigned char *buffer, size_t size) {
  f->head = f->tail = f->begin = buffer;
  f->end = buffer + size;
}

inline size_t fifo_len(FIFOBuffer *f) {
  return f->end - f->begin;
}

typedef struct FIFOBuffer16
{
  uint16_t *begin;
  uint16_t *end;
  uint16_t * volatile head;
  uint16_t * volatile tail;
} FIFOBuffer16;

inline bool fifo16_isempty(const FIFOBuffer16 *f) {
  return f->head == f->tail;
}

inline bool fifo16_isfull(const FIFOBuffer16 *f) {
  return ((f->head == f->begin) && (f->tail == f->end)) || (f->tail == f->head - 1);
}

inline void fifo16_push(FIFOBuffer16 *f, uint16_t c) {
  *(f->tail) = c;

  if (f->tail == f->end) {
    f->tail = f->begin;
  } else {
    f->tail++;
  }
}

inline uint16_t fifo16_pop(FIFOBuffer16 *f) {
  if(f->head == f->end) {
    f->head = f->begin;
    return *(f->end);
  } else {
    return *(f->head++);
  }
}

inline void fifo16_flush(FIFOBuffer16 *f) {
  f->head = f->tail;
}

#if SERIAL_EVENTS == SERIAL_INTERRUPT
	static inline bool fifo16_isempty_locked(const FIFOBuffer16 *f) {
	  bool result;
	  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
	    result = fifo16_isempty(f);
	  }

	  return result;
	}
#endif

/*
static inline bool fifo16_isfull_locked(const FIFOBuffer16 *f) {
  bool result;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    result = fifo16_isfull(f);
  }
  return result;
}


static inline void fifo16_push_locked(FIFOBuffer16 *f, uint16_t c) {
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    fifo16_push(f, c);
  }
}

static inline size_t fifo16_pop_locked(FIFOBuffer16 *f) {
  size_t c;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    c = fifo16_pop(f);
  }
  return c;
}
*/

inline void fifo16_init(FIFOBuffer16 *f, uint16_t *buffer, uint16_t size) {
  f->head = f->tail = f->begin = buffer;
  f->end = buffer + size;
}

inline uint16_t fifo16_len(FIFOBuffer16 *f) {
  return (f->end - f->begin);
}

#if BOARD_MODEL == BOARD_TBEAM
	#include <axp20x.h>
	AXP20X_Class PMU;

	bool initPMU()
	{
	    if (PMU.begin(Wire, AXP192_SLAVE_ADDRESS) == AXP_FAIL) {
	        return false;
	    }
	    /*
	     * The charging indicator can be turned on or off
	     * * * */
	    PMU.setChgLEDMode(AXP20X_LED_OFF);

	    /*
	    * The default ESP32 power supply has been turned on,
	    * no need to set, please do not set it, if it is turned off,
	    * it will not be able to program
	    *
	    *   PMU.setDCDC3Voltage(3300);
	    *   PMU.setPowerOutPut(AXP192_DCDC3, AXP202_ON);
	    *
	    * * * */

	    /*
	     *   Turn off unused power sources to save power
	     * **/

	    PMU.setPowerOutPut(AXP192_DCDC1, AXP202_OFF);
	    PMU.setPowerOutPut(AXP192_DCDC2, AXP202_OFF);
	    PMU.setPowerOutPut(AXP192_LDO2, AXP202_OFF);
	    PMU.setPowerOutPut(AXP192_LDO3, AXP202_OFF);
	    PMU.setPowerOutPut(AXP192_EXTEN, AXP202_OFF);

	    /*
	     * Set the power of LoRa and GPS module to 3.3V
	     **/
	    PMU.setLDO2Voltage(3300);   //LoRa VDD
	    PMU.setLDO3Voltage(3300);   //GPS  VDD
	    PMU.setDCDC1Voltage(3300);  //3.3V Pin next to 21 and 22 is controlled by DCDC1

	    PMU.setPowerOutPut(AXP192_DCDC1, AXP202_ON);

	    // Turn on SX1276
	    PMU.setPowerOutPut(AXP192_LDO2, AXP202_ON);

	    // Turn off GPS
	    PMU.setPowerOutPut(AXP192_LDO3, AXP202_OFF);

	    pinMode(PMU_IRQ, INPUT_PULLUP);
	    attachInterrupt(PMU_IRQ, [] {
	        // pmu_irq = true;
	    }, FALLING);

	    PMU.adc1Enable(AXP202_VBUS_VOL_ADC1 |
	                   AXP202_VBUS_CUR_ADC1 |
	                   AXP202_BATT_CUR_ADC1 |
	                   AXP202_BATT_VOL_ADC1,
	                   AXP202_ON);

	    PMU.enableIRQ(AXP202_VBUS_REMOVED_IRQ |
	                  AXP202_VBUS_CONNECT_IRQ |
	                  AXP202_BATT_REMOVED_IRQ |
	                  AXP202_BATT_CONNECT_IRQ,
	                  AXP202_ON);
	    PMU.clearIRQ();

	    return true;
	}

	void disablePeripherals()
	{
	    PMU.setPowerOutPut(AXP192_DCDC1, AXP202_OFF);
	    PMU.setPowerOutPut(AXP192_LDO2, AXP202_OFF);
	    PMU.setPowerOutPut(AXP192_LDO3, AXP202_OFF);
	}

#endif
