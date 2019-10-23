#include <Arduino.h>
#include <SPI.h>
#include "Utilities.h"

void setup() {
  // Seed the PRNG
  randomSeed(analogRead(0));

  // Initialise serial communication
  Serial.begin(serial_baudrate);
  while (!Serial);

  // Configure input and output pins
  pinMode(pin_led_rx, OUTPUT);
  pinMode(pin_led_tx, OUTPUT);

  // Initialise buffers
  memset(pbuf, 0, sizeof(pbuf));
  memset(sbuf, 0, sizeof(sbuf));
  memset(cbuf, 0, sizeof(cbuf));
  
  #if QUEUE_SIZE > 0
    memset(qbuf, 0, sizeof(qbuf));
    memset(queued_lengths, 0, sizeof(queued_lengths));
  #endif

  // Set chip select, reset and interrupt
  // pins for the LoRa module
  LoRa.setPins(pin_cs, pin_reset, pin_dio);

  // Validate board health, EEPROM and config
  validateStatus();
}

bool startRadio() {
  update_radio_lock();
  if (!radio_online) {
    if (!radio_locked && hw_ready) {
      if (!LoRa.begin(lora_freq)) {
        // The radio could not be started.
        // Indicate this failure over both the
        // serial port and with the onboard LEDs
        kiss_indicate_error(ERROR_INITRADIO);
        led_indicate_error(0);
      } else {
        radio_online = true;

        setTXPower();
        setBandwidth();
        setSpreadingFactor();
        setCodingRate();
        getFrequency();

        LoRa.enableCrc();
        LoRa.onReceive(receiveCallback);

        LoRa.receive();

        // Flash an info pattern to indicate
        // that the radio is now on
        led_indicate_info(3);
      }

    } else {
      // Flash a warning pattern to indicate
      // that the radio was locked, and thus
      // not started
      led_indicate_warning(3);
    }
  } else {
    // If radio is already on, we silently
    // ignore the request.
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

void receiveCallback(int packet_size) {
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
      last_rssi = LoRa.packetRssi();
      getPacketData(packet_size);
    } else if (isSplitPacket(header) && seq == sequence) {
      // This is the second part of a split
      // packet, so we add it to the buffer
      // and set the ready flag.
      last_rssi = (last_rssi+LoRa.packetRssi())/2;
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
      last_rssi = LoRa.packetRssi();
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

      last_rssi = LoRa.packetRssi();
      getPacketData(packet_size);
      ready = true;
    }

    if (ready) {
      // We first signal the RSSI of the
      // recieved packet to the host.
      Serial.write(FEND);
      Serial.write(CMD_STAT_RSSI);
      Serial.write((uint8_t)(last_rssi+rssi_offset));
      Serial.write(FEND);

      // And then write the entire packet
      Serial.write(FEND);
      Serial.write(CMD_DATA);
      for (int i = 0; i < read_len; i++) {
        uint8_t byte = pbuf[i];
        if (byte == FEND) { Serial.write(FESC); byte = TFEND; }
        if (byte == FESC) { Serial.write(FESC); byte = TFESC; }
        Serial.write(byte);
      }
      Serial.write(FEND);
      read_len = 0;
    }  
  } else {
    // In promiscuous mode, raw packets are
    // output directly over to the host
    read_len = 0;
    last_rssi = LoRa.packetRssi();
    getPacketData(packet_size);

    // We first signal the RSSI of the
    // recieved packet to the host.
    Serial.write(FEND);
    Serial.write(CMD_STAT_RSSI);
    Serial.write((uint8_t)(last_rssi+rssi_offset));
    Serial.write(FEND);

    // And then write the entire packet
    Serial.write(FEND);
    Serial.write(CMD_DATA);
    for (int i = 0; i < read_len; i++) {
      uint8_t byte = pbuf[i];
      if (byte == FEND) { Serial.write(FESC); byte = TFEND; }
      if (byte == FESC) { Serial.write(FESC); byte = TFESC; }
      Serial.write(byte);
    }
    Serial.write(FEND);
    read_len = 0;    
  }
}


bool outboundReady() {
  #if QUEUE_SIZE > 0
    if (queue_head != queue_tail) {
      return true;
    } else {
      return false;
    }
  #else
    return outbound_ready;
  #endif
}

bool queueFull() {
  size_t new_queue_head = (queue_head+1)%QUEUE_BUF_SIZE;
  if (new_queue_head == queue_tail) {
    return true;
  } else {
    return false;
  }
}

void enqueuePacket(size_t length) {
  size_t new_queue_head = (queue_head+1)%QUEUE_BUF_SIZE;
  if (new_queue_head != queue_tail) {
    queued_lengths[queue_head] = length;
    size_t insert_addr = queue_head * MTU;
    for (int i = 0; i < length; i++) {
      qbuf[insert_addr+i] = sbuf[i];
    }
    queue_head = new_queue_head;
    if (!queueFull()) {
      kiss_indicate_ready();
    }
  } else {
    kiss_indicate_error(ERROR_QUEUE_FULL);
  }
}

#if QUEUE_SIZE > 0
void processQueue() {
  size_t fetch_address = queue_tail*MTU;
  size_t fetch_length  = queued_lengths[queue_tail];

  for (int i = 0; i < fetch_length; i++) {
    tbuf[i] = qbuf[fetch_address+i];
    qbuf[fetch_address+i] = 0x00;
  }

  queued_lengths[queue_tail] = 0;

  queue_tail = ++queue_tail%QUEUE_BUF_SIZE;

  transmit(fetch_length);

  if (!queueFull()) {
    kiss_indicate_ready();
  }
}
#endif

void transmit(size_t size) {
  if (radio_online) {
    if (!promisc) {
      led_tx_on();
      size_t  written = 0;
      uint8_t header  = random(256) & 0xF0;

      if (size > SINGLE_MTU - HEADER_L) {
        header = header | FLAG_SPLIT;
      }

      LoRa.beginPacket();
      LoRa.write(header); written++;

      for (size_t i; i < size; i++) {
        #if QUEUE_SIZE > 0
          LoRa.write(tbuf[i]);
        #else
          LoRa.write(sbuf[i]);
        #endif

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

      LoRa.receive();
    } else {
      // In promiscuous mode, we only send out
      // plain raw LoRa packets with a maximum
      // payload of 255 bytes
      led_tx_on();
      size_t  written = 0;
      
      // Cap packets at 255 bytes
      if (size > SINGLE_MTU) {
        size = SINGLE_MTU;
      }

      LoRa.beginPacket();
      for (size_t i; i < size; i++) {
        #if QUEUE_SIZE > 0
          LoRa.write(tbuf[i]);
        #else
          LoRa.write(sbuf[i]);
        #endif

        written++;
      }
      LoRa.endPacket();
      led_tx_off();

      LoRa.receive();
    }
  } else {
    kiss_indicate_error(ERROR_TXFAILED);
    led_indicate_error(5);
  }

  #if QUEUE_SIZE == 0
    if (FLOW_CONTROL_ENABLED)
        kiss_indicate_ready();
  #endif
}

void serialCallback(uint8_t sbyte) {
  if (IN_FRAME && sbyte == FEND && command == CMD_DATA) {
    IN_FRAME = false;

    if (QUEUE_SIZE == 0) {
      if (outbound_ready) {
        kiss_indicate_error(ERROR_QUEUE_FULL);
      } else {
        outbound_ready = true;
      }
    } else {
      enqueuePacket(frame_len);
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
        if (sbyte == FESC) {
            ESCAPE = true;
        } else {
            if (ESCAPE) {
                if (sbyte == TFEND) sbyte = FEND;
                if (sbyte == TFESC) sbyte = FESC;
                ESCAPE = false;
            }
            sbuf[frame_len++] = sbyte;
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
            cbuf[frame_len++] = sbyte;
        }

        if (frame_len == 4) {
          uint32_t freq = (uint32_t)cbuf[0] << 24 | (uint32_t)cbuf[1] << 16 | (uint32_t)cbuf[2] << 8 | (uint32_t)cbuf[3];

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
            cbuf[frame_len++] = sbyte;
        }

        if (frame_len == 4) {
          uint32_t bw = (uint32_t)cbuf[0] << 24 | (uint32_t)cbuf[1] << 16 | (uint32_t)cbuf[2] << 8 | (uint32_t)cbuf[3];

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
    } else if (command == CMD_RADIO_STATE) {
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
        kiss_indicate_detect();
      }
    } else if (command == CMD_PROMISC) {
      if (sbyte == 0x01) {
        promisc_enable();
      } else if (sbyte == 0x00) {
        promisc_disable();
      }
      kiss_indicate_promisc();
    } else if (command == CMD_UNLOCK_ROM) {
      if (sbyte == ROM_UNLOCK_BYTE) {
        unlock_rom();
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
            cbuf[frame_len++] = sbyte;
        }

        if (frame_len == 2) {
          eeprom_write(cbuf[0], cbuf[1]);
        }
    } else if (command == CMD_FW_VERSION) {
      kiss_indicate_version();
    } else if (command == CMD_CONF_SAVE) {
      eeprom_conf_save();
    } else if (command == CMD_CONF_DELETE) {
      eeprom_conf_delete();
    }
  }
}

void updateModemStatus() {
  uint8_t status = LoRa.modemStatus();
  last_status_update = millis();
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

void validateStatus() {
  if (eeprom_lock_set()) {
    if (eeprom_product_valid() && eeprom_model_valid() && eeprom_hwrev_valid()) {
      if (eeprom_checksum_valid()) {
        hw_ready = true;

        if (eeprom_have_conf()) {
          eeprom_conf_load();
          op_mode = MODE_TNC;
          startRadio();
        }
      }
    } else {
      hw_ready = false;
    }
  } else {
    hw_ready = false;
  }
}

void loop() {
  if (radio_online) {
    checkModemStatus();
    if (outboundReady() && !SERIAL_READING) {
      if (!dcd_waiting) updateModemStatus();
      if (!dcd && !dcd_led) {
        if (dcd_waiting) delay(lora_rx_turnaround_ms);
        updateModemStatus();
        if (!dcd) {
          dcd_waiting = false;
          #if QUEUE_SIZE > 0
            processQueue();
          #else
            outbound_ready = false;
            transmit(frame_len);
          #endif
        }
      } else {
        dcd_waiting = true;
      }
    }
  
  } else {
    if (hw_ready) {
      led_indicate_standby();
    } else {
      led_indicate_not_ready();
      stopRadio();
    }
  }

  if (Serial.available()) {
    SERIAL_READING = true;
    char sbyte = Serial.read();
    serialCallback(sbyte);
    last_serial_read = millis();
  } else {
    if (SERIAL_READING && millis()-last_serial_read >= serial_read_timeout_ms) {
      SERIAL_READING = false;
    }
  }
}
