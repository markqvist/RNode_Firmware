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

#include "BluetoothSerial.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"

BluetoothSerial SerialBT;
#define BT_PAIRING_TIMEOUT 35000
uint32_t bt_pairing_started = 0;

#define BT_DEV_ADDR_LEN 6
#define BT_DEV_HASH_LEN 16
uint8_t dev_bt_mac[BT_DEV_ADDR_LEN];
char bt_da[BT_DEV_ADDR_LEN];
char bt_dh[BT_DEV_HASH_LEN];
char bt_devname[11];

#if MCU_VARIANT == MCU_ESP32

  void bt_confirm_pairing(uint32_t numVal) {
    bt_ssp_pin = numVal;
    kiss_indicate_btpin();
    if (bt_allow_pairing) {
      SerialBT.confirmReply(true);
    } else {
      SerialBT.confirmReply(false);
    }
  }

  void bt_stop() {
    if (bt_state != BT_STATE_OFF) {
      SerialBT.end();
      bt_allow_pairing = false;
      bt_state = BT_STATE_OFF;
    }
  }

  void bt_start() {
    if (bt_state == BT_STATE_OFF) {
      SerialBT.begin(bt_devname);
      bt_state = BT_STATE_ON;
     }
  }

  void bt_enable_pairing() {
    if (bt_state == BT_STATE_OFF) bt_start();
    bt_allow_pairing = true;
    bt_pairing_started = millis();
    bt_state = BT_STATE_PAIRING;
  }

  void bt_disable_pairing() {
    bt_allow_pairing = false;
    bt_ssp_pin = 0;
    bt_state = BT_STATE_ON;
  }

  void bt_pairing_complete(boolean success) {
    if (success) {
      bt_disable_pairing();
    } else {
      bt_ssp_pin = 0;
    }
  }

  void bt_connection_callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param){
    if(event == ESP_SPP_SRV_OPEN_EVT) {
      bt_state = BT_STATE_CONNECTED;
      cable_state = CABLE_STATE_DISCONNECTED;
    }
     
    if(event == ESP_SPP_CLOSE_EVT ){
      bt_state = BT_STATE_ON;
    }
  }

  bool bt_setup_hw() {
    if (!bt_ready) {
      if (EEPROM.read(eeprom_addr(ADDR_CONF_BT)) == BT_ENABLE_BYTE) {
        bt_enabled = true;
      } else {
        bt_enabled = false;
      }
      if (btStart()) {
        if (esp_bluedroid_init() == ESP_OK) {
          if (esp_bluedroid_enable() == ESP_OK) {
            const uint8_t* bda_ptr = esp_bt_dev_get_address();
            char *data = (char*)malloc(BT_DEV_ADDR_LEN+1);
            for (int i = 0; i < BT_DEV_ADDR_LEN; i++) {
                data[i] = bda_ptr[i];
            }
            data[BT_DEV_ADDR_LEN] = EEPROM.read(eeprom_addr(ADDR_SIGNATURE));
            unsigned char *hash = MD5::make_hash(data, BT_DEV_ADDR_LEN);
            memcpy(bt_dh, hash, BT_DEV_HASH_LEN);
            sprintf(bt_devname, "RNode %02X%02X", bt_dh[14], bt_dh[15]);
            free(data);

            SerialBT.enableSSP();
            SerialBT.onConfirmRequest(bt_confirm_pairing);
            SerialBT.onAuthComplete(bt_pairing_complete);
            SerialBT.register_callback(bt_connection_callback);
            
            bt_ready = true;
            return true;

          } else { return false; }
        } else { return false; }
      } else { return false; }
    } else { return false; }
  }

  bool bt_init() {
      bt_state = BT_STATE_OFF;
      if (bt_setup_hw()) {
        if (bt_enabled && !console_active) bt_start();
        return true;
      } else {
        return false;
      }
  }

  void update_bt() {
    if (bt_allow_pairing && millis()-bt_pairing_started >= BT_PAIRING_TIMEOUT) {
      bt_disable_pairing();
    }
  }

#endif
