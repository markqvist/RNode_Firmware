#include <Arduino.h>
#include <SPI.h>
#include "Utilities.h"

FIFOBuffer serialFIFO;
uint8_t serialBuffer[CONFIG_UART_BUFFER_SIZE+1];

FIFOBuffer16 packet_starts;
uint16_t packet_starts_buf[CONFIG_QUEUE_MAX_LENGTH+1];

FIFOBuffer16 packet_lengths;
uint16_t packet_lengths_buf[CONFIG_QUEUE_MAX_LENGTH+1];

uint8_t packet_queue[CONFIG_QUEUE_SIZE];

volatile uint8_t queue_height = 0;
volatile uint16_t queued_bytes = 0;
volatile uint16_t queue_cursor = 0;
volatile uint16_t current_packet_start = 0;
volatile bool serial_buffering = false;
#if HAS_BLUETOOTH
  bool bt_init_ran = false;
#endif

#if HAS_CONSOLE
  #include "Console.h"
#endif

char sbuf[128];

#if MCU_VARIANT == MCU_ESP32
  bool packet_ready = false;
#endif

void setup() {
  #if MCU_VARIANT == MCU_ESP32
    boot_seq();
    EEPROM.begin(EEPROM_SIZE);
    Serial.setRxBufferSize(CONFIG_UART_BUFFER_SIZE);
  #endif

  // Seed the PRNG
  randomSeed(analogRead(0));

  // Initialise serial communication
  memset(serialBuffer, 0, sizeof(serialBuffer));
  fifo_init(&serialFIFO, serialBuffer, CONFIG_UART_BUFFER_SIZE);

  Serial.begin(serial_baudrate);
  while (!Serial);

  serial_interrupt_init();

  // Configure input and output pins
  #if HAS_NP == false
    pinMode(pin_led_rx, OUTPUT);
    pinMode(pin_led_tx, OUTPUT);
  #endif

  // Initialise buffers
  memset(pbuf, 0, sizeof(pbuf));
  memset(cmdbuf, 0, sizeof(cmdbuf));
  
  memset(packet_queue, 0, sizeof(packet_queue));

  memset(packet_starts_buf, 0, sizeof(packet_starts_buf));
  fifo16_init(&packet_starts, packet_starts_buf, CONFIG_QUEUE_MAX_LENGTH);
  
  memset(packet_lengths_buf, 0, sizeof(packet_starts_buf));
  fifo16_init(&packet_lengths, packet_lengths_buf, CONFIG_QUEUE_MAX_LENGTH);

  // Set chip select, reset and interrupt
  // pins for the LoRa module
  LoRa.setPins(pin_cs, pin_reset, pin_dio);
  
  if (LoRa.preInit()) {
    sx1276_installed = true;
    uint32_t lfr = LoRa.getFrequency();
    if (lfr == 0) {
      // Normal boot
    } else if (lfr == M_FRQ_R) {
      // Quick reboot
      #if HAS_CONSOLE
        if (rtc_get_reset_reason(0) == POWERON_RESET) {
          console_active = true;
        }
      #endif
    } else {
      // Unknown boot
    }
    LoRa.setFrequency(M_FRQ_S);
  } else {
    sx1276_installed = false;
  }

  #if HAS_DISPLAY
    disp_ready = display_init();
    update_display();
  #endif

  #if MCU_VARIANT == MCU_ESP32
    #if HAS_PMU == true
      pmu_ready = init_pmu();
    #endif

    #if HAS_BLUETOOTH
      bt_init();
      bt_init_ran = true;
    #endif

    if (console_active) {
      console_start();
    } else {
      kiss_indicate_reset();
    }
  #endif

  // Validate board health, EEPROM and config
  validate_status();

  LoRa.setFrequency(0);
}

void lora_receive() {
  if (!implicit) {
    LoRa.receive();
  } else {
    LoRa.receive(implicit_l);
  }
}

inline void kiss_write_packet() {
  serial_write(FEND);
  serial_write(CMD_DATA);
  for (uint16_t i = 0; i < read_len; i++) {
    uint8_t byte = pbuf[i];
    if (byte == FEND) { serial_write(FESC); byte = TFEND; }
    if (byte == FESC) { serial_write(FESC); byte = TFESC; }
    serial_write(byte);
  }
  serial_write(FEND);
  read_len = 0;
  #if MCU_VARIANT == MCU_ESP32
    packet_ready = false;
  #endif
}

inline void getPacketData(uint16_t len) {
  while (len-- && read_len < MTU) {
    pbuf[read_len++] = LoRa.read();
  }
}

void ISR_VECT receive_callback(int packet_size) {
  if (!promisc) {
    // The standard operating mode allows large
    // packets with a payload up to 500 bytes,
    // by combining two raw LoRa packets.
    // We read the 1-byte header and extract
    // packet sequence number and split flags
    uint8_t header   = LoRa.read(); packet_size--;
    uint8_t sequence = packetSequence(header);
    bool    ready    = false;

    if (isSplitPacket(header) && seq == SEQ_UNSET) {
      // This is the first part of a split
      // packet, so we set the seq variable
      // and add the data to the buffer
      read_len = 0;
      seq = sequence;

      #if MCU_VARIANT != MCU_ESP32
        last_rssi = LoRa.packetRssi();
        last_snr_raw = LoRa.packetSnrRaw();
      #endif

      getPacketData(packet_size);

    } else if (isSplitPacket(header) && seq == sequence) {
      // This is the second part of a split
      // packet, so we add it to the buffer
      // and set the ready flag.
      
      #if MCU_VARIANT != MCU_ESP32
        last_rssi = (last_rssi+LoRa.packetRssi())/2;
        last_snr_raw = (last_snr_raw+LoRa.packetSnrRaw())/2;
      #endif
      
      getPacketData(packet_size);
      seq = SEQ_UNSET;
      ready = true;

    } else if (isSplitPacket(header) && seq != sequence) {
      // This split packet does not carry the
      // same sequence id, so we must assume
      // that we are seeing the first part of
      // a new split packet.
      read_len = 0;
      seq = sequence;

      #if MCU_VARIANT != MCU_ESP32
        last_rssi = LoRa.packetRssi();
        last_snr_raw = LoRa.packetSnrRaw();
      #endif

      getPacketData(packet_size);

    } else if (!isSplitPacket(header)) {
      // This is not a split packet, so we
      // just read it and set the ready
      // flag to true.

      if (seq != SEQ_UNSET) {
        // If we already had part of a split
        // packet in the buffer, we clear it.
        read_len = 0;
        seq = SEQ_UNSET;
      }

      #if MCU_VARIANT != MCU_ESP32
        last_rssi = LoRa.packetRssi();
        last_snr_raw = LoRa.packetSnrRaw();
      #endif

      getPacketData(packet_size);
      ready = true;
    }

    if (ready) {
      #if MCU_VARIANT != MCU_ESP32
        // We first signal the RSSI of the
        // recieved packet to the host.
        kiss_indicate_stat_rssi();
        kiss_indicate_stat_snr();

        // And then write the entire packet
        kiss_write_packet();
      #else
        packet_ready = true;
      #endif
    }  
  } else {
    // In promiscuous mode, raw packets are
    // output directly to the host
    read_len = 0;

    #if MCU_VARIANT != MCU_ESP32
      last_rssi = LoRa.packetRssi();
      last_snr_raw = LoRa.packetSnrRaw();
      getPacketData(packet_size);

      // We first signal the RSSI of the
      // recieved packet to the host.
      kiss_indicate_stat_rssi();
      kiss_indicate_stat_snr();

      // And then write the entire packet
      kiss_write_packet();

    #else
      getPacketData(packet_size);
      packet_ready = true;
    #endif
  }
}

bool startRadio() {
  update_radio_lock();
  if (!radio_online) {
    if (!radio_locked && hw_ready) {
      if (!LoRa.begin(lora_freq)) {
        // The radio could not be started.
        // Indicate this failure over both the
        // serial port and with the onboard LEDs
        radio_error = true;
        kiss_indicate_error(ERROR_INITRADIO);
        led_indicate_error(0);
        return false;
      } else {
        radio_online = true;

        setTXPower();
        setBandwidth();
        setSpreadingFactor();
        setCodingRate();
        getFrequency();

        LoRa.enableCrc();

        LoRa.onReceive(receive_callback);

        lora_receive();

        // Flash an info pattern to indicate
        // that the radio is now on
        kiss_indicate_radiostate();
        led_indicate_info(3);
        return true;
      }

    } else {
      // Flash a warning pattern to indicate
      // that the radio was locked, and thus
      // not started
      radio_online = false;
      kiss_indicate_radiostate();
      led_indicate_warning(3);
      return false;
    }
  } else {
    // If radio is already on, we silently
    // ignore the request.
    kiss_indicate_radiostate();
    return true;
  }
}

void stopRadio() {
  LoRa.end();
  radio_online = false;
}

void update_radio_lock() {
  if (lora_freq != 0 && lora_bw != 0 && lora_txp != 0xFF && lora_sf != 0) {
    radio_locked = false;
  } else {
    radio_locked = true;
  }
}

bool queueFull() {
  return (queue_height >= CONFIG_QUEUE_MAX_LENGTH || queued_bytes >= CONFIG_QUEUE_SIZE);
}

volatile bool queue_flushing = false;
void flushQueue(void) {
  if (!queue_flushing) {
    queue_flushing = true;

    uint16_t processed = 0;

    #if MCU_VARIANT == MCU_ESP32
    while (!fifo16_isempty(&packet_starts)) {
    #else
    while (!fifo16_isempty_locked(&packet_starts)) {
    #endif

      uint16_t start = fifo16_pop(&packet_starts);
      uint16_t length = fifo16_pop(&packet_lengths);

      if (length >= MIN_L && length <= MTU) {
        for (uint16_t i = 0; i < length; i++) {
          uint16_t pos = (start+i)%CONFIG_QUEUE_SIZE;
          tbuf[i] = packet_queue[pos];
        }

        transmit(length);
        processed++;
      }
    }
  }

  queue_height = 0;
  queued_bytes = 0;
  queue_flushing = false;
}

void transmit(uint16_t size) {
  if (radio_online) {
    if (!promisc) {
      led_tx_on();
      uint16_t  written = 0;
      uint8_t header  = random(256) & 0xF0;

      if (size > SINGLE_MTU - HEADER_L) {
        header = header | FLAG_SPLIT;
      }

      LoRa.beginPacket();
      LoRa.write(header); written++;

      for (uint16_t i=0; i < size; i++) {
        LoRa.write(tbuf[i]);  

        written++;

        if (written == 255) {
          LoRa.endPacket();
          LoRa.beginPacket();
          LoRa.write(header);
          written = 1;
        }
      }

      LoRa.endPacket();
      led_tx_off();

      lora_receive();
    } else {
      // In promiscuous mode, we only send out
      // plain raw LoRa packets with a maximum
      // payload of 255 bytes
      led_tx_on();
      uint16_t  written = 0;
      
      // Cap packets at 255 bytes
      if (size > SINGLE_MTU) {
        size = SINGLE_MTU;
      }

      // If implicit header mode has been set,
      // set packet length to payload data length
      if (!implicit) {
        LoRa.beginPacket();
      } else {
        LoRa.beginPacket(size);
      }

      for (uint16_t i=0; i < size; i++) {
        LoRa.write(tbuf[i]);

        written++;
      }
      LoRa.endPacket();
      led_tx_off();

      lora_receive();
    }
  } else {
    kiss_indicate_error(ERROR_TXFAILED);
    led_indicate_error(5);
  }
}

void serialCallback(uint8_t sbyte) {
  if (IN_FRAME && sbyte == FEND && command == CMD_DATA) {
    IN_FRAME = false;

    if (!fifo16_isfull(&packet_starts) && queued_bytes < CONFIG_QUEUE_SIZE) {
        uint16_t s = current_packet_start;
        int16_t e = queue_cursor-1; if (e == -1) e = CONFIG_QUEUE_SIZE-1;
        uint16_t l;

        if (s != e) {
            l = (s < e) ? e - s + 1 : CONFIG_QUEUE_SIZE - s + e + 1;
        } else {
            l = 1;
        }

        if (l >= MIN_L) {
            queue_height++;

            fifo16_push(&packet_starts, s);
            fifo16_push(&packet_lengths, l);

            current_packet_start = queue_cursor;
        }

    }

  } else if (sbyte == FEND) {
    IN_FRAME = true;
    command = CMD_UNKNOWN;
    frame_len = 0;
  } else if (IN_FRAME && frame_len < MTU) {
    // Have a look at the command byte first
    if (frame_len == 0 && command == CMD_UNKNOWN) {
        command = sbyte;
    } else if (command == CMD_DATA) {
        if (bt_state != BT_STATE_CONNECTED) cable_state = CABLE_STATE_CONNECTED;
        if (sbyte == FESC) {
            ESCAPE = true;
        } else {
            if (ESCAPE) {
                if (sbyte == TFEND) sbyte = FEND;
                if (sbyte == TFESC) sbyte = FESC;
                ESCAPE = false;
            }
            if (queue_height < CONFIG_QUEUE_MAX_LENGTH && queued_bytes < CONFIG_QUEUE_SIZE) {
              queued_bytes++;
              packet_queue[queue_cursor++] = sbyte;
              if (queue_cursor == CONFIG_QUEUE_SIZE) queue_cursor = 0;
            }
        }
    } else if (command == CMD_FREQUENCY) {
      if (sbyte == FESC) {
            ESCAPE = true;
        } else {
            if (ESCAPE) {
                if (sbyte == TFEND) sbyte = FEND;
                if (sbyte == TFESC) sbyte = FESC;
                ESCAPE = false;
            }
            if (frame_len < CMD_L) cmdbuf[frame_len++] = sbyte;
        }

        if (frame_len == 4) {
          uint32_t freq = (uint32_t)cmdbuf[0] << 24 | (uint32_t)cmdbuf[1] << 16 | (uint32_t)cmdbuf[2] << 8 | (uint32_t)cmdbuf[3];

          if (freq == 0) {
            kiss_indicate_frequency();
          } else {
            lora_freq = freq;
            if (op_mode == MODE_HOST) setFrequency();
            kiss_indicate_frequency();
          }
        }
    } else if (command == CMD_BANDWIDTH) {
      if (sbyte == FESC) {
            ESCAPE = true;
        } else {
            if (ESCAPE) {
                if (sbyte == TFEND) sbyte = FEND;
                if (sbyte == TFESC) sbyte = FESC;
                ESCAPE = false;
            }
            if (frame_len < CMD_L) cmdbuf[frame_len++] = sbyte;
        }

        if (frame_len == 4) {
          uint32_t bw = (uint32_t)cmdbuf[0] << 24 | (uint32_t)cmdbuf[1] << 16 | (uint32_t)cmdbuf[2] << 8 | (uint32_t)cmdbuf[3];

          if (bw == 0) {
            kiss_indicate_bandwidth();
          } else {
            lora_bw = bw;
            if (op_mode == MODE_HOST) setBandwidth();
            kiss_indicate_bandwidth();
          }
        }
    } else if (command == CMD_TXPOWER) {
      if (sbyte == 0xFF) {
        kiss_indicate_txpower();
      } else {
        int txp = sbyte;
        if (txp > 17) txp = 17;

        lora_txp = txp;
        if (op_mode == MODE_HOST) setTXPower();
        kiss_indicate_txpower();
      }
    } else if (command == CMD_SF) {
      if (sbyte == 0xFF) {
        kiss_indicate_spreadingfactor();
      } else {
        int sf = sbyte;
        if (sf < 6) sf = 6;
        if (sf > 12) sf = 12;

        lora_sf = sf;
        if (op_mode == MODE_HOST) setSpreadingFactor();
        kiss_indicate_spreadingfactor();
      }
    } else if (command == CMD_CR) {
      if (sbyte == 0xFF) {
        kiss_indicate_codingrate();
      } else {
        int cr = sbyte;
        if (cr < 5) cr = 5;
        if (cr > 8) cr = 8;

        lora_cr = cr;
        if (op_mode == MODE_HOST) setCodingRate();
        kiss_indicate_codingrate();
      }
    } else if (command == CMD_IMPLICIT) {
      set_implicit_length(sbyte);
      kiss_indicate_implicit_length();
    } else if (command == CMD_LEAVE) {
      if (sbyte == 0xFF) {
        cable_state   = CABLE_STATE_DISCONNECTED;
        current_rssi  = -292;
        last_rssi     = -292;
        last_rssi_raw = 0x00;
        last_snr_raw  = 0x80;
      }
    } else if (command == CMD_RADIO_STATE) {
      if (bt_state != BT_STATE_CONNECTED) cable_state = CABLE_STATE_CONNECTED;
      if (sbyte == 0xFF) {
        kiss_indicate_radiostate();
      } else if (sbyte == 0x00) {
        stopRadio();
        kiss_indicate_radiostate();
      } else if (sbyte == 0x01) {
        startRadio();
        kiss_indicate_radiostate();
      }
    } else if (command == CMD_STAT_RX) {
      kiss_indicate_stat_rx();
    } else if (command == CMD_STAT_TX) {
      kiss_indicate_stat_tx();
    } else if (command == CMD_STAT_RSSI) {
      kiss_indicate_stat_rssi();
    } else if (command == CMD_RADIO_LOCK) {
      update_radio_lock();
      kiss_indicate_radio_lock();
    } else if (command == CMD_BLINK) {
      led_indicate_info(sbyte);
    } else if (command == CMD_RANDOM) {
      kiss_indicate_random(getRandom());
    } else if (command == CMD_DETECT) {
      if (sbyte == DETECT_REQ) {
        if (bt_state != BT_STATE_CONNECTED) cable_state = CABLE_STATE_CONNECTED;
        kiss_indicate_detect();
      }
    } else if (command == CMD_PROMISC) {
      if (sbyte == 0x01) {
        promisc_enable();
      } else if (sbyte == 0x00) {
        promisc_disable();
      }
      kiss_indicate_promisc();
    } else if (command == CMD_READY) {
      if (!queueFull()) {
        kiss_indicate_ready();
      } else {
        kiss_indicate_not_ready();
      }
    } else if (command == CMD_UNLOCK_ROM) {
      if (sbyte == ROM_UNLOCK_BYTE) {
        unlock_rom();
      }
    } else if (command == CMD_RESET) {
      if (sbyte == CMD_RESET_BYTE) {
        hard_reset();
      }
    } else if (command == CMD_ROM_READ) {
      kiss_dump_eeprom();
    } else if (command == CMD_ROM_WRITE) {
      if (sbyte == FESC) {
            ESCAPE = true;
        } else {
            if (ESCAPE) {
                if (sbyte == TFEND) sbyte = FEND;
                if (sbyte == TFESC) sbyte = FESC;
                ESCAPE = false;
            }
            if (frame_len < CMD_L) cmdbuf[frame_len++] = sbyte;
        }

        if (frame_len == 2) {
          eeprom_write(cmdbuf[0], cmdbuf[1]);
        }
    } else if (command == CMD_FW_VERSION) {
      kiss_indicate_version();
    } else if (command == CMD_PLATFORM) {
      kiss_indicate_platform();
    } else if (command == CMD_MCU) {
      kiss_indicate_mcu();
    } else if (command == CMD_BOARD) {
      kiss_indicate_board();
    } else if (command == CMD_CONF_SAVE) {
      eeprom_conf_save();
    } else if (command == CMD_CONF_DELETE) {
      eeprom_conf_delete();
    } else if (command == CMD_FB_EXT) {
      #if HAS_DISPLAY == true
        if (sbyte == 0xFF) {
          kiss_indicate_fbstate();
        } else if (sbyte == 0x00) {
          ext_fb_disable();
          kiss_indicate_fbstate();
        } else if (sbyte == 0x01) {
          ext_fb_enable();
          kiss_indicate_fbstate();
        }
      #endif
    } else if (command == CMD_FB_WRITE) {
      if (sbyte == FESC) {
            ESCAPE = true;
        } else {
            if (ESCAPE) {
                if (sbyte == TFEND) sbyte = FEND;
                if (sbyte == TFESC) sbyte = FESC;
                ESCAPE = false;
            }
            if (frame_len < CMD_L) cmdbuf[frame_len++] = sbyte;
        }
        #if HAS_DISPLAY
          if (frame_len == 9) {
            uint8_t line = cmdbuf[0];
            if (line > 63) line = 63;
            int fb_o = line*8; 
            memcpy(fb+fb_o, cmdbuf+1, 8);
          }
        #endif
    } else if (command == CMD_FB_READ) {
      if (sbyte != 0x00) {
        kiss_indicate_fb();
      }
    } else if (command == CMD_DEV_HASH) {
      #if MCU_VARIANT == MCU_ESP32
        if (sbyte != 0x00) {
          kiss_indicate_device_hash();
        }
      #endif
    } else if (command == CMD_DEV_SIG) {
      #if MCU_VARIANT == MCU_ESP32
        if (sbyte == FESC) {
              ESCAPE = true;
          } else {
              if (ESCAPE) {
                  if (sbyte == TFEND) sbyte = FEND;
                  if (sbyte == TFESC) sbyte = FESC;
                  ESCAPE = false;
              }
              if (frame_len < CMD_L) cmdbuf[frame_len++] = sbyte;
          }

          if (frame_len == DEV_SIG_LEN) {
            memcpy(dev_sig, cmdbuf, DEV_SIG_LEN);
            device_save_signature();
          }
      #endif
    } else if (command == CMD_FW_UPD) {
      if (sbyte == 0x01) {
        firmware_update_mode = true;
      } else {
        firmware_update_mode = false;
      }
    } else if (command == CMD_HASHES) {
      #if MCU_VARIANT == MCU_ESP32
        if (sbyte == 0x01) {
          kiss_indicate_target_fw_hash();
        } else if (sbyte == 0x02) {
          kiss_indicate_fw_hash();
        } else if (sbyte == 0x03) {
          kiss_indicate_bootloader_hash();
        } else if (sbyte == 0x04) {
          kiss_indicate_partition_table_hash();
        }
      #endif
    } else if (command == CMD_FW_HASH) {
      #if MCU_VARIANT == MCU_ESP32
        if (sbyte == FESC) {
              ESCAPE = true;
          } else {
              if (ESCAPE) {
                  if (sbyte == TFEND) sbyte = FEND;
                  if (sbyte == TFESC) sbyte = FESC;
                  ESCAPE = false;
              }
              if (frame_len < CMD_L) cmdbuf[frame_len++] = sbyte;
          }

          if (frame_len == DEV_HASH_LEN) {
            memcpy(dev_firmware_hash_target, cmdbuf, DEV_SIG_LEN);
            device_save_firmware_hash();
          }
      #endif
    } else if (command == CMD_BT_CTRL) {
      #if HAS_BLUETOOTH
        if (sbyte == 0x00) {
          bt_stop();
          bt_conf_save(false);
        } else if (sbyte == 0x01) {
          bt_start();
          bt_conf_save(true);
        } else if (sbyte == 0x02) {
          bt_enable_pairing();
        }
      #endif
    } else if (command == CMD_DISP_INT) {
      #if HAS_DISPLAY
        if (sbyte == FESC) {
            ESCAPE = true;
        } else {
            if (ESCAPE) {
                if (sbyte == TFEND) sbyte = FEND;
                if (sbyte == TFESC) sbyte = FESC;
                ESCAPE = false;
            }
            display_intensity = sbyte;
        }

      #endif
    }
  }
}

#if MCU_VARIANT == MCU_ESP32
  portMUX_TYPE update_lock = portMUX_INITIALIZER_UNLOCKED;
#endif

void updateModemStatus() {
  #if MCU_VARIANT == MCU_ESP32
    portENTER_CRITICAL(&update_lock);
  #endif

  uint8_t status = LoRa.modemStatus();
  current_rssi = LoRa.currentRssi();
  last_status_update = millis();

  #if MCU_VARIANT == MCU_ESP32
    portEXIT_CRITICAL(&update_lock);
  #endif

  if (status & SIG_DETECT == SIG_DETECT) { stat_signal_detected = true; } else { stat_signal_detected = false; }
  if (status & SIG_SYNCED == SIG_SYNCED) { stat_signal_synced = true; } else { stat_signal_synced = false; }
  if (status & RX_ONGOING == RX_ONGOING) { stat_rx_ongoing = true; } else { stat_rx_ongoing = false; }

  if (stat_signal_detected || stat_signal_synced || stat_rx_ongoing) {
    if (dcd_count < dcd_threshold) {
      dcd_count++;
      dcd = true;
    } else {
      dcd = true;
      dcd_led = true;
    }
  } else {
    if (dcd_count > 0) {
      dcd_count--;
    } else {
      dcd_led = false;
    }
    dcd = false;
  }

  if (dcd_led) {
    led_rx_on();
  } else {
    led_rx_off();
  }
}

void checkModemStatus() {
  if (millis()-last_status_update >= status_interval_ms) {
    updateModemStatus();
  }
}

void validate_status() {
  #if MCU_VARIANT == MCU_1284P
      uint8_t boot_flags = OPTIBOOT_MCUSR;
      uint8_t F_POR = PORF;
      uint8_t F_BOR = BORF;
      uint8_t F_WDR = WDRF;
  #elif MCU_VARIANT == MCU_2560
      uint8_t boot_flags = OPTIBOOT_MCUSR;
      if (boot_flags == 0x00) boot_flags = 0x03;
      uint8_t F_POR = PORF;
      uint8_t F_BOR = BORF;
      uint8_t F_WDR = WDRF;
  #elif MCU_VARIANT == MCU_ESP32
      // TODO: Get ESP32 boot flags
      uint8_t boot_flags = 0x02;
      uint8_t F_POR = 0x00;
      uint8_t F_BOR = 0x00;
      uint8_t F_WDR = 0x01;
  #endif

  if (hw_ready || device_init_done) {
    hw_ready = false;
    Serial.write("Error, invalid hardware check state\r\n");
    #if HAS_DISPLAY
      if (disp_ready) {
        device_init_done = true;
        update_display();
      }
    #endif
    led_indicate_boot_error();
  }

  if (boot_flags & (1<<F_POR)) {
    boot_vector = START_FROM_POWERON;
  } else if (boot_flags & (1<<F_BOR)) {
    boot_vector = START_FROM_BROWNOUT;
  } else if (boot_flags & (1<<F_WDR)) {
    boot_vector = START_FROM_BOOTLOADER;
  } else {
      Serial.write("Error, indeterminate boot vector\r\n");
      #if HAS_DISPLAY
        if (disp_ready) {
          device_init_done = true;
          update_display();
        }
      #endif
      led_indicate_boot_error();
  }

  if (boot_vector == START_FROM_BOOTLOADER || boot_vector == START_FROM_POWERON) {
    if (eeprom_lock_set()) {
      if (eeprom_product_valid() && eeprom_model_valid() && eeprom_hwrev_valid()) {
        if (eeprom_checksum_valid()) {
          eeprom_ok = true;
          if (sx1276_installed) {
            #if PLATFORM == PLATFORM_ESP32
              if (device_init()) {
                hw_ready = true;
              } else {
                hw_ready = false;
              }
            #else
              hw_ready = true;
            #endif
          } else {
            hw_ready = false;
            Serial.write("No SX1276/SX1278 radio module found\r\n");
            #if HAS_DISPLAY
              if (disp_ready) {
                device_init_done = true;
                update_display();
              }
            #endif
          }
          
          if (hw_ready && eeprom_have_conf()) {
            eeprom_conf_load();
            op_mode = MODE_TNC;
            startRadio();
          }
        } else {
          hw_ready = false;
          #if HAS_DISPLAY
            if (disp_ready) {
              device_init_done = true;
              update_display();
            }
          #endif
        }
      } else {
        hw_ready = false;
        #if HAS_DISPLAY
          if (disp_ready) {
            device_init_done = true;
            update_display();
          }
        #endif
      }
    } else {
      hw_ready = false;
      #if HAS_DISPLAY
        if (disp_ready) {
          device_init_done = true;
          update_display();
        }
      #endif
    }
  } else {
    hw_ready = false;
    Serial.write("Error, incorrect boot vector\r\n");
    #if HAS_DISPLAY
      if (disp_ready) {
        device_init_done = true;
        update_display();
      }
    #endif
    led_indicate_boot_error();
  }
}

void loop() {
  if (radio_online) {
    checkModemStatus();

    #if MCU_VARIANT == MCU_ESP32
      if (packet_ready) {
        portENTER_CRITICAL(&update_lock);
        last_rssi = LoRa.packetRssi();
        last_snr_raw = LoRa.packetSnrRaw();
        portEXIT_CRITICAL(&update_lock);
        kiss_indicate_stat_rssi();
        kiss_indicate_stat_snr();
        kiss_write_packet();
      }
    #endif

    if (queue_height > 0) {
      if (!dcd_waiting) updateModemStatus();

      if (!dcd && !dcd_led) {
        if (dcd_waiting) delay(lora_rx_turnaround_ms);

        updateModemStatus();

        if (!dcd) {
          dcd_waiting = false;

          flushQueue();
          
        }
      } else {
        dcd_waiting = true;
      }
    }
  
  } else {
    if (hw_ready) {
      if (console_active) {
        console_loop();
      } else {
        led_indicate_standby();
      }
    } else {

      led_indicate_not_ready();
      stopRadio();
    }
  }

  #if MCU_VARIANT == MCU_ESP32
    // if (!console_active) {
      buffer_serial();
      if (!fifo_isempty(&serialFIFO)) serial_poll();
    // }
  #else
    if (!fifo_isempty_locked(&serialFIFO)) serial_poll();
  #endif

  #if HAS_DISPLAY
    if (disp_ready) update_display();
  #endif

  #if HAS_PMU
    if (pmu_ready) update_pmu();
  #endif

  #if HAS_BLUETOOTH
    if (!console_active && bt_ready) update_bt();
  #endif
}

volatile bool serial_polling = false;
void serial_poll() {
  serial_polling = true;

  #if MCU_VARIANT != MCU_ESP32
  while (!fifo_isempty_locked(&serialFIFO)) {
  #else
  while (!fifo_isempty(&serialFIFO)) {
  #endif
    char sbyte = fifo_pop(&serialFIFO);
    serialCallback(sbyte);
  }

  serial_polling = false;
}

#if MCU_VARIANT != MCU_ESP32
  #define MAX_CYCLES 20
#else
  #define MAX_CYCLES 10
#endif
void buffer_serial() {
  if (!serial_buffering) {
    serial_buffering = true;

    uint8_t c = 0;

    #if HAS_BLUETOOTH
    while (
      c < MAX_CYCLES &&
      ( (bt_state != BT_STATE_CONNECTED && Serial.available()) || (bt_state == BT_STATE_CONNECTED && SerialBT.available()) )
      )
    #else
    while (c < MAX_CYCLES && Serial.available())
    #endif
    {
      c++;

      #if MCU_VARIANT != MCU_ESP32
        if (!fifo_isfull_locked(&serialFIFO)) {
          fifo_push_locked(&serialFIFO, Serial.read());
        }
      #else
        if (HAS_BLUETOOTH && bt_state == BT_STATE_CONNECTED) {
          if (!fifo_isfull(&serialFIFO)) {
            fifo_push(&serialFIFO, SerialBT.read());
          }
        } else {
          if (!fifo_isfull(&serialFIFO)) {
            fifo_push(&serialFIFO, Serial.read());
          }
        }
      #endif
    }

    serial_buffering = false;
  }
}

void serial_interrupt_init() {
  #if MCU_VARIANT == MCU_1284P
      TCCR3A = 0;
      TCCR3B = _BV(CS10) |
               _BV(WGM33)|
               _BV(WGM32);

      // Buffer incoming frames every 1ms
      ICR3 = 16000;

      TIMSK3 = _BV(ICIE3);

  #elif MCU_VARIANT == MCU_2560
      // TODO: This should probably be updated for
      // atmega2560 support. Might be source of
      // reported issues from snh.
      TCCR3A = 0;
      TCCR3B = _BV(CS10) |
               _BV(WGM33)|
               _BV(WGM32);

      // Buffer incoming frames every 1ms
      ICR3 = 16000;

      TIMSK3 = _BV(ICIE3);

  #elif MCU_VARIANT == MCU_ESP32
      // No interrupt-based polling on ESP32
  #endif

}

#if MCU_VARIANT == MCU_1284P || MCU_VARIANT == MCU_2560
  ISR(TIMER3_CAPT_vect) {
    buffer_serial();
  }
#endif
