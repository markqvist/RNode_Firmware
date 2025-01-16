// Copyright (C) 2024, Mark Qvist

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
#if HAS_BLUETOOTH || HAS_BLE == true
  bool bt_init_ran = false;
#endif

#if HAS_CONSOLE
  #include "Console.h"
#endif

#if PLATFORM == PLATFORM_ESP32 || PLATFORM == PLATFORM_NRF52
  #define MODEM_QUEUE_SIZE 4
  typedef struct {
          size_t len;
          int rssi;
          int snr_raw;
          uint8_t data[];
  } modem_packet_t;
  static xQueueHandle modem_packet_queue = NULL;
#endif

char sbuf[128];

#if MCU_VARIANT == MCU_ESP32 || MCU_VARIANT == MCU_NRF52
  bool packet_ready = false;
#endif

void setup() {
  #if MCU_VARIANT == MCU_ESP32
    boot_seq();
    EEPROM.begin(EEPROM_SIZE);
    Serial.setRxBufferSize(CONFIG_UART_BUFFER_SIZE);

    #if BOARD_MODEL == BOARD_TDECK
      pinMode(pin_poweron, OUTPUT);
      digitalWrite(pin_poweron, HIGH);

      pinMode(SD_CS, OUTPUT);
      pinMode(DISPLAY_CS, OUTPUT);
      digitalWrite(SD_CS, HIGH);
      digitalWrite(DISPLAY_CS, HIGH);

      pinMode(DISPLAY_BL_PIN, OUTPUT);
    #endif
  #endif

  #if MCU_VARIANT == MCU_NRF52
    #if BOARD_MODEL == BOARD_TECHO
      delay(200);
      pinMode(PIN_VEXT_EN, OUTPUT);
      digitalWrite(PIN_VEXT_EN, HIGH);
      pinMode(pin_btn_usr1, INPUT_PULLUP);
      pinMode(pin_btn_touch, INPUT_PULLUP);
      pinMode(PIN_LED_RED, OUTPUT);
      pinMode(PIN_LED_GREEN, OUTPUT);
      pinMode(PIN_LED_BLUE, OUTPUT);
      delay(200);
    #endif

    if (!eeprom_begin()) { Serial.write("EEPROM initialisation failed.\r\n"); }
  #endif

  // Seed the PRNG for CSMA R-value selection
  # if MCU_VARIANT == MCU_ESP32
    // On ESP32, get the seed value from the
    // hardware RNG
    int seed_val = (int)esp_random();
  #else
    // Otherwise, get a pseudo-random seed
    // value from an unconnected analog pin
    int seed_val = analogRead(0);
  #endif
  randomSeed(seed_val);

  // Initialise serial communication
  memset(serialBuffer, 0, sizeof(serialBuffer));
  fifo_init(&serialFIFO, serialBuffer, CONFIG_UART_BUFFER_SIZE);

  Serial.begin(serial_baudrate);

  #if HAS_NP
    led_init();
  #endif

  #if BOARD_MODEL != BOARD_RAK4631 && BOARD_MODEL != BOARD_HELTEC_T114 && BOARD_MODEL != BOARD_TECHO && BOARD_MODEL != BOARD_T3S3 && BOARD_MODEL != BOARD_TBEAM_S_V1
    // Some boards need to wait until the hardware UART is set up before booting
    // the full firmware. In the case of the RAK4631 and Heltec T114, the line below will wait
    // until a serial connection is actually established with a master. Thus, it
    // is disabled on this platform.
    while (!Serial);
  #endif

  serial_interrupt_init();

  // Configure input and output pins
  #if HAS_INPUT
    input_init();
  #endif

  #if HAS_NP == false
    pinMode(pin_led_rx, OUTPUT);
    pinMode(pin_led_tx, OUTPUT);
  #endif

  #if HAS_TCXO == true
    if (pin_tcxo_enable != -1) {
        pinMode(pin_tcxo_enable, OUTPUT);
        digitalWrite(pin_tcxo_enable, HIGH);
    }
  #endif

  // Initialise buffers
  memset(pbuf, 0, sizeof(pbuf));
  memset(cmdbuf, 0, sizeof(cmdbuf));
  
  memset(packet_queue, 0, sizeof(packet_queue));

  memset(packet_starts_buf, 0, sizeof(packet_starts_buf));
  fifo16_init(&packet_starts, packet_starts_buf, CONFIG_QUEUE_MAX_LENGTH);
  
  memset(packet_lengths_buf, 0, sizeof(packet_starts_buf));
  fifo16_init(&packet_lengths, packet_lengths_buf, CONFIG_QUEUE_MAX_LENGTH);

  #if PLATFORM == PLATFORM_ESP32 || PLATFORM == PLATFORM_NRF52
    modem_packet_queue = xQueueCreate(MODEM_QUEUE_SIZE, sizeof(modem_packet_t*));
  #endif

  // Set chip select, reset and interrupt
  // pins for the LoRa module
  #if MODEM == SX1276 || MODEM == SX1278
  LoRa->setPins(pin_cs, pin_reset, pin_dio, pin_busy);
  #elif MODEM == SX1262
  LoRa->setPins(pin_cs, pin_reset, pin_dio, pin_busy, pin_rxen);
  #elif MODEM == SX1280
  LoRa->setPins(pin_cs, pin_reset, pin_dio, pin_busy, pin_rxen, pin_txen);
  #endif
  
  #if MCU_VARIANT == MCU_ESP32 || MCU_VARIANT == MCU_NRF52
    init_channel_stats();

    #if BOARD_MODEL == BOARD_T3S3
      #if MODEM == SX1280
        delay(300);
        LoRa->reset();
        delay(100);
      #endif
    #endif

    // Check installed transceiver chip and
    // probe boot parameters.
    if (LoRa->preInit()) {
      modem_installed = true;
      
      #if HAS_INPUT
        // Skip quick-reset console activation
      #else
        uint32_t lfr = LoRa->getFrequency();
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
        LoRa->setFrequency(M_FRQ_S);
      #endif

    } else {
      modem_installed = false;
    }
  #else
    // Older variants only came with SX1276/78 chips,
    // so assume that to be the case for now.
    modem_installed = true;
  #endif

  #if HAS_DISPLAY
    #if HAS_EEPROM
    if (EEPROM.read(eeprom_addr(ADDR_CONF_DSET)) != CONF_OK_BYTE) {
    #elif MCU_VARIANT == MCU_NRF52
    if (eeprom_read(eeprom_addr(ADDR_CONF_DSET)) != CONF_OK_BYTE) {
    #endif
      eeprom_update(eeprom_addr(ADDR_CONF_DSET), CONF_OK_BYTE);
      #if BOARD_MODEL == BOARD_TECHO
        eeprom_update(eeprom_addr(ADDR_CONF_DINT), 0x03);
      #else
        eeprom_update(eeprom_addr(ADDR_CONF_DINT), 0xFF);
      #endif
    }
    #if BOARD_MODEL == BOARD_TECHO
      display_add_callback(work_while_waiting);
    #endif

    display_unblank();
    disp_ready = display_init();
    update_display();
  #endif

  #if MCU_VARIANT == MCU_ESP32 || MCU_VARIANT == MCU_NRF52
    #if HAS_PMU == true
      pmu_ready = init_pmu();
    #endif

    #if HAS_BLUETOOTH || HAS_BLE == true
      bt_init();
      bt_init_ran = true;
    #endif

    if (console_active) {
      #if HAS_CONSOLE
        console_start();
      #else
        kiss_indicate_reset();
      #endif
    } else {
      kiss_indicate_reset();
    }
  #endif

  #if MCU_VARIANT == MCU_ESP32 || MCU_VARIANT == MCU_NRF52
    #if MODEM == SX1280
      avoid_interference = false;
    #else
      #if HAS_EEPROM
        uint8_t ia_conf = EEPROM.read(eeprom_addr(ADDR_CONF_DIA));
        if (ia_conf == 0x00) { avoid_interference = true; }
        else                 { avoid_interference = false; }
      #elif MCU_VARIANT == MCU_NRF52
        uint8_t ia_conf = eeprom_read(eeprom_addr(ADDR_CONF_DIA));
        if (ia_conf == 0x00) { avoid_interference = true; }
        else                 { avoid_interference = false; }
      #endif
    #endif
  #endif

  // Validate board health, EEPROM and config
  validate_status();

  if (op_mode != MODE_TNC) LoRa->setFrequency(0);
}

void lora_receive() {
  if (!implicit) {
    LoRa->receive();
  } else {
    LoRa->receive(implicit_l);
  }
}

inline void kiss_write_packet() {
  serial_write(FEND);
  serial_write(CMD_DATA);
  
  for (uint16_t i = 0; i < read_len; i++) {
    #if MCU_VARIANT == MCU_NRF52
      portENTER_CRITICAL();
      uint8_t byte = pbuf[i];
      portEXIT_CRITICAL();
    #else
      uint8_t byte = pbuf[i];
    #endif

    if (byte == FEND) { serial_write(FESC); byte = TFEND; }
    if (byte == FESC) { serial_write(FESC); byte = TFESC; }
    serial_write(byte);
  }

  serial_write(FEND);
  read_len = 0;

  #if MCU_VARIANT == MCU_ESP32 || MCU_VARIANT == MCU_NRF52
    packet_ready = false;
  #endif

  #if MCU_VARIANT == MCU_ESP32
    #if HAS_BLE
      bt_flush();
    #endif
  #endif
}

inline void getPacketData(uint16_t len) {
  #if MCU_VARIANT != MCU_NRF52
    while (len-- && read_len < MTU) {
      pbuf[read_len++] = LoRa->read();
    }  
  #else
    BaseType_t int_mask = taskENTER_CRITICAL_FROM_ISR();
    while (len-- && read_len < MTU) {
      pbuf[read_len++] = LoRa->read();
    }
    taskEXIT_CRITICAL_FROM_ISR(int_mask);
  #endif
}

void ISR_VECT receive_callback(int packet_size) {
  #if MCU_VARIANT == MCU_ESP32 || MCU_VARIANT == MCU_NRF52
    BaseType_t int_mask;
  #endif

  if (!promisc) {
    // The standard operating mode allows large
    // packets with a payload up to 500 bytes,
    // by combining two raw LoRa packets.
    // We read the 1-byte header and extract
    // packet sequence number and split flags
    uint8_t header   = LoRa->read(); packet_size--;
    uint8_t sequence = packetSequence(header);
    bool    ready    = false;

    if (isSplitPacket(header) && seq == SEQ_UNSET) {
      // This is the first part of a split
      // packet, so we set the seq variable
      // and add the data to the buffer
      #if MCU_VARIANT == MCU_NRF52
        int_mask = taskENTER_CRITICAL_FROM_ISR(); read_len = 0; taskEXIT_CRITICAL_FROM_ISR(int_mask);
      #else
        read_len = 0;
      #endif
      
      seq = sequence;

      #if MCU_VARIANT != MCU_ESP32 && MCU_VARIANT != MCU_NRF52
        last_rssi = LoRa->packetRssi();
        last_snr_raw = LoRa->packetSnrRaw();
      #endif

      getPacketData(packet_size);

    } else if (isSplitPacket(header) && seq == sequence) {
      // This is the second part of a split
      // packet, so we add it to the buffer
      // and set the ready flag.
      #if MCU_VARIANT != MCU_ESP32 && MCU_VARIANT != MCU_NRF52
        last_rssi = (last_rssi+LoRa->packetRssi())/2;
        last_snr_raw = (last_snr_raw+LoRa->packetSnrRaw())/2;
      #endif

      getPacketData(packet_size);
      seq = SEQ_UNSET;
      ready = true;

    } else if (isSplitPacket(header) && seq != sequence) {
      // This split packet does not carry the
      // same sequence id, so we must assume
      // that we are seeing the first part of
      // a new split packet.
      #if MCU_VARIANT == MCU_NRF52
        int_mask = taskENTER_CRITICAL_FROM_ISR(); read_len = 0; taskEXIT_CRITICAL_FROM_ISR(int_mask);
      #else
        read_len = 0;
      #endif
      seq = sequence;

      #if MCU_VARIANT != MCU_ESP32 && MCU_VARIANT != MCU_NRF52
        last_rssi = LoRa->packetRssi();
        last_snr_raw = LoRa->packetSnrRaw();
      #endif

      getPacketData(packet_size);

    } else if (!isSplitPacket(header)) {
      // This is not a split packet, so we
      // just read it and set the ready
      // flag to true.

      if (seq != SEQ_UNSET) {
        // If we already had part of a split
        // packet in the buffer, we clear it.
        #if MCU_VARIANT == MCU_NRF52
          int_mask = taskENTER_CRITICAL_FROM_ISR(); read_len = 0; taskEXIT_CRITICAL_FROM_ISR(int_mask);
        #else
          read_len = 0;
        #endif
        seq = SEQ_UNSET;
      }

      #if MCU_VARIANT != MCU_ESP32 && MCU_VARIANT != MCU_NRF52
        last_rssi = LoRa->packetRssi();
        last_snr_raw = LoRa->packetSnrRaw();
      #endif

      getPacketData(packet_size);
      ready = true;
    }

    if (ready) {
      #if MCU_VARIANT != MCU_ESP32 && MCU_VARIANT != MCU_NRF52
        // We first signal the RSSI of the
        // recieved packet to the host.
        kiss_indicate_stat_rssi();
        kiss_indicate_stat_snr();

        // And then write the entire packet
        kiss_write_packet();
      
      #else
        // Allocate packet struct, but abort if there
        // is not enough memory available.
        modem_packet_t *modem_packet = (modem_packet_t*)malloc(sizeof(modem_packet_t) + read_len);
        if(!modem_packet) { memory_low = true; return; }

        // Get packet RSSI and SNR
        #if MCU_VARIANT == MCU_ESP32
          modem_packet->snr_raw = LoRa->packetSnrRaw();
          modem_packet->rssi = LoRa->packetRssi(modem_packet->snr_raw);
        #endif

        // Send packet to event queue, but free the
        // allocated memory again if the queue is
        // unable to receive the packet.
        modem_packet->len = read_len;
        memcpy(modem_packet->data, pbuf, read_len);
        if (!modem_packet_queue || xQueueSendFromISR(modem_packet_queue, &modem_packet, NULL) != pdPASS) {
            free(modem_packet);
        }
      #endif
    }  
  } else {
    // In promiscuous mode, raw packets are
    // output directly to the host
    read_len = 0;

    #if MCU_VARIANT != MCU_ESP32 && MCU_VARIANT != MCU_NRF52
      last_rssi = LoRa->packetRssi();
      last_snr_raw = LoRa->packetSnrRaw();
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
  if (!radio_online && !console_active) {
    if (!radio_locked && hw_ready) {
      if (!LoRa->begin(lora_freq)) {
        // The radio could not be started.
        // Indicate this failure over both the
        // serial port and with the onboard LEDs
        radio_error = true;
        kiss_indicate_error(ERROR_INITRADIO);
        led_indicate_error(0);
        return false;
      } else {
        radio_online = true;

        init_channel_stats();

        setTXPower();
        setBandwidth();
        setSpreadingFactor();
        setCodingRate();
        getFrequency();

        LoRa->enableCrc();
        LoRa->onReceive(receive_callback);
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
  LoRa->end();
  radio_online = false;
}

void update_radio_lock() {
  if (lora_freq != 0 && lora_bw != 0 && lora_txp != 0xFF && lora_sf != 0) {
    radio_locked = false;
  } else {
    radio_locked = true;
  }
}

bool queue_full() { return (queue_height >= CONFIG_QUEUE_MAX_LENGTH || queued_bytes >= CONFIG_QUEUE_SIZE); }

volatile bool queue_flushing = false;
void flush_queue(void) {
  if (!queue_flushing) {
    queue_flushing = true;
    led_tx_on(); uint16_t processed = 0;

    #if MCU_VARIANT == MCU_ESP32 || MCU_VARIANT == MCU_NRF52
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

        transmit(length); processed++;
      }
    }

    lora_receive(); led_tx_off();
  }

  queue_height = 0;
  queued_bytes = 0;

  #if MCU_VARIANT == MCU_ESP32 || MCU_VARIANT == MCU_NRF52
    update_airtime();
  #endif

  queue_flushing = false;

  #if HAS_DISPLAY
    display_tx = true;
  #endif
}

void pop_queue() {
  if (!queue_flushing) {
    queue_flushing = true;
    led_tx_on(); uint16_t processed = 0;

    #if MCU_VARIANT == MCU_ESP32 || MCU_VARIANT == MCU_NRF52
    if (!fifo16_isempty(&packet_starts)) {
    #else
    if (!fifo16_isempty_locked(&packet_starts)) {
    #endif

      uint16_t start = fifo16_pop(&packet_starts);
      uint16_t length = fifo16_pop(&packet_lengths);
      if (length >= MIN_L && length <= MTU) {
        for (uint16_t i = 0; i < length; i++) {
          uint16_t pos = (start+i)%CONFIG_QUEUE_SIZE;
          tbuf[i] = packet_queue[pos];
        }

        transmit(length); processed++;
      }
      queue_height -= processed;
      queued_bytes -= length;
    }

    lora_receive(); led_tx_off();
  }

  #if MCU_VARIANT == MCU_ESP32 || MCU_VARIANT == MCU_NRF52
    update_airtime();
  #endif

  queue_flushing = false;

  #if HAS_DISPLAY
    display_tx = true;
  #endif
}

void add_airtime(uint16_t written) {
  #if MCU_VARIANT == MCU_ESP32 || MCU_VARIANT == MCU_NRF52
    float lora_symbols = 0;
    float packet_cost_ms = 0.0;
    int ldr_opt = 0; if (lora_low_datarate) ldr_opt = 1;

    #if MODEM == SX1276 || MODEM == SX1278
      lora_symbols += (8*written + PHY_CRC_LORA_BITS - 4*lora_sf + 8 + PHY_HEADER_LORA_SYMBOLS);
      lora_symbols /=                          4*(lora_sf-2*ldr_opt);
      lora_symbols *= lora_cr;
      lora_symbols += lora_preamble_symbols + 0.25 + 8;
      packet_cost_ms += lora_symbols * lora_symbol_time_ms;
      
    #elif MODEM == SX1262 || MODEM == SX1280
      if (lora_sf < 7) {
        lora_symbols += (8*written + PHY_CRC_LORA_BITS - 4*lora_sf + PHY_HEADER_LORA_SYMBOLS);
        lora_symbols /=                              4*lora_sf;
        lora_symbols *= lora_cr;
        lora_symbols += lora_preamble_symbols + 2.25 + 8;
        packet_cost_ms += lora_symbols * lora_symbol_time_ms;

      } else {
        lora_symbols += (8*written + PHY_CRC_LORA_BITS - 4*lora_sf + 8 + PHY_HEADER_LORA_SYMBOLS);
        lora_symbols /=                         4*(lora_sf-2*ldr_opt);
        lora_symbols *= lora_cr;
        lora_symbols += lora_preamble_symbols + 0.25 + 8;
        packet_cost_ms += lora_symbols * lora_symbol_time_ms;
      }
    
    #endif

    uint16_t cb = current_airtime_bin();
    uint16_t nb = cb+1; if (nb == AIRTIME_BINS) { nb = 0; }
    airtime_bins[cb] += packet_cost_ms;
    airtime_bins[nb] = 0;

  #endif
}

void update_airtime() {
  #if MCU_VARIANT == MCU_ESP32 || MCU_VARIANT == MCU_NRF52
    uint16_t cb = current_airtime_bin();
    uint16_t pb = cb-1; if (cb-1 < 0) { pb = AIRTIME_BINS-1; }
    uint16_t nb = cb+1; if (nb == AIRTIME_BINS) { nb = 0; }
    airtime_bins[nb] = 0; airtime = (float)(airtime_bins[cb]+airtime_bins[pb])/(2.0*AIRTIME_BINLEN_MS);

    uint32_t longterm_airtime_sum = 0;
    for (uint16_t bin = 0; bin < AIRTIME_BINS; bin++) { longterm_airtime_sum += airtime_bins[bin]; }
    longterm_airtime = (float)longterm_airtime_sum/(float)AIRTIME_LONGTERM_MS;

    float longterm_channel_util_sum = 0.0;
    for (uint16_t bin = 0; bin < AIRTIME_BINS; bin++) { longterm_channel_util_sum += longterm_bins[bin]; }
    longterm_channel_util = (float)longterm_channel_util_sum/(float)AIRTIME_BINS;

    #if MCU_VARIANT == MCU_ESP32 || MCU_VARIANT == MCU_NRF52
      update_csma_parameters();
    #endif

    kiss_indicate_channel_stats();
  #endif
}

void transmit(uint16_t size) {
  if (radio_online) {
    if (!promisc) {
      uint16_t  written = 0;
      uint8_t header  = random(256) & 0xF0;
      if (size > SINGLE_MTU - HEADER_L) { header = header | FLAG_SPLIT; }

      LoRa->beginPacket();
      LoRa->write(header); written++;

      for (uint16_t i=0; i < size; i++) {
        LoRa->write(tbuf[i]); written++;

        if (written == 255 && isSplitPacket(header)) {
          if (!LoRa->endPacket()) {
            kiss_indicate_error(ERROR_MODEM_TIMEOUT);
            kiss_indicate_error(ERROR_TXFAILED);
            led_indicate_error(5);
            hard_reset();
          }

          add_airtime(written);
          LoRa->beginPacket();
          LoRa->write(header);
          written = 1;
        }
      }

      if (!LoRa->endPacket()) {
        kiss_indicate_error(ERROR_MODEM_TIMEOUT);
        kiss_indicate_error(ERROR_TXFAILED);
        led_indicate_error(5);
        hard_reset();
      }

      add_airtime(written);

    } else {
      led_tx_on(); uint16_t written = 0;
      if (size > SINGLE_MTU) { size = SINGLE_MTU; }
      if (!implicit) { LoRa->beginPacket(); }
      else           { LoRa->beginPacket(size); }
      for (uint16_t i=0; i < size; i++) { LoRa->write(tbuf[i]); written++; }
      LoRa->endPacket(); add_airtime(written);
    }

  } else { kiss_indicate_error(ERROR_TXFAILED); led_indicate_error(5); }
}

void serial_callback(uint8_t sbyte) {
  if (IN_FRAME && sbyte == FEND && command == CMD_DATA) {
    IN_FRAME = false;

    if (!fifo16_isfull(&packet_starts) && queued_bytes < CONFIG_QUEUE_SIZE) {
        uint16_t s = current_packet_start;
        int16_t e = queue_cursor-1; if (e == -1) e = CONFIG_QUEUE_SIZE-1;
        uint16_t l;

        if (s != e) { l = (s < e) ? e - s + 1 : CONFIG_QUEUE_SIZE - s + e + 1; }
        else        { l = 1; }

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
        if (bt_state != BT_STATE_CONNECTED) {
          cable_state = CABLE_STATE_CONNECTED;
        }
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
        #if MODEM == SX1262
          if (txp > 22) txp = 22;
        #elif MODEM == SX1280
          #if HAS_PA
            if (txp > 20) txp = 20;
          #else
            if (txp > 13) txp = 13;
          #endif
        #else
          if (txp > 17) txp = 17;
        #endif

        lora_txp = txp;
        if (op_mode == MODE_HOST) setTXPower();
        kiss_indicate_txpower();
      }
    } else if (command == CMD_SF) {
      if (sbyte == 0xFF) {
        kiss_indicate_spreadingfactor();
      } else {
        int sf = sbyte;
        if (sf < 5) sf = 5;
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
        display_unblank();
        cable_state   = CABLE_STATE_DISCONNECTED;
        current_rssi  = -292;
        last_rssi     = -292;
        last_rssi_raw = 0x00;
        last_snr_raw  = 0x80;
      }
    } else if (command == CMD_RADIO_STATE) {
      if (bt_state != BT_STATE_CONNECTED) {
        cable_state = CABLE_STATE_CONNECTED;
        display_unblank();
      }
      if (sbyte == 0xFF) {
        kiss_indicate_radiostate();
      } else if (sbyte == 0x00) {
        stopRadio();
        kiss_indicate_radiostate();
      } else if (sbyte == 0x01) {
        startRadio();
        kiss_indicate_radiostate();
      }
    } else if (command == CMD_ST_ALOCK) {
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
          uint16_t at = (uint16_t)cmdbuf[0] << 8 | (uint16_t)cmdbuf[1];

          if (at == 0) {
            st_airtime_limit = 0.0;
          } else {
            st_airtime_limit = (float)at/(100.0*100.0);
            if (st_airtime_limit >= 1.0) { st_airtime_limit = 0.0; }
          }
          kiss_indicate_st_alock();
        }
    } else if (command == CMD_LT_ALOCK) {
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
          uint16_t at = (uint16_t)cmdbuf[0] << 8 | (uint16_t)cmdbuf[1];

          if (at == 0) {
            lt_airtime_limit = 0.0;
          } else {
            lt_airtime_limit = (float)at/(100.0*100.0);
            if (lt_airtime_limit >= 1.0) { lt_airtime_limit = 0.0; }
          }
          kiss_indicate_lt_alock();
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
      if (!queue_full()) {
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
      if (sbyte != 0x00) { kiss_indicate_fb(); }
    } else if (command == CMD_DISP_READ) {
      if (sbyte != 0x00) { kiss_indicate_disp(); }
    } else if (command == CMD_DEV_HASH) {
      #if MCU_VARIANT == MCU_ESP32 || MCU_VARIANT == MCU_NRF52
        if (sbyte != 0x00) {
          kiss_indicate_device_hash();
        }
      #endif
    } else if (command == CMD_DEV_SIG) {
      #if MCU_VARIANT == MCU_ESP32 || MCU_VARIANT == MCU_NRF52
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
      #if MCU_VARIANT == MCU_ESP32 || MCU_VARIANT == MCU_NRF52
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
      #if MCU_VARIANT == MCU_ESP32 || MCU_VARIANT == MCU_NRF52
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
            memcpy(dev_firmware_hash_target, cmdbuf, DEV_HASH_LEN);
            device_save_firmware_hash();
          }
      #endif
    } else if (command == CMD_BT_CTRL) {
      #if HAS_BLUETOOTH || HAS_BLE
        if (sbyte == 0x00) {
          bt_stop();
          bt_conf_save(false);
        } else if (sbyte == 0x01) {
          bt_start();
          bt_conf_save(true);
        } else if (sbyte == 0x02) {
          if (bt_state == BT_STATE_OFF) {
            bt_start();
            bt_conf_save(true);
          }
          if (bt_state != BT_STATE_CONNECTED) {
            bt_enable_pairing();
          }
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
            di_conf_save(display_intensity);
            display_unblank();
        }
      #endif
    } else if (command == CMD_DISP_ADDR) {
      #if HAS_DISPLAY
        if (sbyte == FESC) {
            ESCAPE = true;
        } else {
            if (ESCAPE) {
                if (sbyte == TFEND) sbyte = FEND;
                if (sbyte == TFESC) sbyte = FESC;
                ESCAPE = false;
            }
            display_addr = sbyte;
            da_conf_save(display_addr);
        }

      #endif
    } else if (command == CMD_DISP_BLNK) {
      #if HAS_DISPLAY
        if (sbyte == FESC) {
            ESCAPE = true;
        } else {
            if (ESCAPE) {
                if (sbyte == TFEND) sbyte = FEND;
                if (sbyte == TFESC) sbyte = FESC;
                ESCAPE = false;
            }
            db_conf_save(sbyte);
            display_unblank();
        }
      #endif
    } else if (command == CMD_DISP_ROT) {
      #if HAS_DISPLAY
        if (sbyte == FESC) {
            ESCAPE = true;
        } else {
            if (ESCAPE) {
                if (sbyte == TFEND) sbyte = FEND;
                if (sbyte == TFESC) sbyte = FESC;
                ESCAPE = false;
            }
            drot_conf_save(sbyte);
            display_unblank();
        }
      #endif
    } else if (command == CMD_DIS_IA) {
      if (sbyte == FESC) {
          ESCAPE = true;
      } else {
          if (ESCAPE) {
              if (sbyte == TFEND) sbyte = FEND;
              if (sbyte == TFESC) sbyte = FESC;
              ESCAPE = false;
          }
          dia_conf_save(sbyte);
      }
    } else if (command == CMD_DISP_RCND) {
      #if HAS_DISPLAY
        if (sbyte == FESC) {
            ESCAPE = true;
        } else {
            if (ESCAPE) {
                if (sbyte == TFEND) sbyte = FEND;
                if (sbyte == TFESC) sbyte = FESC;
                ESCAPE = false;
            }
            if (sbyte > 0x00) recondition_display = true;
        }
      #endif
    } else if (command == CMD_NP_INT) {
      #if HAS_NP
        if (sbyte == FESC) {
            ESCAPE = true;
        } else {
            if (ESCAPE) {
                if (sbyte == TFEND) sbyte = FEND;
                if (sbyte == TFESC) sbyte = FESC;
                ESCAPE = false;
            }
            sbyte;
            led_set_intensity(sbyte);
            np_int_conf_save(sbyte);
        }

      #endif
    }
  }
}

#if MCU_VARIANT == MCU_ESP32
  portMUX_TYPE update_lock = portMUX_INITIALIZER_UNLOCKED;
#endif

bool medium_free() {
  update_modem_status();
  if (avoid_interference && interference_detected) { return false; }
  return !dcd;
}

bool noise_floor_sampled = false;
int  noise_floor_sample  = 0;
int  noise_floor_buffer[NOISE_FLOOR_SAMPLES] = {0};
void update_noise_floor() {
  #if MCU_VARIANT == MCU_ESP32 || MCU_VARIANT == MCU_NRF52
    if (!dcd) {
      if (!noise_floor_sampled || current_rssi < noise_floor + CSMA_INFR_THRESHOLD_DB) {
        noise_floor_buffer[noise_floor_sample] = current_rssi;
        noise_floor_sample = noise_floor_sample+1;
        if (noise_floor_sample >= NOISE_FLOOR_SAMPLES) {
          noise_floor_sample %= NOISE_FLOOR_SAMPLES;
          noise_floor_sampled = true;
        }

        if (noise_floor_sampled) {
          noise_floor = 0;
          for (int ni = 0; ni < NOISE_FLOOR_SAMPLES; ni++) { noise_floor += noise_floor_buffer[ni]; }
          noise_floor /= NOISE_FLOOR_SAMPLES;
        }
      }
    }
  #endif
}

#define LED_ID_TRIG 16
uint8_t led_id_filter = 0;
void update_modem_status() {
  #if MCU_VARIANT == MCU_ESP32
    portENTER_CRITICAL(&update_lock);
  #elif MCU_VARIANT == MCU_NRF52
    portENTER_CRITICAL();
  #endif

  bool carrier_detected = LoRa->dcd();
  current_rssi = LoRa->currentRssi();
  last_status_update = millis();

  #if MCU_VARIANT == MCU_ESP32
    portEXIT_CRITICAL(&update_lock);
  #elif MCU_VARIANT == MCU_NRF52
    portEXIT_CRITICAL();
  #endif

  interference_detected = !carrier_detected && (current_rssi > (noise_floor+CSMA_INFR_THRESHOLD_DB));
  if (interference_detected) { if (led_id_filter < LED_ID_TRIG) { led_id_filter += 1; } }
  else                       { if (led_id_filter > 0) {led_id_filter -= 1; } }

  if (carrier_detected) { dcd = true; } else { dcd = false; }

  dcd_led = dcd;
  if (dcd_led) { led_rx_on(); }
  else {
    if (interference_detected) {
      if (led_id_filter >= LED_ID_TRIG && noise_floor_sampled) { led_id_on(); }
    } else {
      if (airtime_lock) { led_indicate_airtime_lock(); }
      else              { led_rx_off(); led_id_off(); }
    }
  }
}

void check_modem_status() {
  if (millis()-last_status_update >= status_interval_ms) {
    update_modem_status();
    update_noise_floor();

    #if MCU_VARIANT == MCU_ESP32 || MCU_VARIANT == MCU_NRF52
      util_samples[dcd_sample] = dcd;
      dcd_sample = (dcd_sample+1)%DCD_SAMPLES;
      if (dcd_sample % UTIL_UPDATE_INTERVAL == 0) {
        int util_count = 0;
        for (int ui = 0; ui < DCD_SAMPLES; ui++) {
          if (util_samples[ui]) util_count++;
        }
        local_channel_util = (float)util_count / (float)DCD_SAMPLES;
        total_channel_util = local_channel_util + airtime;
        if (total_channel_util > 1.0) total_channel_util = 1.0;

        int16_t cb = current_airtime_bin();
        uint16_t nb = cb+1; if (nb == AIRTIME_BINS) { nb = 0; }
        if (total_channel_util > longterm_bins[cb]) longterm_bins[cb] = total_channel_util;
        longterm_bins[nb] = 0.0;

        update_airtime();
      }
    #endif
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
  #elif MCU_VARIANT == MCU_NRF52
      // TODO: Get NRF52 boot flags
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
          if (modem_installed) {
            #if PLATFORM == PLATFORM_ESP32 || PLATFORM == PLATFORM_NRF52
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
            Serial.write("No radio module found\r\n");
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
          Serial.write("Invalid EEPROM checksum\r\n");
          #if HAS_DISPLAY
            if (disp_ready) {
              device_init_done = true;
              update_display();
            }
          #endif
        }
      } else {
        hw_ready = false;
        Serial.write("Invalid EEPROM configuration\r\n");
        #if HAS_DISPLAY
          if (disp_ready) {
            device_init_done = true;
            update_display();
          }
        #endif
      }
    } else {
      hw_ready = false;
      Serial.write("Device unprovisioned, no device configuration found in EEPROM\r\n");
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

#if MCU_VARIANT == MCU_ESP32 || MCU_VARIANT == MCU_NRF52
  void update_csma_parameters() {
    int airtime_pct = (int)(airtime*100);
    int new_cw_band = cw_band;

    if (airtime_pct <= CSMA_BAND_1_MAX_AIRTIME) { new_cw_band = 1; }
    else {
      int at = airtime_pct + CSMA_BAND_1_MAX_AIRTIME;
      new_cw_band = map(at, CSMA_BAND_1_MAX_AIRTIME, CSMA_BAND_N_MIN_AIRTIME, 2, CSMA_CW_BANDS);
    }

    if (new_cw_band > CSMA_CW_BANDS) { new_cw_band = CSMA_CW_BANDS; }
    if (new_cw_band != cw_band) { 
      cw_band = (uint8_t)(new_cw_band);
      cw_min  = (cw_band-1) * CSMA_CW_PER_BAND_WINDOWS;
      cw_max  = (cw_band) * CSMA_CW_PER_BAND_WINDOWS - 1;
      kiss_indicate_csma_stats();
    }
  }
#endif

void tx_queue_handler() {
  if (!airtime_lock && queue_height > 0) {
    if (csma_cw == -1) {
      csma_cw = random(cw_min, cw_max);
      cw_wait_target = csma_cw * csma_slot_ms;
    }

    if (difs_wait_start == -1) {                                                  // DIFS wait not yet started
      if (medium_free()) { difs_wait_start = millis(); return; }                  // Set DIFS wait start time
      else               { return; } }                                            // Medium not yet free, continue waiting
    
    else {                                                                        // We are waiting for DIFS or CW to pass
      if (!medium_free()) { difs_wait_start = -1; cw_wait_start = -1; return; }   // Medium became occupied while in DIFS wait, restart waiting when free again
      else {                                                                      // Medium is free, so continue waiting
        if (millis() < difs_wait_start+difs_ms) { return; }                       // DIFS has not yet passed, continue waiting
        else {                                                                    // DIFS has passed, and we are now in CW wait
          if (cw_wait_start == -1) { cw_wait_start = millis(); return; }          // If we haven't started counting CW wait time, do it from now
          else {                                                                  // If we are already counting CW wait time, add it to the counter
            cw_wait_passed += millis()-cw_wait_start; cw_wait_start   = millis();
            if (cw_wait_passed < cw_wait_target) { return; }                      // Contention window wait time has not yet passed, continue waiting
            else {                                                                // Wait time has passed, flush the queue
              if (!lora_limit_rate) { flush_queue(); } else { pop_queue(); }
              cw_wait_passed = 0; csma_cw = -1; difs_wait_start = -1; }
          }
        }
      }
    }
  }
}

void work_while_waiting() { loop(); }

void loop() {
  if (radio_online) {
    #if MCU_VARIANT == MCU_ESP32
      modem_packet_t *modem_packet = NULL;
      if(modem_packet_queue && xQueueReceive(modem_packet_queue, &modem_packet, 0) == pdTRUE && modem_packet) {
        read_len = modem_packet->len;
        last_rssi = modem_packet->rssi;
        last_snr_raw = modem_packet->snr_raw;
        memcpy(&pbuf, modem_packet->data, modem_packet->len);
        free(modem_packet);
        modem_packet = NULL;

        kiss_indicate_stat_rssi();
        kiss_indicate_stat_snr();
        kiss_write_packet();
      }

      airtime_lock = false;
      if (st_airtime_limit != 0.0 && airtime >= st_airtime_limit) airtime_lock = true;
      if (lt_airtime_limit != 0.0 && longterm_airtime >= lt_airtime_limit) airtime_lock = true;

    #elif MCU_VARIANT == MCU_NRF52
      modem_packet_t *modem_packet = NULL;
      if(modem_packet_queue && xQueueReceive(modem_packet_queue, &modem_packet, 0) == pdTRUE && modem_packet) {
        memcpy(&pbuf, modem_packet->data, modem_packet->len);
        read_len = modem_packet->len;
        free(modem_packet);
        modem_packet = NULL;

        portENTER_CRITICAL();
        last_rssi = LoRa->packetRssi();
        last_snr_raw = LoRa->packetSnrRaw();
        portEXIT_CRITICAL();
        kiss_indicate_stat_rssi();
        kiss_indicate_stat_snr();
        kiss_write_packet();
      }

      airtime_lock = false;
      if (st_airtime_limit != 0.0 && airtime >= st_airtime_limit) airtime_lock = true;
      if (lt_airtime_limit != 0.0 && longterm_airtime >= lt_airtime_limit) airtime_lock = true;

    #endif

    tx_queue_handler();
    check_modem_status();
  
  } else {
    if (hw_ready) {
      if (console_active) {
        #if HAS_CONSOLE
          console_loop();
        #endif
      } else {
        led_indicate_standby();
      }
    } else {

      led_indicate_not_ready();
      stopRadio();
    }
  }

  #if MCU_VARIANT == MCU_ESP32 || MCU_VARIANT == MCU_NRF52
      buffer_serial();
      if (!fifo_isempty(&serialFIFO)) serial_poll();
  #else
    if (!fifo_isempty_locked(&serialFIFO)) serial_poll();
  #endif

  #if HAS_DISPLAY
    if (disp_ready && !display_updating) update_display();
  #endif

  #if HAS_PMU
    if (pmu_ready) update_pmu();
  #endif

  #if HAS_BLUETOOTH || HAS_BLE == true
    if (!console_active && bt_ready) update_bt();
  #endif

  #if HAS_INPUT
    input_read();
  #endif

  if (memory_low) {
    #if PLATFORM == PLATFORM_ESP32
      if (esp_get_free_heap_size() < 8192) {
        kiss_indicate_error(ERROR_MEMORY_LOW); memory_low = false;
      } else {
        memory_low = false;
      }
    #else
      kiss_indicate_error(ERROR_MEMORY_LOW); memory_low = false;
    #endif
  }
}

void sleep_now() {
  #if HAS_SLEEP == true
    #if PLATFORM == PLATFORM_ESP32
      #if BOARD_MODEL == BOARD_T3S3
        display_intensity = 0;
        update_display(true);
      #endif
      #if PIN_DISP_SLEEP >= 0
        pinMode(PIN_DISP_SLEEP, OUTPUT);
        digitalWrite(PIN_DISP_SLEEP, DISP_SLEEP_LEVEL);
      #endif
      #if HAS_BLUETOOTH
        if (bt_state == BT_STATE_CONNECTED) {
          bt_stop();
          delay(100);
        }
      #endif
      esp_sleep_enable_ext0_wakeup(PIN_WAKEUP, WAKEUP_LEVEL);
      esp_deep_sleep_start();
    #elif PLATFORM == PLATFORM_NRF52
      #if BOARD_MODEL == BOARD_HELTEC_T114
        npset(0,0,0);
        digitalWrite(PIN_VEXT_EN, LOW);
        digitalWrite(PIN_T114_TFT_BLGT, HIGH);
        digitalWrite(PIN_T114_TFT_EN, HIGH);
      #elif BOARD_MODEL == BOARD_TECHO
        for (uint8_t i = display_intensity; i > 0; i--) { analogWrite(pin_backlight, i-1); delay(1); }
        epd_black(true); delay(300); epd_black(true); delay(300); epd_black(false);
        delay(2000);
        analogWrite(PIN_VEXT_EN, 0);
        delay(100);
      #endif
      sd_power_gpregret_set(0, 0x6d);
      nrf_gpio_cfg_sense_input(pin_btn_usr1, NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW);
      NRF_POWER->SYSTEMOFF = 1;
    #endif
  #endif
}

void button_event(uint8_t event, unsigned long duration) {
  #if MCU_VARIANT == MCU_ESP32 || MCU_VARIANT == MCU_NRF52
    if (display_blanked) {
      display_unblank();
    } else {
      if (duration > 10000) {
        #if HAS_CONSOLE
          #if HAS_BLUETOOTH || HAS_BLE
            bt_stop();
          #endif
          console_active = true;
          console_start();
        #endif
      } else if (duration > 5000) {
        #if HAS_BLUETOOTH || HAS_BLE
          if (bt_state != BT_STATE_CONNECTED) { bt_enable_pairing(); }
        #endif
      } else if (duration > 700) {
        #if HAS_SLEEP
          sleep_now();
        #endif
      } else {
        #if HAS_BLUETOOTH || HAS_BLE
        if (bt_state != BT_STATE_CONNECTED) {
          if (bt_state == BT_STATE_OFF) {
            bt_start();
            bt_conf_save(true);
          } else {
            bt_stop();
            bt_conf_save(false);
          }
        }
        #endif
      }
    }
  #endif
}

volatile bool serial_polling = false;
void serial_poll() {
  serial_polling = true;

  #if MCU_VARIANT != MCU_ESP32 && MCU_VARIANT != MCU_NRF52
  while (!fifo_isempty_locked(&serialFIFO)) {
  #else
  while (!fifo_isempty(&serialFIFO)) {
  #endif
    char sbyte = fifo_pop(&serialFIFO);
    serial_callback(sbyte);
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

    #if HAS_BLUETOOTH || HAS_BLE == true
    while (
      c < MAX_CYCLES &&
      ( (bt_state != BT_STATE_CONNECTED && Serial.available()) || (bt_state == BT_STATE_CONNECTED && SerialBT.available()) )
      )
    #else
    while (c < MAX_CYCLES && Serial.available())
    #endif
    {
      c++;

      #if MCU_VARIANT != MCU_ESP32 && MCU_VARIANT != MCU_NRF52
        if (!fifo_isfull_locked(&serialFIFO)) {
          fifo_push_locked(&serialFIFO, Serial.read());
        }
      #elif HAS_BLUETOOTH || HAS_BLE == true
        if (bt_state == BT_STATE_CONNECTED) {
          if (!fifo_isfull(&serialFIFO)) {
            fifo_push(&serialFIFO, SerialBT.read());
          }
        } else {
          if (!fifo_isfull(&serialFIFO)) {
            fifo_push(&serialFIFO, Serial.read());
          }
        }
      #else
        if (!fifo_isfull(&serialFIFO)) {
          fifo_push(&serialFIFO, Serial.read());
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
  ISR(TIMER3_CAPT_vect) { buffer_serial(); }
#endif
