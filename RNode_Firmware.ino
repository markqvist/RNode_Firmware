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

#if BOARD_MODEL == BOARD_TWATCH_ULT
  #include "esp_task_wdt.h"
  #include "XL9555.h"
  #include "CO5300.h"
  #include "DRV2605.h"

  // BHI260AP sensor hub — IMU + step counter + wrist wake
  #include <SensorBHI260AP.hpp>
  #include <bosch/BoschSensorDataHelper.hpp>
  #define BOSCH_BHI260_GPIO
  #include <BoschFirmware.h>
  SensorBHI260AP *bhi260 = NULL;
  bool bhi260_ready = false;
  volatile uint32_t imu_step_count = 0;
  volatile bool imu_wrist_tilt = false;

  // Filtered accelerometer for bubble level with adaptive noise tracking
  volatile float imu_ax_f = 0, imu_ay_f = 0, imu_az_f = 4096;
  volatile float imu_noise = 0;  // running estimate of accel noise (0=still, 1+=noisy)
  void imu_accel_live_cb(uint8_t sensor_id, uint8_t *data, uint32_t size, uint64_t *timestamp, void *user_data) {
    if (size >= 6) {
      float ax = (int16_t)(data[0] | (data[1] << 8));
      float ay = (int16_t)(data[2] | (data[3] << 8));
      float az = (int16_t)(data[4] | (data[5] << 8));
      // Noise: EMA of squared deviation from filtered value
      float dx = ax - imu_ax_f, dy = ay - imu_ay_f, dz = az - imu_az_f;
      float dev = (dx*dx + dy*dy + dz*dz) / (4096.0f * 4096.0f);
      imu_noise += 0.1f * (dev - imu_noise);
      // Adaptive EMA: responsive when quiet, smooth when noisy
      float alpha = (imu_noise < 0.001f) ? 0.4f :
                    (imu_noise < 0.01f)  ? 0.2f : 0.08f;
      imu_ax_f += alpha * (ax - imu_ax_f);
      imu_ay_f += alpha * (ay - imu_ay_f);
      imu_az_f += alpha * (az - imu_az_f);
    }
  }

  // MAX98357A I2S speaker + SPM1423 PDM microphone
  #include "Speaker.h"
  #include "Microphone.h"

  // Sensor data logger to SD card (must be before callbacks that call sensor_log_*)
  #include "IMULogger.h"

  // IMU sensor callbacks
  void imu_step_cb(uint8_t sensor_id, uint8_t *data, uint32_t size, uint64_t *timestamp, void *user_data) {
    if (size >= 4) {
      imu_step_count = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
      sensor_log_step(imu_step_count);
    }
  }

  void imu_wrist_tilt_cb(uint8_t sensor_id, uint8_t *data, uint32_t size, uint64_t *timestamp, void *user_data) {
    imu_wrist_tilt = true;
    sensor_log_wrist_tilt();
  }

  // Shared SPI bus mutex (LoRa + SD + NFC)
  #include "SharedSPI.h"
  SemaphoreHandle_t shared_spi_mutex = NULL;  // definition (declared extern in SharedSPI.h)

  // USB Mass Storage for SD card access (TinyUSB OTG mode only)
  #if !ARDUINO_USB_MODE
    #include "USBSD.h"
  #endif

  // CST9217 capacitive touch panel
  #include <touch/TouchDrvCST92xx.h>
  TouchDrvCST92xx touch;
  bool touch_ready = false;
  volatile bool touch_irq = false;
  #define TP_INT 12
  void IRAM_ATTR touch_isr() { touch_irq = true; }
#endif

#define CHANNEL_FIFO_SIZE (CONFIG_UART_BUFFER_SIZE / NUM_CHANNELS)
FIFOBuffer   channelFIFO[NUM_CHANNELS];
uint8_t      channelBuffer[NUM_CHANNELS][CHANNEL_FIFO_SIZE + 1];
ChannelState channel_state[NUM_CHANNELS];

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
  #define MODEM_QUEUE_SIZE 8
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

    // Hardware watchdog — auto-resets on lockup (30s timeout covers
    // BHI260AP firmware upload which takes ~10s at boot)
    #if BOARD_MODEL == BOARD_TWATCH_ULT
      esp_task_wdt_init(30, true);  // 30s timeout, panic on expire
      esp_task_wdt_add(NULL);       // subscribe current task (loopTask)
    #endif

    // Init shared SPI bus mutex before any SPI users
    #if BOARD_MODEL == BOARD_TWATCH_ULT
      shared_spi_init();
    #endif

    EEPROM.begin(EEPROM_SIZE);
    Serial.setRxBufferSize(CONFIG_UART_BUFFER_SIZE);

    // T-Watch Ultra: self-provision if EEPROM is blank (after flash erase)
    #if BOARD_MODEL == BOARD_TWATCH_ULT
    if (EEPROM.read(eeprom_addr(ADDR_PRODUCT)) == 0xFF) {
      EEPROM.write(eeprom_addr(ADDR_PRODUCT), PRODUCT_TWATCH_ULT);
      EEPROM.write(eeprom_addr(ADDR_MODEL), MODEL_DA);  // 868 MHz
      EEPROM.write(eeprom_addr(ADDR_HW_REV), 0x01);
      EEPROM.write(eeprom_addr(ADDR_SERIAL), 0x00);
      EEPROM.write(eeprom_addr(ADDR_SERIAL+1), 0x00);
      EEPROM.write(eeprom_addr(ADDR_SERIAL+2), 0x00);
      EEPROM.write(eeprom_addr(ADDR_SERIAL+3), 0x00);
      EEPROM.write(eeprom_addr(ADDR_MADE), 0x00);
      EEPROM.write(eeprom_addr(ADDR_MADE+1), 0x00);
      EEPROM.write(eeprom_addr(ADDR_MADE+2), 0x00);
      EEPROM.write(eeprom_addr(ADDR_MADE+3), 0x00);
      EEPROM.write(eeprom_addr(ADDR_INFO_LOCK), INFO_LOCK_BYTE);
      EEPROM.write(eeprom_addr(ADDR_CONF_OK), CONF_OK_BYTE);
      // Compute and write EEPROM checksum (MD5 of first CHECKSUMMED_SIZE bytes)
      char chk_data[CHECKSUMMED_SIZE];
      for (uint8_t i = 0; i < CHECKSUMMED_SIZE; i++)
        chk_data[i] = EEPROM.read(eeprom_addr(i));
      unsigned char *chk_hash = MD5::make_hash(chk_data, CHECKSUMMED_SIZE);
      for (uint8_t i = 0; i < 16; i++)
        EEPROM.write(eeprom_addr(ADDR_CHKSUM + i), chk_hash[i]);
      free(chk_hash);
      EEPROM.commit();
    }
    #endif

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
  #if MCU_VARIANT == MCU_ESP32
    // On ESP32, get the seed value from the
    // hardware RNG
    unsigned long seed_val = (unsigned long)esp_random();
  #elif MCU_VARIANT == MCU_NRF52
    // On nRF, get the seed value from the
    // hardware RNG
    unsigned long seed_val = get_rng_seed();
  #else
    // Otherwise, get a pseudo-random seed
    // value from an unconnected analog pin
    //
    // CAUTION! If you are implementing the
    // firmware on a platform that does not
    // have a hardware RNG, you MUST take
    // care to get a seed value with enough
    // entropy at each device reset!
    unsigned long seed_val = analogRead(0);
  #endif
  randomSeed(seed_val);

  // Initialise serial communication
  for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
    memset(channelBuffer[ch], 0, sizeof(channelBuffer[ch]));
    fifo_init(&channelFIFO[ch], channelBuffer[ch], CHANNEL_FIFO_SIZE);
    memset(&channel_state[ch], 0, sizeof(ChannelState));
    channel_state[ch].command = CMD_UNKNOWN;
  }

  Serial.begin(serial_baudrate);

  // USB MSC requires TinyUSB mode which adds ~900ms/loop overhead on ESP32-S3.
  // SD card access uses serial file transfer instead (debug command 'F').

  #if HAS_NP
    led_init();
  #endif

  #if MCU_VARIANT == MCU_NRF52 && HAS_NP == true
    boot_seq();
  #endif

  #if BOARD_MODEL != BOARD_RAK4631 && BOARD_MODEL != BOARD_HELTEC_T114 && BOARD_MODEL != BOARD_TECHO && BOARD_MODEL != BOARD_T3S3 && BOARD_MODEL != BOARD_TBEAM_S_V1 && BOARD_MODEL != BOARD_HELTEC32_V4 && BOARD_MODEL != BOARD_TWATCH_ULT
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
    if (pin_led_rx >= 0) pinMode(pin_led_rx, OUTPUT);
    if (pin_led_tx >= 0) pinMode(pin_led_tx, OUTPUT);
  #endif

  #if HAS_TCXO == true
    if (pin_tcxo_enable != -1) {
        pinMode(pin_tcxo_enable, OUTPUT);
        digitalWrite(pin_tcxo_enable, HIGH);
    }
  #endif

  // Initialise buffers
  memset(pbuf, 0, sizeof(pbuf));
  
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

    #if BOARD_MODEL == BOARD_XIAO_S3
      // Improve wakeup from sleep
      delay(300);
      LoRa->reset();
      delay(100);
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

    // T-Watch init order: display_init() MUST run BEFORE xl9555_init().
    // The XL9555 GPIO expander controls the display power gate (EXPANDS_DISP_EN),
    // but its outputs default HIGH at power-on, so the display is powered before
    // the expander is explicitly configured. Moving display_init() after xl9555_init()
    // causes a black screen because the power gate cycling disrupts the CO5300 QSPI
    // init sequence. Do not reorder.
    display_unblank();
    disp_ready = display_init();
    update_display();
  #endif

  #if MCU_VARIANT == MCU_ESP32 || MCU_VARIANT == MCU_NRF52

    #if HAS_PMU == true
      pmu_ready = init_pmu();
    #endif

    #if BOARD_MODEL == BOARD_TWATCH_ULT
      xl9555_init();
      xl9555_enable_lora_antenna();
      xl9555_set(EXPANDS_DRV_EN, true);   // Enable haptic motor driver
      xl9555_set(EXPANDS_DISP_EN, true);  // Confirm display power gate on
      xl9555_set(EXPANDS_TOUCH_RST, true);  // Release touch reset
      delay(100);
      drv2605_init();
      if (drv2605_ready) drv2605_play(HAPTIC_SHARP_CLICK);  // Boot feedback

      // Init touch panel
      touch.setPins(-1, TP_INT);  // No reset pin (handled by XL9555), INT on GPIO 12
      if (touch.begin(Wire, 0x1A, I2C_SDA, I2C_SCL)) {
        touch_ready = true;
        attachInterrupt(TP_INT, touch_isr, FALLING);

        // Register touch with LVGL GUI
        #if HAS_DISPLAY == true
        gui_set_touch_handler([](int16_t *x, int16_t *y) -> bool {
          if (!touch_ready) return false;
          return touch.getPoint(x, y, 1) > 0;
        });
        #endif
      }

      // Init speaker (BLDO2 already enabled by PMU init) and microphone
      speaker_init();
      mic_init();

      // USB MSC SD card — deferred to main loop (SPI bus needs LoRa init first)

      // BHI260AP init deferred — firmware upload takes ~10s at 1MHz I2C
      // and blocks serial communication during boot. Will be initialized
      // lazily from the main loop after radio is up.

      // Beacon timer wakeup: if we woke from deep sleep via timer,
      // take the fast path — init GPS/LoRa only, transmit, sleep again.
      // esp_reset_reason() reliably distinguishes deep sleep from cold boot.
      #if HAS_GPS == true
        if (esp_reset_reason() == ESP_RST_DEEPSLEEP &&
            esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
          beacon_wake_cycle();  // Does not return
        }
      #endif
    #endif

    #if HAS_BLUETOOTH || HAS_BLE == true
      bt_init();
      bt_init_ran = true;
    #endif

    #if HAS_RTC == true
      rtc_setup();
    #endif

    #if HAS_GPS == true
      gps_setup();
      // Load beacon encryption config from EEPROM (config region)
      if (EEPROM.read(config_addr(ADDR_BCN_OK)) == CONF_OK_BYTE) {
          for (int i = 0; i < 32; i++)
              collector_pub_key[i] = EEPROM.read(config_addr(ADDR_BCN_KEY + i));
          for (int i = 0; i < 16; i++)
              collector_identity_hash[i] = EEPROM.read(config_addr(ADDR_BCN_IHASH + i));
          for (int i = 0; i < 16; i++)
              collector_dest_hash[i] = EEPROM.read(config_addr(ADDR_BCN_DHASH + i));
          beacon_crypto_configured = true;
      }
      // Initialize LXMF identity (load from NVS or generate new)
      lxmf_init_identity();
      // Initialize IFAC authentication (load from NVS, or self-provision)
      ifac_init();
      if (!ifac_configured) {
          ifac_self_provision(BEACON_NETWORK_NAME, BEACON_PASSPHRASE);
      }
      // Load user settings from config EEPROM
      uint8_t s_disp = EEPROM.read(config_addr(ADDR_CONF_DISP_TIMEOUT));
      if (s_disp != 0xFF && s_disp >= 5 && s_disp <= 60)
          display_blanking_timeout = (uint32_t)s_disp * 1000;
      uint8_t s_bcn_int = EEPROM.read(config_addr(ADDR_CONF_BCN_INT));
      if (s_bcn_int != 0xFF && s_bcn_int < BEACON_INTERVAL_OPTIONS_COUNT)
          beacon_interval_ms = beacon_interval_options[s_bcn_int];
      uint8_t s_gps_model = EEPROM.read(config_addr(ADDR_CONF_GPS_MODEL));
      if (s_gps_model != 0xFF && s_gps_model < GPS_MODEL_OPTIONS_COUNT)
          gps_set_dynamic_model(s_gps_model);
      uint8_t s_bcn_en = EEPROM.read(config_addr(ADDR_CONF_BCN_EN));
      if (s_bcn_en != 0xFF)
          beacon_enabled = (s_bcn_en != 0);
    #endif

    if (console_active) {
      #if HAS_CONSOLE
        console_start();
      #else
        kiss_indicate_reset();
      #endif
    } else {
      #if HAS_WIFI
        wifi_mode = EEPROM.read(eeprom_addr(ADDR_CONF_WIFI));
        if (wifi_mode == WR_WIFI_STA || wifi_mode == WR_WIFI_AP) { wifi_remote_init(); }
      #endif
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
  
  for (uint16_t i = 0; i < host_write_len; i++) {
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
  host_write_len = 0;

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
      stat_rx++;

      #if MCU_VARIANT != MCU_ESP32 && MCU_VARIANT != MCU_NRF52
        // We first signal the RSSI of the
        // recieved packet to the host.
        response_channel = data_channel;
        kiss_indicate_stat_rssi();
        kiss_indicate_stat_snr();

        // And then write the entire packet
        host_write_len = read_len;
        kiss_write_packet(); read_len = 0;

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
        memcpy(modem_packet->data, pbuf, read_len); read_len = 0;
        if (!modem_packet_queue || xQueueSendFromISR(modem_packet_queue, &modem_packet, NULL) != pdPASS) {
            free(modem_packet);
        }
      #endif
    }
  } else {
    // In promiscuous mode, raw packets are
    // output directly to the host
    read_len = 0;
    stat_rx++;

    #if MCU_VARIANT != MCU_ESP32 && MCU_VARIANT != MCU_NRF52
      last_rssi = LoRa->packetRssi();
      last_snr_raw = LoRa->packetSnrRaw();
      getPacketData(packet_size);

      // We first signal the RSSI of the
      // recieved packet to the host.
      response_channel = data_channel;
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
    led_tx_on();

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

        transmit(length);
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
    queue_flushing = true; led_tx_on();

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

        transmit(length);
      }
      queue_height -= 1;
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

    response_channel = data_channel;
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
      stat_tx++;

    } else {
      led_tx_on(); uint16_t written = 0;
      if (size > SINGLE_MTU) { size = SINGLE_MTU; }
      if (!implicit) { LoRa->beginPacket(); }
      else           { LoRa->beginPacket(size); }
      for (uint16_t i=0; i < size; i++) { LoRa->write(tbuf[i]); written++; }
      LoRa->endPacket(); add_airtime(written);
      stat_tx++;
    }

  } else { kiss_indicate_error(ERROR_TXFAILED); led_indicate_error(5); }
}

// Transmit raw RNS packet without the 1-byte RNode LoRa header.
// Used by beacon mode so the receiving RNode passes the packet
// directly to Reticulum without a spurious header byte.
// Diagnostic: dump last beacon packet (pre and post IFAC)
uint8_t  diag_beacon_pre[256];
uint16_t diag_beacon_pre_len = 0;
uint8_t  diag_beacon_post[256];
uint16_t diag_beacon_post_len = 0;

void beacon_transmit(uint16_t size) {
  if (radio_online) {
    // Save pre-IFAC packet for diagnostics
    if (size <= 256) {
      memcpy(diag_beacon_pre, tbuf, size);
      diag_beacon_pre_len = size;
    }

    #if HAS_GPS == true
      size = ifac_apply(tbuf, size);
    #endif

    // Save post-IFAC packet
    if (size <= 256) {
      memcpy(diag_beacon_post, tbuf, size);
      diag_beacon_post_len = size;
    }

    LoRa->beginPacket();
    for (uint16_t i = 0; i < size; i++) {
      LoRa->write(tbuf[i]);
    }
    if (!LoRa->endPacket()) {
      led_indicate_error(5);
    }
    add_airtime(size);
    lora_receive();
  }
}

void serial_callback(uint8_t sbyte, uint8_t ch) {
  ChannelState *cs = &channel_state[ch];
  if (cs->in_frame && sbyte == FEND && cs->command == CMD_DATA) {
    cs->in_frame = false;

    #if NUM_CHANNELS > 1
    if (cs->pkt_len >= MIN_L && !fifo16_isfull(&packet_starts)
        && queue_height < CONFIG_QUEUE_MAX_LENGTH && queued_bytes + cs->pkt_len <= CONFIG_QUEUE_SIZE) {
        uint16_t s = queue_cursor;
        for (uint16_t i = 0; i < cs->pkt_len; i++) {
            packet_queue[queue_cursor++] = cs->pktbuf[i];
            if (queue_cursor == CONFIG_QUEUE_SIZE) queue_cursor = 0;
        }
        queue_height++;
        queued_bytes += cs->pkt_len;
        fifo16_push(&packet_starts, s);
        fifo16_push(&packet_lengths, cs->pkt_len);
    }
    cs->pkt_len = 0;
    data_channel = ch;
    #else
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
    #endif

  } else if (sbyte == FEND) {
    cs->in_frame = true;
    cs->command = CMD_UNKNOWN;
    cs->frame_len = 0;
  } else if (cs->in_frame && cs->frame_len < MTU) {
    // Have a look at the command byte first
    if (cs->frame_len == 0 && cs->command == CMD_UNKNOWN) {
        cs->command = sbyte;
        #if HAS_GPS == true
          beacon_check_host_activity();
        #endif
    } else if (cs->command == CMD_DATA) {
        if (ch == CHANNEL_USB) {
          cable_state = CABLE_STATE_CONNECTED;
        }
        if (sbyte == FESC) {
            cs->escape = true;
        } else {
            if (cs->escape) {
                if (sbyte == TFEND) sbyte = FEND;
                if (sbyte == TFESC) sbyte = FESC;
                cs->escape = false;
            }
            #if NUM_CHANNELS > 1
            if (cs->pkt_len < MTU) {
              cs->pktbuf[cs->pkt_len++] = sbyte;
            }
            #else
            if (queue_height < CONFIG_QUEUE_MAX_LENGTH && queued_bytes < CONFIG_QUEUE_SIZE) {
              queued_bytes++;
              packet_queue[queue_cursor++] = sbyte;
              if (queue_cursor == CONFIG_QUEUE_SIZE) queue_cursor = 0;
            }
            #endif
        }
    } else if (cs->command == CMD_FREQUENCY) {
      if (sbyte == FESC) {
            cs->escape = true;
        } else {
            if (cs->escape) {
                if (sbyte == TFEND) sbyte = FEND;
                if (sbyte == TFESC) sbyte = FESC;
                cs->escape = false;
            }
            if (cs->frame_len < CMD_L) cs->cmdbuf[cs->frame_len++] = sbyte;
        }

        if (cs->frame_len == 4) {
          uint32_t freq = (uint32_t)cs->cmdbuf[0] << 24 | (uint32_t)cs->cmdbuf[1] << 16 | (uint32_t)cs->cmdbuf[2] << 8 | (uint32_t)cs->cmdbuf[3];

          if (freq == 0) {
            kiss_indicate_frequency();
          } else {
            lora_freq = freq;
            if (op_mode == MODE_HOST) setFrequency();
            kiss_indicate_frequency();
          }
        }
    } else if (cs->command == CMD_BANDWIDTH) {
      if (sbyte == FESC) {
            cs->escape = true;
        } else {
            if (cs->escape) {
                if (sbyte == TFEND) sbyte = FEND;
                if (sbyte == TFESC) sbyte = FESC;
                cs->escape = false;
            }
            if (cs->frame_len < CMD_L) cs->cmdbuf[cs->frame_len++] = sbyte;
        }

        if (cs->frame_len == 4) {
          uint32_t bw = (uint32_t)cs->cmdbuf[0] << 24 | (uint32_t)cs->cmdbuf[1] << 16 | (uint32_t)cs->cmdbuf[2] << 8 | (uint32_t)cs->cmdbuf[3];

          if (bw == 0) {
            kiss_indicate_bandwidth();
          } else {
            lora_bw = bw;
            if (op_mode == MODE_HOST) setBandwidth();
            kiss_indicate_bandwidth();
          }
        }
    } else if (cs->command == CMD_TXPOWER) {
      if (sbyte == 0xFF) {
        kiss_indicate_txpower();
      } else {
        int txp = sbyte;
        #if MODEM == SX1262
          #if HAS_LORA_PA
            if (txp > PA_MAX_OUTPUT) txp = PA_MAX_OUTPUT;
          #else
            if (txp > 22) txp = 22;
          #endif
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
    } else if (cs->command == CMD_SF) {
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
    } else if (cs->command == CMD_CR) {
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
    } else if (cs->command == CMD_IMPLICIT) {
      set_implicit_length(sbyte);
      kiss_indicate_implicit_length();
    } else if (cs->command == CMD_LEAVE) {
      if (sbyte == 0xFF) {
        display_unblank();
        cable_state   = CABLE_STATE_DISCONNECTED;
        current_rssi  = -292;
        last_rssi     = -292;
        last_rssi_raw = 0x00;
        last_snr_raw  = 0x80;
      }
    } else if (cs->command == CMD_RADIO_STATE) {
      if (ch == CHANNEL_USB) {
        cable_state = CABLE_STATE_CONNECTED;
        display_unblank();
      }
      if (sbyte == 0xFF) {
        kiss_indicate_radiostate();
      } else if (sbyte == 0x00) {
        stopRadio();
        kiss_indicate_radiostate();
      } else if (sbyte == 0x01) {
        // Force full restart to ensure clean SX1262 init with current params
        if (radio_online) stopRadio();
        startRadio();
        kiss_indicate_radiostate();
      }
    } else if (cs->command == CMD_ST_ALOCK) {
      if (sbyte == FESC) {
            cs->escape = true;
        } else {
            if (cs->escape) {
                if (sbyte == TFEND) sbyte = FEND;
                if (sbyte == TFESC) sbyte = FESC;
                cs->escape = false;
            }
            if (cs->frame_len < CMD_L) cs->cmdbuf[cs->frame_len++] = sbyte;
        }

        if (cs->frame_len == 2) {
          uint16_t at = (uint16_t)cs->cmdbuf[0] << 8 | (uint16_t)cs->cmdbuf[1];

          if (at == 0) {
            st_airtime_limit = 0.0;
          } else {
            st_airtime_limit = (float)at/(100.0*100.0);
            if (st_airtime_limit >= 1.0) { st_airtime_limit = 0.0; }
          }
          kiss_indicate_st_alock();
        }
    } else if (cs->command == CMD_LT_ALOCK) {
      if (sbyte == FESC) {
            cs->escape = true;
        } else {
            if (cs->escape) {
                if (sbyte == TFEND) sbyte = FEND;
                if (sbyte == TFESC) sbyte = FESC;
                cs->escape = false;
            }
            if (cs->frame_len < CMD_L) cs->cmdbuf[cs->frame_len++] = sbyte;
        }

        if (cs->frame_len == 2) {
          uint16_t at = (uint16_t)cs->cmdbuf[0] << 8 | (uint16_t)cs->cmdbuf[1];

          if (at == 0) {
            lt_airtime_limit = 0.0;
          } else {
            lt_airtime_limit = (float)at/(100.0*100.0);
            if (lt_airtime_limit >= 1.0) { lt_airtime_limit = 0.0; }
          }
          kiss_indicate_lt_alock();
        }
    } else if (cs->command == CMD_STAT_RX) {
      kiss_indicate_stat_rx();
    } else if (cs->command == CMD_STAT_TX) {
      kiss_indicate_stat_tx();
    } else if (cs->command == CMD_STAT_RSSI) {
      kiss_indicate_stat_rssi();
    #if HAS_GPS == true
    } else if (cs->command == CMD_STAT_GPS) {
      kiss_indicate_stat_gps();
    #endif
    } else if (cs->command == CMD_RADIO_LOCK) {
      update_radio_lock();
      kiss_indicate_radio_lock();
    } else if (cs->command == CMD_BLINK) {
      led_indicate_info(sbyte);
    } else if (cs->command == CMD_RANDOM) {
      kiss_indicate_random(getRandom());
    } else if (cs->command == CMD_DETECT) {
      if (sbyte == DETECT_REQ) {
        if (ch == CHANNEL_USB) cable_state = CABLE_STATE_CONNECTED;
        kiss_indicate_detect();
      }
    } else if (cs->command == CMD_PROMISC) {
      if (sbyte == 0x01) {
        promisc_enable();
      } else if (sbyte == 0x00) {
        promisc_disable();
      }
      kiss_indicate_promisc();
    } else if (cs->command == CMD_READY) {
      if (!queue_full()) {
        kiss_indicate_ready();
      } else {
        kiss_indicate_not_ready();
      }
    } else if (cs->command == CMD_UNLOCK_ROM) {
      if (sbyte == ROM_UNLOCK_BYTE) {
        unlock_rom();
      }
    } else if (cs->command == CMD_RESET) {
      if (sbyte == CMD_RESET_BYTE) {
        hard_reset();
      }
    } else if (cs->command == CMD_ROM_READ) {
      kiss_dump_eeprom();
    } else if (cs->command == CMD_CFG_READ) {
      kiss_dump_config();
    } else if (cs->command == CMD_ROM_WRITE) {
      if (sbyte == FESC) {
            cs->escape = true;
        } else {
            if (cs->escape) {
                if (sbyte == TFEND) sbyte = FEND;
                if (sbyte == TFESC) sbyte = FESC;
                cs->escape = false;
            }
            if (cs->frame_len < CMD_L) cs->cmdbuf[cs->frame_len++] = sbyte;
        }

        if (cs->frame_len == 2) {
          eeprom_write(cs->cmdbuf[0], cs->cmdbuf[1]);
        }
    } else if (cs->command == CMD_FW_VERSION) {
      kiss_indicate_version();
    } else if (cs->command == CMD_PLATFORM) {
      kiss_indicate_platform();
    } else if (cs->command == CMD_MCU) {
      kiss_indicate_mcu();
    } else if (cs->command == CMD_BOARD) {
      kiss_indicate_board();
    } else if (cs->command == CMD_CONF_SAVE) {
      eeprom_conf_save();
    } else if (cs->command == CMD_CONF_DELETE) {
      eeprom_conf_delete();
    } else if (cs->command == CMD_FB_EXT) {
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
    } else if (cs->command == CMD_FB_WRITE) {
      if (sbyte == FESC) {
            cs->escape = true;
        } else {
            if (cs->escape) {
                if (sbyte == TFEND) sbyte = FEND;
                if (sbyte == TFESC) sbyte = FESC;
                cs->escape = false;
            }
            if (cs->frame_len < CMD_L) cs->cmdbuf[cs->frame_len++] = sbyte;
        }
        #if HAS_DISPLAY
          if (cs->frame_len == 9) {
            uint8_t line = cs->cmdbuf[0];
            if (line > 63) line = 63;
            int fb_o = line*8; 
            memcpy(fb+fb_o, cs->cmdbuf+1, 8);
          }
        #endif
    } else if (cs->command == CMD_FB_READ) {
      if (sbyte != 0x00) { kiss_indicate_fb(); }
    } else if (cs->command == CMD_DISP_READ) {
      if (sbyte != 0x00) { kiss_indicate_disp(); }
    } else if (cs->command == CMD_DEV_HASH) {
      #if MCU_VARIANT == MCU_ESP32 || MCU_VARIANT == MCU_NRF52
        if (sbyte != 0x00) {
          kiss_indicate_device_hash();
        }
      #endif
    } else if (cs->command == CMD_DEV_SIG) {
      #if MCU_VARIANT == MCU_ESP32 || MCU_VARIANT == MCU_NRF52
        if (sbyte == FESC) {
              cs->escape = true;
          } else {
              if (cs->escape) {
                  if (sbyte == TFEND) sbyte = FEND;
                  if (sbyte == TFESC) sbyte = FESC;
                  cs->escape = false;
              }
              if (cs->frame_len < CMD_L) cs->cmdbuf[cs->frame_len++] = sbyte;
          }

          if (cs->frame_len == DEV_SIG_LEN) {
            memcpy(dev_sig, cs->cmdbuf, DEV_SIG_LEN);
            device_save_signature();
          }
      #endif
    } else if (cs->command == CMD_FW_UPD) {
      if (sbyte == 0x01) {
        firmware_update_mode = true;
      } else {
        firmware_update_mode = false;
      }
    } else if (cs->command == CMD_HASHES) {
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
    } else if (cs->command == CMD_FW_HASH) {
      #if MCU_VARIANT == MCU_ESP32 || MCU_VARIANT == MCU_NRF52
        if (sbyte == FESC) {
              cs->escape = true;
          } else {
              if (cs->escape) {
                  if (sbyte == TFEND) sbyte = FEND;
                  if (sbyte == TFESC) sbyte = FESC;
                  cs->escape = false;
              }
              if (cs->frame_len < CMD_L) cs->cmdbuf[cs->frame_len++] = sbyte;
          }

          if (cs->frame_len == DEV_HASH_LEN) {
            memcpy(dev_firmware_hash_target, cs->cmdbuf, DEV_HASH_LEN);
            device_save_firmware_hash();
          }
      #endif
    } else if (cs->command == CMD_WIFI_CHN) {
      #if HAS_WIFI
        if (sbyte > 0 && sbyte < 14) { eeprom_update(eeprom_addr(ADDR_CONF_WCHN), sbyte); }
      #endif
    } else if (cs->command == CMD_WIFI_MODE) {
      #if HAS_WIFI
        if (sbyte == WR_WIFI_OFF || sbyte == WR_WIFI_STA || sbyte == WR_WIFI_AP) {
          wr_conf_save(sbyte);
          wifi_mode = sbyte;
          wifi_remote_init();
        }
      #endif
    } else if (cs->command == CMD_WIFI_SSID) {
      #if HAS_WIFI
        if (sbyte == FESC) { cs->escape = true; }
        else {
          if (cs->escape) {
            if (sbyte == TFEND) sbyte = FEND;
            if (sbyte == TFESC) sbyte = FESC;
            cs->escape = false;
          }
          if (cs->frame_len < CMD_L) cs->cmdbuf[cs->frame_len++] = sbyte;
        }

        if (sbyte == 0x00) {
          for (uint8_t i = 0; i<33; i++) {
            if (i<cs->frame_len && i<32) { eeprom_update(config_addr(ADDR_CONF_SSID+i), cs->cmdbuf[i]); }
            else                     { eeprom_update(config_addr(ADDR_CONF_SSID+i), 0x00); }
          }
        }
      #endif
    } else if (cs->command == CMD_WIFI_PSK) {
      #if HAS_WIFI
        if (sbyte == FESC) { cs->escape = true; }
        else {
          if (cs->escape) {
            if (sbyte == TFEND) sbyte = FEND;
            if (sbyte == TFESC) sbyte = FESC;
            cs->escape = false;
          }
          if (cs->frame_len < CMD_L) cs->cmdbuf[cs->frame_len++] = sbyte;
        }

        if (sbyte == 0x00) {
          for (uint8_t i = 0; i<33; i++) {
            if (i<cs->frame_len && i<32) { eeprom_update(config_addr(ADDR_CONF_PSK+i), cs->cmdbuf[i]); }
            else                     { eeprom_update(config_addr(ADDR_CONF_PSK+i), 0x00); }
          }
        }
      #endif
    } else if (cs->command == CMD_WIFI_IP) {
      #if HAS_WIFI
        if (sbyte == FESC) { cs->escape = true; }
        else {
          if (cs->escape) {
            if (sbyte == TFEND) sbyte = FEND;
            if (sbyte == TFESC) sbyte = FESC;
            cs->escape = false;
          }
          if (cs->frame_len < CMD_L) cs->cmdbuf[cs->frame_len++] = sbyte;
        }

        if (cs->frame_len == 4) { for (uint8_t i = 0; i<4; i++) { eeprom_update(config_addr(ADDR_CONF_IP+i), cs->cmdbuf[i]); } }
      #endif
    } else if (cs->command == CMD_WIFI_NM) {
      #if HAS_WIFI
        if (sbyte == FESC) { cs->escape = true; }
        else {
          if (cs->escape) {
            if (sbyte == TFEND) sbyte = FEND;
            if (sbyte == TFESC) sbyte = FESC;
            cs->escape = false;
          }
          if (cs->frame_len < CMD_L) cs->cmdbuf[cs->frame_len++] = sbyte;
        }

        if (cs->frame_len == 4) { for (uint8_t i = 0; i<4; i++) { eeprom_update(config_addr(ADDR_CONF_NM+i), cs->cmdbuf[i]); } }
      #endif
    } else if (cs->command == CMD_BCN_KEY) {
      #if HAS_GPS == true
        if (sbyte == FESC) { cs->escape = true; }
        else {
          if (cs->escape) {
            if (sbyte == TFEND) sbyte = FEND;
            if (sbyte == TFESC) sbyte = FESC;
            cs->escape = false;
          }
          if (cs->frame_len < CMD_L) cs->cmdbuf[cs->frame_len++] = sbyte;
        }
        // 64 bytes: 32B X25519 pub key + 16B identity hash + 16B dest hash
        if (cs->frame_len == 64) {
          for (int i = 0; i < 32; i++)
            eeprom_update(config_addr(ADDR_BCN_KEY + i), cs->cmdbuf[i]);
          for (int i = 0; i < 16; i++)
            eeprom_update(config_addr(ADDR_BCN_IHASH + i), cs->cmdbuf[32 + i]);
          for (int i = 0; i < 16; i++)
            eeprom_update(config_addr(ADDR_BCN_DHASH + i), cs->cmdbuf[48 + i]);
          eeprom_update(config_addr(ADDR_BCN_OK), CONF_OK_BYTE);
          // Load into RAM immediately
          memcpy(collector_pub_key, cs->cmdbuf, 32);
          memcpy(collector_identity_hash, cs->cmdbuf + 32, 16);
          memcpy(collector_dest_hash, cs->cmdbuf + 48, 16);
          beacon_crypto_configured = true;
          lxmf_provisioned_at = millis();
          kiss_indicate_ready();
        }
      #endif
    } else if (cs->command == CMD_LXMF_HASH) {
      #if HAS_GPS == true
        // Return the RNode's LXMF source hash (16 bytes) for display/debugging.
        // Any byte triggers the response (query command).
        if (lxmf_identity_configured) {
          serial_write(FEND);
          serial_write(CMD_LXMF_HASH);
          for (int i = 0; i < 16; i++) {
            uint8_t b = lxmf_source_hash[i];
            if (b == FEND) { serial_write(FESC); serial_write(TFEND); }
            else if (b == FESC) { serial_write(FESC); serial_write(TFESC); }
            else serial_write(b);
          }
          serial_write(FEND);
        }
      #endif
    } else if (cs->command == CMD_LXMF_TEST) {
      #if HAS_GPS == true
        // Force-trigger LXMF announce + beacon for USB testing.
        // Emits pre-encryption plaintext as CMD_DIAG frames.
        lxmf_test_send();
      #endif
    } else if (cs->command == CMD_IFAC_KEY) {
      #if HAS_GPS == true
        if (sbyte == FESC) { cs->escape = true; }
        else {
          if (cs->escape) {
            if (sbyte == TFEND) sbyte = FEND;
            if (sbyte == TFESC) sbyte = FESC;
            cs->escape = false;
          }
          if (cs->frame_len < CMD_L) cs->cmdbuf[cs->frame_len++] = sbyte;
        }
        // 64 bytes: IFAC key derived from network_name + passphrase
        if (cs->frame_len == 64) {
          memcpy(ifac_key, cs->cmdbuf, 64);
          ifac_nvs_save();
          ifac_derive_keypair();
          ifac_configured = true;
          kiss_indicate_ready();
        }
      #endif
    } else if (cs->command == CMD_TRANSPORT_ID) {
      #if HAS_GPS == true
        if (sbyte == FESC) { cs->escape = true; }
        else {
          if (cs->escape) {
            if (sbyte == TFEND) sbyte = FEND;
            if (sbyte == TFESC) sbyte = FESC;
            cs->escape = false;
          }
          if (cs->frame_len < CMD_L) cs->cmdbuf[cs->frame_len++] = sbyte;
        }
        // 16 bytes: transport node's identity hash
        if (cs->frame_len == 16) {
          memcpy(transport_id, cs->cmdbuf, 16);
          lxmf_nvs_save_transport_id();
          transport_configured = true;
          kiss_indicate_ready();
        }
      #endif
    } else if (cs->command == CMD_BT_CTRL) {
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
    } else if (cs->command == CMD_BT_UNPAIR) {
      #if HAS_BLE
        if (sbyte == 0x01) { bt_debond_all(); }
      #endif
    } else if (cs->command == CMD_DISP_INT) {
      #if HAS_DISPLAY
        if (sbyte == FESC) {
            cs->escape = true;
        } else {
            if (cs->escape) {
                if (sbyte == TFEND) sbyte = FEND;
                if (sbyte == TFESC) sbyte = FESC;
                cs->escape = false;
            }
            display_intensity = sbyte;
            di_conf_save(display_intensity);
            display_unblank();
        }
      #endif
    } else if (cs->command == CMD_DISP_ADDR) {
      #if HAS_DISPLAY
        if (sbyte == FESC) {
            cs->escape = true;
        } else {
            if (cs->escape) {
                if (sbyte == TFEND) sbyte = FEND;
                if (sbyte == TFESC) sbyte = FESC;
                cs->escape = false;
            }
            display_addr = sbyte;
            da_conf_save(display_addr);
        }

      #endif
    } else if (cs->command == CMD_DISP_BLNK) {
      #if HAS_DISPLAY
        if (sbyte == FESC) {
            cs->escape = true;
        } else {
            if (cs->escape) {
                if (sbyte == TFEND) sbyte = FEND;
                if (sbyte == TFESC) sbyte = FESC;
                cs->escape = false;
            }
            db_conf_save(sbyte);
            display_unblank();
        }
      #endif
    } else if (cs->command == CMD_DISP_ROT) {
      #if HAS_DISPLAY
        if (sbyte == FESC) {
            cs->escape = true;
        } else {
            if (cs->escape) {
                if (sbyte == TFEND) sbyte = FEND;
                if (sbyte == TFESC) sbyte = FESC;
                cs->escape = false;
            }
            drot_conf_save(sbyte);
            display_unblank();
        }
      #endif
    } else if (cs->command == CMD_DIS_IA) {
      if (sbyte == FESC) {
          cs->escape = true;
      } else {
          if (cs->escape) {
              if (sbyte == TFEND) sbyte = FEND;
              if (sbyte == TFESC) sbyte = FESC;
              cs->escape = false;
          }
          dia_conf_save(sbyte);
      }
    } else if (cs->command == CMD_DISP_RCND) {
      #if HAS_DISPLAY
        if (sbyte == FESC) {
            cs->escape = true;
        } else {
            if (cs->escape) {
                if (sbyte == TFEND) sbyte = FEND;
                if (sbyte == TFESC) sbyte = FESC;
                cs->escape = false;
            }
            if (sbyte > 0x00) recondition_display = true;
        }
      #endif
    } else if (cs->command == CMD_NP_INT) {
      #if HAS_NP
        if (sbyte == FESC) {
            cs->escape = true;
        } else {
            if (cs->escape) {
                if (sbyte == TFEND) sbyte = FEND;
                if (sbyte == TFESC) sbyte = FESC;
                cs->escape = false;
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
      #if BOARD_MODEL != BOARD_HELTEC32_V4
      if (!noise_floor_sampled || current_rssi < noise_floor + CSMA_INFR_THRESHOLD_DB) {
      #else
      if ((!noise_floor_sampled || current_rssi < noise_floor + CSMA_INFR_THRESHOLD_DB) || (noise_floor_sampled && (noise_floor < LNA_GD_THRSHLD && current_rssi <= LNA_GD_LIMIT))) {
      #endif
        #if HAS_LORA_LNA
          // Discard invalid samples due to gain variance
          // during LoRa LNA re-calibration
          if (current_rssi < noise_floor-LORA_LNA_GVT) { return; }
        #endif
        bool sum_noise_floor = false;
        noise_floor_buffer[noise_floor_sample] = current_rssi;
        noise_floor_sample = noise_floor_sample+1;
        if (noise_floor_sample >= NOISE_FLOOR_SAMPLES) {
          noise_floor_sample %= NOISE_FLOOR_SAMPLES;
          noise_floor_sampled = true;
          sum_noise_floor = true;
        }

        if (noise_floor_sampled && sum_noise_floor) {
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
uint32_t interference_start = 0;
bool interference_persists = false;
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

  #if BOARD_MODEL == BOARD_HELTEC32_V4
    if (noise_floor > LNA_GD_THRSHLD)  { interference_detected = !carrier_detected && (current_rssi > (noise_floor+CSMA_INFR_THRESHOLD_DB)); }
    else                               { interference_detected = !carrier_detected && (current_rssi > LNA_GD_LIMIT); }
  #else
    interference_detected = !carrier_detected && (current_rssi > (noise_floor+CSMA_INFR_THRESHOLD_DB));
  #endif

  if (interference_detected) { if (led_id_filter < LED_ID_TRIG) { led_id_filter += 1; } }
  else                       { if (led_id_filter > 0) {led_id_filter -= 1; } }

  // Handle potential false interference detection due to
  // LNA recalibration, antenna swap, moving into new RF
  // environment or similar.
  if (interference_detected && current_rssi < CSMA_RFENV_RECAL_LIMIT_DB) {
    if (!interference_persists) { interference_persists = true; interference_start = millis(); }
    else {
      if (millis()-interference_start >= CSMA_RFENV_RECAL_MS) { noise_floor_sampled = false; interference_persists = false; }
    }
  } else { interference_persists = false; }

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
              bool should_flush = !lora_limit_rate && !lora_guard_rate;
              if (should_flush) { flush_queue(); } else { pop_queue(); }
              cw_wait_passed = 0; csma_cw = -1; difs_wait_start = -1; }
          }
        }
      }
    }
  }
}

void work_while_waiting() { loop(); }

void loop() {
  #if BOARD_MODEL == BOARD_TWATCH_ULT
    esp_task_wdt_reset();  // Feed watchdog
    uint32_t _prof_t0 = micros(), _prof_t1;
  #endif

  if (radio_online) {
    // Process deferred RX interrupt from main context
    // (avoids SPI bus contention from ISR)
    #if MODEM == SX1262
      if (LoRa->rxPending()) { LoRa->processRxInterrupt(); }
    #endif

    #if MCU_VARIANT == MCU_ESP32
      modem_packet_t *modem_packet = NULL;
      if(modem_packet_queue && xQueueReceive(modem_packet_queue, &modem_packet, 0) == pdTRUE && modem_packet) {
        host_write_len = modem_packet->len;
        last_rssi      = modem_packet->rssi;
        last_snr_raw   = modem_packet->snr_raw;
        memcpy(&pbuf, modem_packet->data, modem_packet->len);
        free(modem_packet);
        modem_packet = NULL;

        response_channel = data_channel;
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
        host_write_len = modem_packet->len;
        free(modem_packet);
        modem_packet = NULL;

        portENTER_CRITICAL();
        last_rssi = LoRa->packetRssi();
        last_snr_raw = LoRa->packetSnrRaw();
        portEXIT_CRITICAL();
        response_channel = data_channel;
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
      // Don't call stopRadio() — it calls SPI.end() which kills the bus.
      // rnsd can still configure the radio via KISS even without hw_ready.
    }
  }

  #if BOARD_MODEL == BOARD_TWATCH_ULT
  _prof_t1 = micros(); prof_radio_us = _prof_t1 - _prof_t0; _prof_t0 = _prof_t1;
  #endif

  #if MCU_VARIANT == MCU_ESP32 || MCU_VARIANT == MCU_NRF52
      buffer_serial();
      {
        bool has_data = false;
        for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
          if (!fifo_isempty(&channelFIFO[ch])) { has_data = true; break; }
        }
        if (has_data) serial_poll();
      }
  #else
    if (!fifo_isempty_locked(&channelFIFO[CHANNEL_USB])) serial_poll();
  #endif

  #if BOARD_MODEL == BOARD_TWATCH_ULT
  _prof_t1 = micros(); prof_serial_us = _prof_t1 - _prof_t0; _prof_t0 = _prof_t1;
  #endif

  #if HAS_DISPLAY
    if (disp_ready && !display_updating) update_display();
  #endif

  #if BOARD_MODEL == BOARD_TWATCH_ULT
  _prof_t1 = micros(); prof_display_us = _prof_t1 - _prof_t0; _prof_t0 = _prof_t1;
  #endif

  #if HAS_PMU
    if (pmu_ready) update_pmu();
  #endif

  #if BOARD_MODEL == BOARD_TWATCH_ULT
  _prof_t1 = micros(); prof_pmu_us = _prof_t1 - _prof_t0; _prof_t0 = _prof_t1;
  #endif

  #if HAS_GPS == true
    if (gps_ready) {
      gps_update();
      if (hw_ready) beacon_update();  // beacon needs provisioned radio
      #if HAS_RTC == true
        if (gps_has_fix) rtc_sync_from_gps(gps_parser);
      #endif

      // Enter beacon sleep cycle when in standalone mode after beacon TX
      // Don't sleep when on external power (USB) — keeps display and debug active
      #if BOARD_MODEL == BOARD_TWATCH_ULT
        if (beacon_mode_active && beacon_gate == 6 &&
            battery_state != BATTERY_STATE_CHARGING &&
            battery_state != BATTERY_STATE_CHARGED &&
            (last_host_activity == 0 || (millis() - last_host_activity >= BEACON_NO_HOST_TIMEOUT_MS))) {
          sleep_now();
        }
      #endif
    }
  #endif

  #if BOARD_MODEL == BOARD_TWATCH_ULT
  _prof_t1 = micros(); prof_gps_us = _prof_t1 - _prof_t0; _prof_t0 = _prof_t1;
  #endif

  #if HAS_RTC == true
    static uint32_t rtc_last_read = 0;
    if (rtc_ready && (millis() - rtc_last_read >= 1000)) {
      rtc_read_time();
      rtc_last_read = millis();
    }
  #endif

  #if HAS_BLUETOOTH || HAS_BLE == true
    if (!console_active && bt_ready) update_bt();
  #endif

  #if BOARD_MODEL == BOARD_TWATCH_ULT
  _prof_t1 = micros(); prof_bt_us = _prof_t1 - _prof_t0; _prof_t0 = _prof_t1;
  #endif

  #if HAS_WIFI
    if (wifi_initialized) update_wifi();
  #endif

  #if HAS_INPUT
    input_read();
  #endif

  // Touch panel — IRQ-driven display wake (LVGL handles touch input via polling)
  #if BOARD_MODEL == BOARD_TWATCH_ULT
    if (touch_ready && touch_irq) {
      touch_irq = false;
      #if HAS_DISPLAY
        if (display_blanked) display_unblank();
      #endif
    }

    // Screenshot: long-press BOOT button (GPIO 0) for 2 seconds
    #if HAS_SD && HAS_DISPLAY
    {
      static uint32_t btn_down_since = 0;
      static bool btn_action_taken = false;
      if (digitalRead(0) == LOW) {
        if (btn_down_since == 0) btn_down_since = millis();
        // Long press (2s): screenshot to SD
        if (!btn_action_taken && millis() - btn_down_since > 2000) {
          btn_action_taken = true;
          if (drv2605_ready) drv2605_play(HAPTIC_DOUBLE_CLICK);
          gui_screenshot_sd();
        }
      } else {
        // Short press: navigate home
        if (btn_down_since > 0 && !btn_action_taken && millis() - btn_down_since > 50) {
          if (display_blanked) {
            display_unblank();
          } else {
            lv_tileview_set_tile(gui_tileview, gui_tile_watch, LV_ANIM_ON);
            if (drv2605_ready) drv2605_play(HAPTIC_LIGHT_CLICK);
          }
        }
        btn_down_since = 0;
        btn_action_taken = false;
      }
    }
    #endif
  #endif

  // USB MSC SD card mode is toggled on demand via debug command 'D'

  // Deferred BHI260AP init — runs once after boot is complete
  // Firmware upload takes ~10s and blocks, so we do it after radio is up
  #if BOARD_MODEL == BOARD_TWATCH_ULT
    static uint32_t bhi260_next_try = 5000;
    if (!bhi260_ready && millis() > bhi260_next_try) {
      bhi260_next_try = millis() + 10000;  // retry every 10s
      if (bhi260 == NULL) {
        bhi260 = new SensorBHI260AP();
      }
      Wire.setClock(1000000UL);
      bhi260->setPins(-1);
      bhi260->setFirmware(bosch_firmware_image, bosch_firmware_size, false);
      bhi260->setBootFromFlash(false);
      if (bhi260->begin(Wire, 0x28, I2C_SDA, I2C_SCL)) {
        bhi260_ready = true;
        pinMode(SENSOR_INT, INPUT);

        // Enable wrist tilt gesture for display wake
        bhi260->configure(SensorBHI260AP::WRIST_TILT_GESTURE, 1.0, 0);
        bhi260->onResultEvent(SensorBHI260AP::WRIST_TILT_GESTURE, imu_wrist_tilt_cb);

        // Always-on accelerometer at 10Hz for bubble level
        bhi260->configure(SensorBHI260AP::ACCEL_PASSTHROUGH, 10.0, 0);
        bhi260->onResultEvent(SensorBHI260AP::ACCEL_PASSTHROUGH, imu_accel_live_cb);

        // Register IMU log toggle for remote debug
        #if HAS_SD && HAS_DISPLAY
        gui_log_toggle_fn = []() -> bool {
          if (!imu_logging) {
            return imu_log_start(bhi260);
          } else {
            imu_log_stop(bhi260);
            return false;
          }
        };
        gui_list_files_fn = []() {
          if (shared_spi_mutex) xSemaphoreTake(shared_spi_mutex, portMAX_DELAY);
          SPI.begin(SD_CLK, SD_MISO, SD_MOSI, SD_CS);
          if (SD.begin(SD_CS, SPI, 4000000, "/sd", 5)) {
            Serial.print("{\"files\":[");
            File root = SD.open("/");
            bool first = true;
            File f;
            while ((f = root.openNextFile())) {
              if (!first) Serial.print(",");
              Serial.printf("{\"name\":\"%s\",\"size\":%lu}", f.name(), (unsigned long)f.size());
              first = false;
              f.close();
            }
            root.close();
            SD.end();
            Serial.println("]}");
          } else {
            Serial.println("{\"error\":\"sd_init_failed\"}");
          }
          if (shared_spi_mutex) xSemaphoreGive(shared_spi_mutex);
        };
        gui_download_file_fn = [](uint8_t index) {
          if (shared_spi_mutex) xSemaphoreTake(shared_spi_mutex, portMAX_DELAY);
          SPI.begin(SD_CLK, SD_MISO, SD_MOSI, SD_CS);
          if (SD.begin(SD_CS, SPI, 4000000, "/sd", 5)) {
            File root = SD.open("/");
            File f;
            uint8_t i = 0;
            while ((f = root.openNextFile())) {
              if (i == index) {
                Serial.printf("{\"name\":\"%s\",\"size\":%lu}\n", f.name(), (unsigned long)f.size());
                uint8_t buf[512];
                while (f.available()) {
                  int n = f.read(buf, sizeof(buf));
                  Serial.write(buf, n);
                }
                f.close();
                root.close();
                SD.end();
                if (shared_spi_mutex) xSemaphoreGive(shared_spi_mutex);
                return;
              }
              f.close();
              i++;
            }
            root.close();
            SD.end();
            Serial.printf("{\"error\":\"index %d not found\"}\n", index);
          } else {
            Serial.println("{\"error\":\"sd_init_failed\"}");
          }
          if (shared_spi_mutex) xSemaphoreGive(shared_spi_mutex);
        };
        #endif

        // Enable step counter (low power, always-on)
        bhi260->configure(SensorBHI260AP::STEP_COUNTER, 1.0, 0);
        bhi260->onResultEvent(SensorBHI260AP::STEP_COUNTER, imu_step_cb);
      }
      Wire.setClock(400000UL);
    }

    // Process IMU events and handle wrist wake
    if (bhi260_ready) {
      bhi260->update();
      if (imu_wrist_tilt) {
        imu_wrist_tilt = false;
        #if HAS_DISPLAY
          if (display_blanked) {
            display_unblank();
            if (drv2605_ready) drv2605_play(HAPTIC_LIGHT_CLICK);
          }
        #endif
      }
      #if HAS_SD
        if (imu_logging) {
          imu_log_flush();
          // Log GPS at 1Hz when logging
          static uint32_t last_gps_log = 0;
          if (gps_has_fix && millis() - last_gps_log >= 1000) {
            sensor_log_gps(gps_lat, gps_lon, gps_alt, gps_speed, gps_hdop, gps_sats);
            last_gps_log = millis();
          }
        }
      #endif
    }
  #endif

  #if BOARD_MODEL == BOARD_TWATCH_ULT
  _prof_t1 = micros(); prof_imu_us = _prof_t1 - _prof_t0;
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

#if BOARD_MODEL == BOARD_TWATCH_ULT
// Shared deep sleep entry for T-Watch Ultra.
// Safely shuts down peripherals and enters ESP32 deep sleep.
// Does not return — device reboots on wake.
void twatch_enter_deep_sleep(bool beacon_timer) {
  // 0. Haptic feedback before sleep
  if (drv2605_ready) {
    drv2605_play(HAPTIC_SOFT_BUMP);
    delay(150);  // Let the motor spin briefly before powering down
  }

  // 1. Shut down audio and display before closing buses
  mic_end();
  speaker_end();
  #if HAS_DISPLAY
    co5300_sleep();
  #endif

  // 2. Gate display VCI power and disable haptics via XL9555
  xl9555_sleep_prepare();

  // 3. Disable PMU peripheral rails (no PMU->enableSleep — that bricks I2C!)
  pmu_prepare_sleep();

  // 4. Close communication buses
  #if HAS_GPS
    gps_serial.end();
  #endif
  Serial1.end();
  SPI.end();
  Wire.end();

  // 5. Reset unused GPIOs to INPUT (minimal leakage)
  // DO NOT touch I2C pins (GPIO 2/3) — external pullups, and setting
  // them to OPEN_DRAIN persists across battery-backed resets, bricking I2C.
  const uint8_t sleep_pins[] = {
    DISP_D0, DISP_D1, DISP_D2, DISP_D3,
    DISP_SCK, DISP_CS, DISP_TE, DISP_RST,
    RTC_INT, NFC_INT, SENSOR_INT, NFC_CS,
    I2S_BCLK, I2S_WCLK, I2S_DOUT, SD_CS,
    pin_mosi, pin_miso, pin_sclk, pin_cs,
    PIN_GPS_TX, PIN_GPS_RX, PIN_GPS_PPS,
    pin_reset, pin_busy, pin_dio,
  };
  for (auto p : sleep_pins) {
    gpio_reset_pin((gpio_num_t)p);  // Resets to INPUT, clears any drive
  }

  // 6. Configure wakeup sources
  esp_sleep_enable_ext1_wakeup(1ULL << PMU_IRQ, ESP_EXT1_WAKEUP_ANY_LOW);
  if (beacon_timer) {
    esp_sleep_enable_timer_wakeup((uint64_t)beacon_interval_ms * 1000ULL);
  }

  // 7. Enter deep sleep (does not return)
  esp_deep_sleep_start();
}

#if HAS_GPS == true
// Minimal boot path for beacon timer wakeup.
// Inits only GPS + LoRa, waits for fix, transmits beacon, sleeps again.
// Called from setup() on timer wake. Does not return.
void beacon_wake_cycle() {
  gps_setup();

  // Load beacon crypto config from EEPROM
  if (EEPROM.read(config_addr(ADDR_BCN_OK)) == CONF_OK_BYTE) {
    for (int i = 0; i < 32; i++)
      collector_pub_key[i] = EEPROM.read(config_addr(ADDR_BCN_KEY + i));
    for (int i = 0; i < 16; i++)
      collector_identity_hash[i] = EEPROM.read(config_addr(ADDR_BCN_IHASH + i));
    for (int i = 0; i < 16; i++)
      collector_dest_hash[i] = EEPROM.read(config_addr(ADDR_BCN_DHASH + i));
    beacon_crypto_configured = true;
  }
  lxmf_init_identity();

  // Wait for GPS fix (up to 60 seconds for warm start)
  uint32_t fix_start = millis();
  while (!gps_has_fix && (millis() - fix_start < 60000)) {
    gps_update();
    delay(100);
  }

  if (gps_has_fix) {
    last_host_activity = 0;
    last_beacon_tx = 0;
    beacon_update();
  }

  stopRadio();
  twatch_enter_deep_sleep(true);  // Sleep with beacon timer
}
#endif
#endif

void sleep_now() {
  #if HAS_SLEEP == true
    stopRadio(); // TODO: Check this on all platforms
    #if PLATFORM == PLATFORM_ESP32
      #if BOARD_MODEL == BOARD_T3S3 || BOARD_MODEL == BOARD_XIAO_S3
        #if HAS_DISPLAY
          display_intensity = 0;
          update_display(true);
        #endif
      #endif
      #if BOARD_MODEL == BOARD_HELTEC32_V4
          digitalWrite(LORA_PA_CPS, LOW);
          digitalWrite(LORA_PA_CSD, LOW);
          digitalWrite(LORA_PA_PWR_EN, LOW);
          digitalWrite(Vext, HIGH);
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

      #if BOARD_MODEL == BOARD_TWATCH_ULT
        #if HAS_GPS == true
          bool use_beacon_timer = beacon_mode_active;
        #else
          bool use_beacon_timer = false;
        #endif
        twatch_enter_deep_sleep(use_beacon_timer);

      #else
        esp_sleep_enable_ext0_wakeup(PIN_WAKEUP, WAKEUP_LEVEL);
        esp_deep_sleep_start();
      #endif
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
  while (!fifo_isempty_locked(&channelFIFO[CHANNEL_USB])) {
    char sbyte = fifo_pop(&channelFIFO[CHANNEL_USB]);
    response_channel = CHANNEL_USB;
    serial_callback(sbyte, CHANNEL_USB);
  }
  #else
  for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
    while (!fifo_isempty(&channelFIFO[ch])) {
      char sbyte = fifo_pop(&channelFIFO[ch]);
      response_channel = ch;
      serial_callback(sbyte, ch);
    }
  }
  #endif

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

    uint8_t c;

    // USB — always read
    c = 0;
    #if MCU_VARIANT != MCU_ESP32 && MCU_VARIANT != MCU_NRF52
    while (c < MAX_CYCLES && Serial.available()) {
      c++;
      if (!fifo_isfull_locked(&channelFIFO[CHANNEL_USB])) { fifo_push_locked(&channelFIFO[CHANNEL_USB], Serial.read()); }
    }
    #else
    while (c < MAX_CYCLES && Serial.available()) {
      c++;
      uint8_t sb = Serial.read();
      #if BOARD_MODEL == BOARD_TWATCH_ULT && HAS_DISPLAY
        gui_process_serial_byte(sb);
      #endif
      if (!fifo_isfull(&channelFIFO[CHANNEL_USB])) { fifo_push(&channelFIFO[CHANNEL_USB], sb); }
    }
    #endif

    #if HAS_BLUETOOTH || HAS_BLE == true
    c = 0;
    while (c < MAX_CYCLES && bt_state == BT_STATE_CONNECTED && SerialBT.available()) {
      c++;
      if (!fifo_isfull(&channelFIFO[CHANNEL_BT])) { fifo_push(&channelFIFO[CHANNEL_BT], SerialBT.read()); }
    }
    #endif

    #if HAS_WIFI == true
    c = 0;
    while (c < MAX_CYCLES && wifi_host_is_connected() && wifi_remote_available()) {
      c++;
      if (!fifo_isfull(&channelFIFO[CHANNEL_WIFI])) { fifo_push(&channelFIFO[CHANNEL_WIFI], wifi_remote_read()); }
    }
    #endif

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
