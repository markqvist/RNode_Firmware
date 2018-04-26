#include <SPI.h>
#include <LoRa.h>
#include "Config.h"
#include "Framing.h"
#include "Utilities.cpp"

void setup() {
  // Seed the PRNG
  randomSeed(analogRead(0));

  // Initialise serial communication
  Serial.begin(serial_baudrate);
  while (!Serial);

  // Configure input and output pins
  pinMode(pin_led_rx, OUTPUT);
  pinMode(pin_led_tx, OUTPUT);

  // Set up buffers
  memset(pbuf, 0, sizeof(pbuf));
  memset(sbuf, 0, sizeof(sbuf));
  memset(cbuf, 0, sizeof(cbuf));

  // Set chip select, reset and interrupt
  // pins for the LoRa module
  LoRa.setPins(pin_cs, pin_reset, pin_dio);
}

bool startRadio() {
  update_radio_lock();
  if (!radio_online) {
    if (!radio_locked) {
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
    Serial.write((uint8_t)(last_rssi-rssi_offset));

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

void transmit(size_t size) {
  if (radio_online) {
    led_tx_on();
    size_t  written = 0;
    uint8_t header  = random(256) & 0xF0;

    if (size > SINGLE_MTU - HEADER_L) {
      header = header | FLAG_SPLIT;
    }

    LoRa.beginPacket();
    LoRa.write(header); written++;

    for (size_t i; i < size; i++) {
      LoRa.write(sbuf[i]);
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
    kiss_indicate_error(ERROR_TXFAILED);
    led_indicate_error(5);
  }

  if (FLOW_CONTROL_ENABLED)
      kiss_indicate_ready();
}

void serialCallback(uint8_t sbyte) {
  if (IN_FRAME && sbyte == FEND && command == CMD_DATA) {
    IN_FRAME = false;
    outbound_ready = true;
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
            setFrequency();
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
            setBandwidth();
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
        setTXPower();
        kiss_indicate_txpower();
      }
    } else if (command == CMD_SF) {
      if (sbyte == 0xFF) {
        kiss_indicate_spreadingfactor();
      } else {
        int sf = sbyte;
        if (sf < 7) sf = 7;
        if (sf > 12) sf = 12;

        lora_sf = sf;
        setSpreadingFactor();
        kiss_indicate_spreadingfactor();
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
    }
  }
}

void updateModemStatus() {
  uint8_t status = LoRa.modemStatus();
  last_status_update = millis();
  if (status & SIG_DETECT == 0x01) { stat_signal_detected = true; } else { stat_signal_detected = false; }
  if (status & SIG_SYNCED == 0x01) { stat_signal_synced = true; } else { stat_signal_synced = false; }
  if (status & RX_ONGOING == 0x01) { stat_rx_ongoing = true; } else { stat_rx_ongoing = false; }

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

void loop() {
  if (radio_online) {
    checkModemStatus();
    if (outbound_ready) {
      if (!dcd_waiting) updateModemStatus();
      if (!dcd && !dcd_led) {
        if (dcd_waiting) delay(lora_rx_turnaround_ms);
        updateModemStatus();
        if (!dcd) {
          outbound_ready = false;
          dcd_waiting = false;
          transmit(frame_len);
        }
      } else {
        dcd_waiting = true;
      }
    }
  }

  if (Serial.available()) {
    char sbyte = Serial.read();
    serialCallback(sbyte);
  }
}
