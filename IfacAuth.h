// Copyright (C) 2026, IFAC authentication contributed by GlassOnTin

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

#ifndef IFAC_AUTH_H
#define IFAC_AUTH_H

#if HAS_GPS == true

#include "sodium/crypto_sign_ed25519.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "mbedtls/sha256.h"

// NVS namespace for IFAC key storage
#define IFAC_NVS_NAMESPACE "ifac"
#define IFAC_NVS_KEY       "ifac_key"

#define IFAC_SIZE 8

// IFAC state
bool     ifac_configured = false;
uint8_t  ifac_key[64];
uint8_t  ifac_ed25519_pk[32];
uint8_t  ifac_ed25519_sk[64];  // libsodium format: seed(32) + pk(32)

// ---- NVS Storage ----

static bool ifac_nvs_load() {
    nvs_handle_t handle;
    if (nvs_open(IFAC_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return false;

    size_t key_len = 64;
    bool ok = (nvs_get_blob(handle, IFAC_NVS_KEY, ifac_key, &key_len) == ESP_OK)
           && (key_len == 64);

    nvs_close(handle);
    return ok;
}

static bool ifac_nvs_save() {
    nvs_handle_t handle;
    if (nvs_open(IFAC_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) return false;

    bool ok = (nvs_set_blob(handle, IFAC_NVS_KEY, ifac_key, 64) == ESP_OK)
           && (nvs_commit(handle) == ESP_OK);

    nvs_close(handle);
    return ok;
}

// ---- Ed25519 Keypair Derivation ----
// The signing seed is ifac_key[32:64] (last 32 bytes).

static void ifac_derive_keypair() {
    crypto_sign_ed25519_seed_keypair(ifac_ed25519_pk, ifac_ed25519_sk,
                                      ifac_key + 32);
}

// ---- Variable-Length HKDF-SHA256 ----
// Matches RNS Cryptography.hkdf() with context=b"".
// Uses hmac_sha256() from BeaconCrypto.h (must be included first).
//
// Extract: PRK = HMAC-SHA256(salt, ikm)
// Expand:  T(i) = HMAC-SHA256(PRK, T(i-1) || counter_byte)
//          counter_byte = (i+1) % 256, starting from i=0

static int rns_hkdf_var(const uint8_t *ikm, size_t ikm_len,
                         const uint8_t *salt, size_t salt_len,
                         uint8_t *output, size_t output_len) {
    uint8_t prk[32];
    int ret = hmac_sha256(salt, salt_len, ikm, ikm_len, prk);
    if (ret != 0) return ret;

    uint8_t prev_block[32];
    size_t prev_len = 0;
    size_t written = 0;
    uint8_t expand_buf[32 + 1];  // max: prev_block(32) + counter(1)
    int block_idx = 0;

    while (written < output_len) {
        // Build input: T(i-1) || counter
        if (prev_len > 0) {
            memcpy(expand_buf, prev_block, prev_len);
        }
        expand_buf[prev_len] = (uint8_t)((block_idx + 1) % 256);

        uint8_t block[32];
        ret = hmac_sha256(prk, 32, expand_buf, prev_len + 1, block);
        if (ret != 0) return ret;

        size_t to_copy = output_len - written;
        if (to_copy > 32) to_copy = 32;
        memcpy(output + written, block, to_copy);
        written += to_copy;

        memcpy(prev_block, block, 32);
        prev_len = 32;
        block_idx++;
    }

    return 0;
}

// ---- Reticulum IFAC_SALT (from Reticulum.py IFAC_SALT constant) ----
static const uint8_t RNS_IFAC_SALT[32] = {
    0xad, 0xf5, 0x4d, 0x88, 0x2c, 0x9a, 0x9b, 0x80, 0x77, 0x1e, 0xb4, 0x99, 0x5d, 0x70, 0x2d, 0x4a,
    0x3e, 0x73, 0x33, 0x91, 0xb2, 0xa0, 0xf5, 0x3f, 0x41, 0x6d, 0x9f, 0x90, 0x7e, 0x55, 0xcf, 0xf8,
};

// ---- Self-provision IFAC key from network_name + passphrase ----
// Replicates Reticulum.py:821-826 IFAC key derivation:
//   ifac_origin = SHA256(network_name) || SHA256(passphrase)
//   ifac_origin_hash = SHA256(ifac_origin)
//   ifac_key = HKDF(ikm=ifac_origin_hash, salt=IFAC_SALT, length=64)
static bool ifac_self_provision(const char *network_name, const char *passphrase) {
    uint8_t name_hash[32], key_hash[32];
    mbedtls_sha256((const uint8_t *)network_name, strlen(network_name), name_hash, 0);
    mbedtls_sha256((const uint8_t *)passphrase, strlen(passphrase), key_hash, 0);

    uint8_t ifac_origin[64];
    memcpy(ifac_origin, name_hash, 32);
    memcpy(ifac_origin + 32, key_hash, 32);

    uint8_t origin_hash[32];
    mbedtls_sha256(ifac_origin, 64, origin_hash, 0);

    int ret = rns_hkdf_var(origin_hash, 32, RNS_IFAC_SALT, 32, ifac_key, 64);
    if (ret != 0) return false;

    ifac_nvs_save();
    ifac_derive_keypair();
    ifac_configured = true;
    return true;
}

// ---- IFAC Initialization ----
// Call from setup() after lxmf_init_identity().
// Loads from NVS if available, otherwise self-provisions from beacon config.

static void ifac_init() {
    if (ifac_nvs_load()) {
        ifac_derive_keypair();
        ifac_configured = true;
    }
    // If no key loaded, caller should call ifac_self_provision()
    // with network_name/passphrase from beacon config.
}

// ---- Apply IFAC to Outgoing Packet ----
// Modifies pkt in-place. Returns new size (size + IFAC_SIZE) on success,
// or original size if IFAC is not configured.
//
// Algorithm (from RNS Transport.transmit):
//   1. sig = Ed25519Sign(pkt, sk)           → 64 bytes
//   2. ifac = sig[56:64]                    → last 8 bytes
//   3. mask = HKDF(ikm=ifac, salt=ifac_key, len=size+8)
//   4. Assemble: new_header(2) + ifac(8) + payload(size-2)
//   5. Set IFAC flag: byte[0] |= 0x80
//   6. XOR mask:
//      - byte 0: masked, but preserve 0x80 flag
//      - byte 1: masked
//      - bytes 2..9: NOT masked (IFAC itself)
//      - bytes 10+: masked

static uint16_t ifac_apply(uint8_t *pkt, uint16_t size) {
    if (!ifac_configured || size < 2) return size;

    uint16_t new_size = size + IFAC_SIZE;

    // 1. Sign the original packet
    uint8_t signature[64];
    unsigned long long sig_len_unused;
    crypto_sign_ed25519_detached(signature, &sig_len_unused,
                                  pkt, size, ifac_ed25519_sk);

    // 2. Extract IFAC: last 8 bytes of signature
    uint8_t ifac[IFAC_SIZE];
    memcpy(ifac, signature + 64 - IFAC_SIZE, IFAC_SIZE);

    // 3. Generate mask
    uint8_t mask[MTU + IFAC_SIZE];
    rns_hkdf_var(ifac, IFAC_SIZE, ifac_key, 64, mask, new_size);

    // 4. Shift payload to make room for IFAC after header
    //    pkt layout before: [hdr0][hdr1][payload...]
    //    pkt layout after:  [hdr0|0x80][hdr1][ifac:8][payload...]
    memmove(pkt + 2 + IFAC_SIZE, pkt + 2, size - 2);

    // 5. Insert IFAC
    memcpy(pkt + 2, ifac, IFAC_SIZE);

    // 6. Set IFAC flag
    pkt[0] |= 0x80;

    // 7. Apply mask
    //    byte 0: XOR then force 0x80
    pkt[0] = (pkt[0] ^ mask[0]) | 0x80;
    //    byte 1: XOR
    pkt[1] ^= mask[1];
    //    bytes 2..9 (IFAC): NOT masked
    //    bytes 10+: XOR
    for (uint16_t i = IFAC_SIZE + 2; i < new_size; i++) {
        pkt[i] ^= mask[i];
    }

    return new_size;
}

#endif // HAS_GPS
#endif // IFAC_AUTH_H
