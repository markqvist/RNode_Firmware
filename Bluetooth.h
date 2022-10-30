#include "BluetoothSerial.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"

BluetoothSerial SerialBT;
#define BT_PAIRING_TIMEOUT 30000
uint32_t bt_pairing_started = 0;

#define BT_DEV_ADDR_LEN 6
#define BT_DEV_HASH_LEN 16
char bt_da[BT_DEV_ADDR_LEN];
char bt_dh[BT_DEV_HASH_LEN];
char bt_devname[11];

bool bt_ready = false;
bool bt_enabled = true;
bool bt_allow_pairing = false;

#if MCU_VARIANT == MCU_ESP32

  void bt_confirm_pairing(uint32_t numVal) {
    if (bt_allow_pairing) {
      bt_state = BT_STATE_ON;
      SerialBT.confirmReply(true);
    } else {
      bt_state = BT_STATE_ON;
      SerialBT.confirmReply(false);
    }
  }

  void bt_pairing_complete(boolean success) {
    if (success) {
      Serial.println("Paired");
    } else {
      Serial.println("Not Paired");
    }
  }

  void bt_enable_pairing() {
    bt_allow_pairing = true;
    bt_pairing_started = millis();
    bt_state = BT_STATE_PAIRING;
  }

  void bt_disable_pairing() {
    bt_allow_pairing = false;
    bt_state = BT_STATE_ON;
  }

  void bt_disable() {
    if (bt_state != BT_STATE_OFF) {
      SerialBT.end();
      bt_state = BT_STATE_OFF;
    }
  }

  void bt_enable() {
    if (bt_state == BT_STATE_OFF) {
      SerialBT.begin(bt_devname);
      bt_state = BT_STATE_ON;
     }
  }

  bool bt_setup_hw() {
    if (!bt_ready) {
      if (btStart()) {
        if (esp_bluedroid_init() == ESP_OK) {
          if (esp_bluedroid_enable() == ESP_OK) {
            const uint8_t* bda_ptr = esp_bt_dev_get_address();
            char *data = (char*)malloc(BT_DEV_ADDR_LEN);
            for (int i = 0; i < BT_DEV_ADDR_LEN; i++) {
                data[i] = bda_ptr[i];
            }
            unsigned char *hash = MD5::make_hash(data, BT_DEV_ADDR_LEN);
            memcpy(bt_dh, hash, BT_DEV_HASH_LEN);
            sprintf(bt_devname, "RNode %02X%02X", bt_dh[14], bt_dh[15]);
            free(data);

            SerialBT.enableSSP();
            SerialBT.onConfirmRequest(bt_confirm_pairing);
            SerialBT.onAuthComplete(bt_pairing_complete);
            
            bt_ready = true;
            return true;

          } else { return false; }
        } else { return false; }
      } else { return false; }
    }
  }

  bool bt_init() {
      bt_state = BT_STATE_OFF;
      if (bt_enabled) {
        if (bt_setup_hw()) {
          // TODO: Read from conf
          bt_enable();
          return true;
        } else {
          return false;
        }
      } else {
        return false;
      }
  }

  void update_bt() {
    if (millis()-bt_pairing_started >= BT_PAIRING_TIMEOUT) {
      bt_disable_pairing();
    }
  }
  
#endif
