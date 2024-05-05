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

#if MCU_VARIANT == MCU_ESP32
  

#elif MCU_VARIANT == MCU_NRF52
#endif

#if MCU_VARIANT == MCU_ESP32
  #if HAS_BLUETOOTH == true
    #include "BluetoothSerial.h"
    #include "esp_bt_main.h"
    #include "esp_bt_device.h"
    BluetoothSerial SerialBT;
  #elif HAS_BLE == true
    #include "esp_bt_main.h"
    #include "esp_bt_device.h"
    // TODO: Remove
    #define SerialBT Serial
  #endif

#elif MCU_VARIANT == MCU_NRF52
  #include <bluefruit.h>
  #include <math.h>
  BLEUart SerialBT;
  BLEDis  bledis;
  BLEBas  blebas;
#endif

#define BT_PAIRING_TIMEOUT 35000
uint32_t bt_pairing_started = 0;

#define BT_DEV_ADDR_LEN 6
#define BT_DEV_HASH_LEN 16
uint8_t dev_bt_mac[BT_DEV_ADDR_LEN];
char bt_da[BT_DEV_ADDR_LEN];
char bt_dh[BT_DEV_HASH_LEN];
char bt_devname[11];

#if MCU_VARIANT == MCU_ESP32
  #if HAS_BLUETOOTH == true

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

  #elif HAS_BLE == true
    void bt_stop() {
      if (bt_state != BT_STATE_OFF) {
        bt_allow_pairing = false;
        bt_state = BT_STATE_OFF;
      }
    }

    void bt_disable_pairing() {
      bt_allow_pairing = false;
      bt_ssp_pin = 0;
      bt_state = BT_STATE_ON;
    }

    void bt_connect_callback(uint16_t conn_handle) {
      bt_state = BT_STATE_CONNECTED;
      cable_state = CABLE_STATE_DISCONNECTED;
    }

    void bt_disconnect_callback(uint16_t conn_handle, uint8_t reason) {
      bt_state = BT_STATE_ON;
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

              // TODO: Implement GAP & GATT for RNode comms over BLE
              
              bt_ready = true;
              return true;

            } else { return false; }
          } else { return false; }
        } else { return false; }
      } else { return false; }
    }

    void bt_start() {
      if (bt_state == BT_STATE_OFF) {
        bt_state = BT_STATE_ON;
        // TODO: Implement
      }
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

    void bt_enable_pairing() {
      if (bt_state == BT_STATE_OFF) bt_start();
      bt_allow_pairing = true;
      bt_pairing_started = millis();
      bt_state = BT_STATE_PAIRING;
    }

    void update_bt() {
      if (bt_allow_pairing && millis()-bt_pairing_started >= BT_PAIRING_TIMEOUT) {
        bt_disable_pairing();
      }
    }
  #endif

#elif MCU_VARIANT == MCU_NRF52
uint8_t eeprom_read(uint32_t mapped_addr);

void bt_stop() {
  if (bt_state != BT_STATE_OFF) {
    bt_allow_pairing = false;
    bt_state = BT_STATE_OFF;
  }
}

void bt_disable_pairing() {
  bt_allow_pairing = false;
  bt_ssp_pin = 0;
  bt_state = BT_STATE_ON;
}

void bt_pairing_complete(uint16_t conn_handle, uint8_t auth_status) {
    if (auth_status == BLE_GAP_SEC_STATUS_SUCCESS) {
      bt_disable_pairing();
    } else {
      bt_ssp_pin = 0;
    }
}

bool bt_passkey_callback(uint16_t conn_handle, uint8_t const passkey[6], bool match_request) {
    for (int i = 0; i < 6; i++) {
        // multiply by tens however many times needed to make numbers appear in order
        bt_ssp_pin += ((int)passkey[i] - 48) * pow(10, 5-i);
    }
    kiss_indicate_btpin();
    if (match_request) {
        if (bt_allow_pairing) {
            return true;
        }
    }
    return false;
}

void bt_connect_callback(uint16_t conn_handle) {
    bt_state = BT_STATE_CONNECTED;
    cable_state = CABLE_STATE_DISCONNECTED;
}

void bt_disconnect_callback(uint16_t conn_handle, uint8_t reason) {
    bt_state = BT_STATE_ON;
}

bool bt_setup_hw() {
  if (!bt_ready) {
    #if HAS_EEPROM 
        if (EEPROM.read(eeprom_addr(ADDR_CONF_BT)) == BT_ENABLE_BYTE) {
    #else
        if (eeprom_read(eeprom_addr(ADDR_CONF_BT)) == BT_ENABLE_BYTE) {
    #endif
      bt_enabled = true;
    } else {
      bt_enabled = false;
    }
    Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
    Bluefruit.autoConnLed(false);
    if (Bluefruit.begin()) {
      Bluefruit.setTxPower(4);    // Check bluefruit.h for supported values
      Bluefruit.Security.setIOCaps(true, true, false);
      Bluefruit.Security.setPairPasskeyCallback(bt_passkey_callback);
      Bluefruit.Periph.setConnectCallback(bt_connect_callback);
      Bluefruit.Periph.setDisconnectCallback(bt_disconnect_callback);
      Bluefruit.Security.setPairCompleteCallback(bt_pairing_complete);
      const ble_gap_addr_t gap_addr = Bluefruit.getAddr();
      char *data = (char*)malloc(BT_DEV_ADDR_LEN+1);
      for (int i = 0; i < BT_DEV_ADDR_LEN; i++) {
          data[i] = gap_addr.addr[i];
      }
      #if HAS_EEPROM 
          data[BT_DEV_ADDR_LEN] = EEPROM.read(eeprom_addr(ADDR_SIGNATURE));
      #else
          data[BT_DEV_ADDR_LEN] = eeprom_read(eeprom_addr(ADDR_SIGNATURE));
      #endif
      unsigned char *hash = MD5::make_hash(data, BT_DEV_ADDR_LEN);
      memcpy(bt_dh, hash, BT_DEV_HASH_LEN);
      sprintf(bt_devname, "RNode %02X%02X", bt_dh[14], bt_dh[15]);
      free(data);

      bt_ready = true;
      return true;

    } else { return false; }
  } else { return false; }
}

void bt_start() {
  if (bt_state == BT_STATE_OFF) {
    Bluefruit.setName(bt_devname);
    bledis.setManufacturer(BLE_MANUFACTURER);
    bledis.setModel(BLE_MODEL);
    // start device information service
    bledis.begin();

    SerialBT.begin();

    blebas.begin();

    // non-connectable advertising
    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addTxPower();

    // Include bleuart 128-bit uuid
    Bluefruit.Advertising.addService(SerialBT);

    // There is no room for Name in Advertising packet
    // Use Scan response for Name
    Bluefruit.ScanResponse.addName();

    Bluefruit.Advertising.start(0);

    bt_state = BT_STATE_ON;
   }
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

void bt_enable_pairing() {
  if (bt_state == BT_STATE_OFF) bt_start();
  bt_allow_pairing = true;
  bt_pairing_started = millis();
  bt_state = BT_STATE_PAIRING;
}

void update_bt() {
  if (bt_allow_pairing && millis()-bt_pairing_started >= BT_PAIRING_TIMEOUT) {
    bt_disable_pairing();
  }
}
#endif
