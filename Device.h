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

#include <Ed25519.h>

#if MCU_VARIANT == MCU_ESP32
#include "mbedtls/md.h"
#include "esp_ota_ops.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#endif


// Forward declaration from Utilities.h
void eeprom_update(int mapped_addr, uint8_t byte);
uint8_t eeprom_read(uint32_t addr);
void hard_reset(void);

const uint8_t dev_keys [] PROGMEM = {
   0x0f, 0x15, 0x86, 0x74, 0xa0, 0x7d, 0xf2, 0xde, 0x32, 0x11, 0x29, 0xc1, 0x0d, 0xda, 0xcc, 0xc3,
   0xe1, 0x9b, 0xac, 0xf2, 0x27, 0x06, 0xee, 0x89, 0x1f, 0x7a, 0xfc, 0xc3, 0x6a, 0xf5, 0x38, 0x08
};

#define DEV_SIG_LEN 64
uint8_t dev_sig[DEV_SIG_LEN];

#define DEV_KEY_LEN 32
uint8_t dev_k_prv[DEV_KEY_LEN];
uint8_t dev_k_pub[DEV_KEY_LEN];

#define DEV_HASH_LEN 32
uint8_t dev_hash[DEV_HASH_LEN];
uint8_t dev_partition_table_hash[DEV_HASH_LEN];
uint8_t dev_bootloader_hash[DEV_HASH_LEN];
uint8_t dev_firmware_hash[DEV_HASH_LEN];
uint8_t dev_firmware_hash_target[DEV_HASH_LEN];

#define EEPROM_SIG_LEN 128
uint8_t dev_eeprom_signature[EEPROM_SIG_LEN];

bool dev_signature_validated = false;
bool fw_signature_validated = true;

#define DEV_SIG_OFFSET EEPROM_SIZE-EEPROM_RESERVED-DEV_SIG_LEN
#define dev_sig_addr(a) (a+DEV_SIG_OFFSET)

#define DEV_FWHASH_OFFSET EEPROM_SIZE-EEPROM_RESERVED-DEV_SIG_LEN-DEV_HASH_LEN
#define dev_fwhash_addr(a) (a+DEV_FWHASH_OFFSET)

bool device_signatures_ok() {
  return dev_signature_validated && fw_signature_validated;
}

void device_validate_signature() {
  int n_keys = sizeof(dev_keys)/DEV_KEY_LEN;
  bool valid_signature_found = false;
  for (int i = 0; i < n_keys; i++) {
    memcpy(dev_k_pub, dev_keys+DEV_KEY_LEN*i, DEV_KEY_LEN);
    if (Ed25519::verify(dev_sig, dev_k_pub, dev_hash, DEV_HASH_LEN)) {
        valid_signature_found = true;
    }
  }

  if (valid_signature_found) {
    dev_signature_validated = true;
  } else {
    dev_signature_validated = false;
  }
}

void device_save_signature() {
  device_validate_signature();
  if (dev_signature_validated) {
    for (uint8_t i = 0; i < DEV_SIG_LEN; i++) {
      eeprom_update(dev_sig_addr(i), dev_sig[i]);
    }
  }
}

void device_load_signature() {
  for (uint8_t i = 0; i < DEV_SIG_LEN; i++) {
    #if HAS_EEPROM
        dev_sig[i] = EEPROM.read(dev_sig_addr(i));
    #elif MCU_VARIANT == MCU_NRF52
        dev_sig[i] = eeprom_read(dev_sig_addr(i));
    #endif
  }
}

void device_load_firmware_hash() {
  for (uint8_t i = 0; i < DEV_HASH_LEN; i++) {
    #if HAS_EEPROM
        dev_firmware_hash_target[i] = EEPROM.read(dev_fwhash_addr(i));
    #elif MCU_VARIANT == MCU_NRF52
        dev_firmware_hash_target[i] = eeprom_read(dev_fwhash_addr(i));
    #endif
  }
}

void device_save_firmware_hash() {
  for (uint8_t i = 0; i < DEV_HASH_LEN; i++) {
    eeprom_update(dev_fwhash_addr(i), dev_firmware_hash_target[i]);
  }
  if (!fw_signature_validated) hard_reset();
}

#if MCU_VARIANT == MCU_ESP32
void device_validate_partitions() {
  device_load_firmware_hash();
  esp_partition_t partition;
  partition.address   = ESP_PARTITION_TABLE_OFFSET;
  partition.size      = ESP_PARTITION_TABLE_MAX_LEN;
  partition.type      = ESP_PARTITION_TYPE_DATA;
  esp_partition_get_sha256(&partition, dev_partition_table_hash);
  partition.address   = ESP_BOOTLOADER_OFFSET;
  partition.size      = ESP_PARTITION_TABLE_OFFSET;
  partition.type      = ESP_PARTITION_TYPE_APP;
  esp_partition_get_sha256(&partition, dev_bootloader_hash);
  esp_partition_get_sha256(esp_ota_get_running_partition(), dev_firmware_hash);
  for (uint8_t i = 0; i < DEV_HASH_LEN; i++) {
    if (dev_firmware_hash_target[i] != dev_firmware_hash[i]) {
      fw_signature_validated = false;
      break;
    }
  }
}
#endif

bool device_firmware_ok() {
  return fw_signature_validated;
}

#if MCU_VARIANT == MCU_ESP32
bool device_init() {
  if (bt_ready) {
    for (uint8_t i=0; i<EEPROM_SIG_LEN; i++){dev_eeprom_signature[i]=EEPROM.read(eeprom_addr(ADDR_SIGNATURE+i));}
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;     
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
    mbedtls_md_starts(&ctx);
    #if HAS_BLUETOOTH == true
      mbedtls_md_update(&ctx, dev_bt_mac, BT_DEV_ADDR_LEN);
    #endif
    mbedtls_md_update(&ctx, dev_eeprom_signature, EEPROM_SIG_LEN);
    mbedtls_md_finish(&ctx, dev_hash);
    mbedtls_md_free(&ctx);
    device_load_signature();
    device_validate_signature();
    device_validate_partitions();
    device_init_done = true;
    return device_init_done && fw_signature_validated;
  } else {
    return false;
  }
}
#endif
