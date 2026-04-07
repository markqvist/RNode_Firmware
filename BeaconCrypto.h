// Copyright (C) 2026, GPS beacon encryption contributed by GlassOnTin

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

#ifndef BEACON_CRYPTO_H
#define BEACON_CRYPTO_H

#if HAS_GPS == true

#include "sodium/crypto_scalarmult_curve25519.h"
#include "mbedtls/aes.h"
#include "mbedtls/md.h"
#include "esp_random.h"

// State loaded from EEPROM on boot
bool beacon_crypto_configured = false;
uint8_t collector_pub_key[32];
uint8_t collector_identity_hash[16];
uint8_t collector_dest_hash[16];

// HMAC-SHA256 (single-shot)
static int hmac_sha256(const uint8_t *key, size_t key_len,
                       const uint8_t *data, size_t data_len,
                       uint8_t *output) {
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    return mbedtls_md_hmac(md_info, key, key_len, data, data_len, output);
}

// RFC 5869 HKDF-SHA256 with info=b"", output 64 bytes
//
// Extract: PRK = HMAC-SHA256(key=salt, data=ikm)
// Expand:  T1 = HMAC-SHA256(PRK, 0x01)            [1 byte input]
//          T2 = HMAC-SHA256(PRK, T1 || 0x02)       [33 bytes input]
//          output = T1 || T2
static int rns_hkdf(const uint8_t *ikm, size_t ikm_len,
                    const uint8_t *salt, size_t salt_len,
                    uint8_t *output_64) {
    uint8_t prk[32];
    int ret = hmac_sha256(salt, salt_len, ikm, ikm_len, prk);
    if (ret != 0) return ret;

    // T1 = HMAC-SHA256(PRK, 0x01)
    uint8_t expand_buf[33];
    expand_buf[0] = 0x01;
    ret = hmac_sha256(prk, 32, expand_buf, 1, output_64);
    if (ret != 0) return ret;

    // T2 = HMAC-SHA256(PRK, T1 || 0x02)
    memcpy(expand_buf, output_64, 32);
    expand_buf[32] = 0x02;
    ret = hmac_sha256(prk, 32, expand_buf, 33, output_64 + 32);
    return ret;
}

// PKCS7 pad to 16-byte blocks. Returns padded length, or 0 on error.
static size_t pkcs7_pad(const uint8_t *input, size_t input_len,
                        uint8_t *output, size_t output_size) {
    uint8_t pad_val = 16 - (input_len % 16);
    size_t padded_len = input_len + pad_val;
    if (padded_len > output_size) return 0;
    memcpy(output, input, input_len);
    memset(output + input_len, pad_val, pad_val);
    return padded_len;
}

// Encrypt beacon payload for RNS SINGLE destination.
//
// Output layout: [ephemeral_pub:32][IV:16][ciphertext:var][HMAC:32]
// Returns total output length, or -1 on error.
//
// Crypto pipeline:
//   1. Generate ephemeral X25519 keypair (libsodium)
//   2. ECDH shared secret with collector's public key
//   3. HKDF-SHA256 → signing_key(32) + encryption_key(32)
//   4. AES-256-CBC encrypt PKCS7-padded plaintext
//   5. HMAC-SHA256(signing_key, IV || ciphertext)
static int beacon_crypto_encrypt(const uint8_t *plaintext, size_t pt_len,
                                 const uint8_t *peer_pub,
                                 const uint8_t *identity_hash,
                                 uint8_t *output) {
    // 1. Generate ephemeral X25519 keypair
    uint8_t eph_priv[32];
    esp_fill_random(eph_priv, 32);
    // Clamp private key per RFC 7748
    eph_priv[0]  &= 248;
    eph_priv[31] &= 127;
    eph_priv[31] |= 64;

    // Compute ephemeral public key and write to output
    if (crypto_scalarmult_curve25519_base(output, eph_priv) != 0) return -1;

    // 2. ECDH shared secret
    uint8_t ss_bytes[32];
    if (crypto_scalarmult_curve25519(ss_bytes, eph_priv, peer_pub) != 0) return -1;

    // 3. HKDF-SHA256: derive signing_key(32) + encryption_key(32)
    uint8_t derived[64];
    int ret = rns_hkdf(ss_bytes, 32, identity_hash, 16, derived);
    if (ret != 0) return -1;

    uint8_t *signing_key = derived;         // bytes 0-31
    uint8_t *encryption_key = derived + 32; // bytes 32-63

    // 4. Random IV
    uint8_t *iv_pos = output + 32;  // after ephemeral pubkey
    esp_fill_random(iv_pos, 16);
    uint8_t iv_copy[16];
    memcpy(iv_copy, iv_pos, 16);  // AES-CBC modifies IV in-place

    // 5. PKCS7 pad
    uint8_t padded[512];
    size_t padded_len = pkcs7_pad(plaintext, pt_len, padded, sizeof(padded));
    if (padded_len == 0) return -1;

    // 6. AES-256-CBC encrypt
    uint8_t *ct_pos = output + 32 + 16;  // after ephemeral pubkey + IV
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    ret = mbedtls_aes_setkey_enc(&aes, encryption_key, 256);
    if (ret != 0) { mbedtls_aes_free(&aes); return -1; }
    ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, padded_len,
                                 iv_copy, padded, ct_pos);
    mbedtls_aes_free(&aes);
    if (ret != 0) return -1;

    // 7. HMAC-SHA256(signing_key, IV || ciphertext)
    uint8_t *hmac_pos = ct_pos + padded_len;
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_context_t md_ctx;
    mbedtls_md_init(&md_ctx);
    ret = mbedtls_md_setup(&md_ctx, md_info, 1);  // 1 = use HMAC
    if (ret != 0) { mbedtls_md_free(&md_ctx); return -1; }
    ret = mbedtls_md_hmac_starts(&md_ctx, signing_key, 32);
    if (ret != 0) { mbedtls_md_free(&md_ctx); return -1; }
    ret = mbedtls_md_hmac_update(&md_ctx, iv_pos, 16);
    if (ret != 0) { mbedtls_md_free(&md_ctx); return -1; }
    ret = mbedtls_md_hmac_update(&md_ctx, ct_pos, padded_len);
    if (ret != 0) { mbedtls_md_free(&md_ctx); return -1; }
    ret = mbedtls_md_hmac_finish(&md_ctx, hmac_pos);
    mbedtls_md_free(&md_ctx);
    if (ret != 0) return -1;

    // Total: ephemeral_pub(32) + IV(16) + ciphertext(padded_len) + HMAC(32)
    return 32 + 16 + (int)padded_len + 32;
}

// Decrypt incoming RNS Token-format payload.
//
// Input layout: [ephemeral_pub:32][IV:16][ciphertext:var][HMAC:32]
// Returns plaintext length, or -1 on error (HMAC fail, decrypt fail, etc.)
//
// Crypto pipeline (reverse of beacon_crypto_encrypt):
//   1. Extract ephemeral public key
//   2. ECDH shared secret with OUR private key
//   3. HKDF-SHA256 → signing_key(32) + encryption_key(32)
//   4. Verify HMAC-SHA256(signing_key, IV || ciphertext)
//   5. AES-256-CBC decrypt
//   6. PKCS7 unpad
static int beacon_crypto_decrypt(const uint8_t *input, size_t input_len,
                                 const uint8_t *our_x25519_sk,
                                 const uint8_t *peer_identity_hash,
                                 uint8_t *output, size_t output_cap) {
    // Minimum: ephemeral(32) + IV(16) + 16-byte block + HMAC(32) = 96
    if (input_len < 96) return -1;

    const uint8_t *eph_pub = input;
    const uint8_t *iv_pos  = input + 32;
    size_t ct_len = input_len - 32 - 16 - 32;  // remove eph + IV + HMAC
    const uint8_t *ct_pos  = input + 32 + 16;
    const uint8_t *hmac_in = input + input_len - 32;

    if (ct_len == 0 || ct_len % 16 != 0) return -1;
    if (ct_len > output_cap) return -1;

    // 1. ECDH shared secret
    uint8_t ss_bytes[32];
    if (crypto_scalarmult_curve25519(ss_bytes, our_x25519_sk, eph_pub) != 0) return -1;

    // 2. HKDF-SHA256: derive signing_key(32) + encryption_key(32)
    uint8_t derived[64];
    int ret = rns_hkdf(ss_bytes, 32, peer_identity_hash, 16, derived);
    if (ret != 0) return -1;

    uint8_t *signing_key    = derived;
    uint8_t *encryption_key = derived + 32;

    // 3. Verify HMAC-SHA256(signing_key, IV || ciphertext)
    uint8_t computed_hmac[32];
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_context_t md_ctx;
    mbedtls_md_init(&md_ctx);
    ret = mbedtls_md_setup(&md_ctx, md_info, 1);
    if (ret == 0) ret = mbedtls_md_hmac_starts(&md_ctx, signing_key, 32);
    if (ret == 0) ret = mbedtls_md_hmac_update(&md_ctx, iv_pos, 16);
    if (ret == 0) ret = mbedtls_md_hmac_update(&md_ctx, ct_pos, ct_len);
    if (ret == 0) ret = mbedtls_md_hmac_finish(&md_ctx, computed_hmac);
    mbedtls_md_free(&md_ctx);
    if (ret != 0) return -1;

    if (memcmp(computed_hmac, hmac_in, 32) != 0) return -2;  // HMAC mismatch

    // 4. AES-256-CBC decrypt
    uint8_t iv_copy[16];
    memcpy(iv_copy, iv_pos, 16);
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    ret = mbedtls_aes_setkey_dec(&aes, encryption_key, 256);
    if (ret != 0) { mbedtls_aes_free(&aes); return -1; }
    ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, ct_len,
                                 iv_copy, ct_pos, output);
    mbedtls_aes_free(&aes);
    if (ret != 0) return -1;

    // 5. PKCS7 unpad
    uint8_t pad_val = output[ct_len - 1];
    if (pad_val == 0 || pad_val > 16) return -3;  // invalid padding
    for (size_t i = 0; i < pad_val; i++) {
        if (output[ct_len - 1 - i] != pad_val) return -3;
    }

    return (int)(ct_len - pad_val);
}

#endif
#endif
