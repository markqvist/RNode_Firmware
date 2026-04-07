// Copyright (C) 2026, LXMF beacon support contributed by GlassOnTin

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

#ifndef LXMF_BEACON_H
#define LXMF_BEACON_H

#if HAS_GPS == true

#include "sodium/crypto_sign_ed25519.h"
#include "sodium/crypto_scalarmult_curve25519.h"
#include "mbedtls/sha256.h"
#include "esp_random.h"

// NVS namespace for LXMF identity storage
#define LXMF_NVS_NAMESPACE "lxmf"
#define LXMF_NVS_KEY_SEED  "ed_seed"
#define LXMF_NVS_KEY_EDPUB "ed_pub"
#define LXMF_NVS_KEY_TXID  "transport_id"

// LXMF identity state
bool lxmf_identity_configured = false;
uint8_t lxmf_ed25519_seed[32];   // Ed25519 private seed
uint8_t lxmf_ed25519_pk[32];     // Ed25519 public key
uint8_t lxmf_ed25519_sk[64];     // Ed25519 expanded secret key (seed+pk)
uint8_t lxmf_x25519_pk[32];      // X25519 public key (derived from Ed25519)
uint8_t lxmf_x25519_sk[32];      // X25519 private key (derived from Ed25519)
uint8_t lxmf_identity_hash[16];  // SHA256(x25519_pk + ed25519_pk)[:16]
uint8_t lxmf_source_hash[16];    // SHA256(name_hash("lxmf","delivery") + identity_hash)[:16]

// Transport node identity for HEADER_2 routing
bool     transport_configured = false;
uint8_t  transport_id[16];       // Transport node's identity hash (16B)

// Announce timing
#define LXMF_ANNOUNCE_INTERVAL_MS 600000  // 10 minutes
uint32_t lxmf_last_announce = 0;

// Provisioning display feedback
uint32_t lxmf_provisioned_at = 0;  // millis() when last CMD_BCN_KEY received

// Forward declarations
void beacon_transmit(uint16_t size);
void lora_receive();

// ---- SHA-256 helpers ----

static void sha256_once(const uint8_t *data, size_t len, uint8_t *out32) {
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, data, len);
    mbedtls_sha256_finish(&ctx, out32);
    mbedtls_sha256_free(&ctx);
}

static void sha256_two(const uint8_t *a, size_t a_len,
                        const uint8_t *b, size_t b_len,
                        uint8_t *out32) {
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, a, a_len);
    mbedtls_sha256_update(&ctx, b, b_len);
    mbedtls_sha256_finish(&ctx, out32);
    mbedtls_sha256_free(&ctx);
}

// ---- RNS Identity Hash Computation ----
// identity_hash = SHA256(x25519_pub(32) + ed25519_pub(32))[:16]

static void compute_identity_hash(const uint8_t *x25519_pub, const uint8_t *ed25519_pub,
                                   uint8_t *out16) {
    uint8_t full[32];
    sha256_two(x25519_pub, 32, ed25519_pub, 32, full);
    memcpy(out16, full, 16);
}

// ---- RNS Destination Hash Computation ----
// dest_hash = SHA256(name_hash + identity_hash)[:16]
// where name_hash = SHA256(SHA256("lxmf") + SHA256("delivery"))[:10]

static void compute_name_hash(const char *app, const char *aspect, uint8_t *out10) {
    // RNS: name_hash = SHA256("app.aspect")[:10]
    char full_name[64];
    snprintf(full_name, sizeof(full_name), "%s.%s", app, aspect);
    uint8_t hash[32];
    sha256_once((const uint8_t*)full_name, strlen(full_name), hash);
    memcpy(out10, hash, 10);
}

static void compute_dest_hash(const uint8_t *name_hash10, const uint8_t *identity_hash16,
                               uint8_t *out16) {
    uint8_t full[32];
    sha256_two(name_hash10, 10, identity_hash16, 16, full);
    memcpy(out16, full, 16);
}

// ---- NVS Identity Storage ----

#include "nvs_flash.h"
#include "nvs.h"

static bool lxmf_nvs_load_identity() {
    nvs_handle_t handle;
    if (nvs_open(LXMF_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return false;

    size_t seed_len = 32, pub_len = 32;
    bool ok = (nvs_get_blob(handle, LXMF_NVS_KEY_SEED, lxmf_ed25519_seed, &seed_len) == ESP_OK)
           && (nvs_get_blob(handle, LXMF_NVS_KEY_EDPUB, lxmf_ed25519_pk, &pub_len) == ESP_OK)
           && (seed_len == 32) && (pub_len == 32);

    nvs_close(handle);
    return ok;
}

static bool lxmf_nvs_save_identity() {
    nvs_handle_t handle;
    if (nvs_open(LXMF_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) return false;

    bool ok = (nvs_set_blob(handle, LXMF_NVS_KEY_SEED, lxmf_ed25519_seed, 32) == ESP_OK)
           && (nvs_set_blob(handle, LXMF_NVS_KEY_EDPUB, lxmf_ed25519_pk, 32) == ESP_OK)
           && (nvs_commit(handle) == ESP_OK);

    nvs_close(handle);
    return ok;
}

static bool lxmf_nvs_load_transport_id() {
    nvs_handle_t handle;
    if (nvs_open(LXMF_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return false;
    size_t len = 16;
    bool ok = (nvs_get_blob(handle, LXMF_NVS_KEY_TXID, transport_id, &len) == ESP_OK) && (len == 16);
    nvs_close(handle);
    return ok;
}

static bool lxmf_nvs_save_transport_id() {
    nvs_handle_t handle;
    if (nvs_open(LXMF_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) return false;
    bool ok = (nvs_set_blob(handle, LXMF_NVS_KEY_TXID, transport_id, 16) == ESP_OK)
           && (nvs_commit(handle) == ESP_OK);
    nvs_close(handle);
    return ok;
}

// ---- Identity Initialization ----
// Call from setup(). Loads or generates Ed25519 keypair, derives X25519 keys,
// computes identity_hash and source_hash (LXMF delivery destination).

static void lxmf_init_identity() {
    bool loaded = lxmf_nvs_load_identity();

    if (!loaded) {
        // Generate new Ed25519 keypair
        crypto_sign_ed25519_keypair(lxmf_ed25519_pk, lxmf_ed25519_sk);
        // Extract seed from sk (first 32 bytes of libsodium's 64-byte sk)
        memcpy(lxmf_ed25519_seed, lxmf_ed25519_sk, 32);
        lxmf_nvs_save_identity();
    } else {
        // Validate NVS: regenerate pk from seed and compare
        uint8_t verify_pk[32], verify_sk[64];
        crypto_sign_ed25519_seed_keypair(verify_pk, verify_sk, lxmf_ed25519_seed);
        if (memcmp(verify_pk, lxmf_ed25519_pk, 32) != 0) {
            // NVS corruption — regenerate and save
            memcpy(lxmf_ed25519_pk, verify_pk, 32);
            memcpy(lxmf_ed25519_sk, verify_sk, 64);
            lxmf_nvs_save_identity();
        } else {
            // Reconstruct expanded secret key from seed
            // libsodium ed25519 sk = seed(32) + pk(32)
            memcpy(lxmf_ed25519_sk, lxmf_ed25519_seed, 32);
            memcpy(lxmf_ed25519_sk + 32, lxmf_ed25519_pk, 32);
        }
    }

    // Derive X25519 keys from Ed25519
    crypto_sign_ed25519_pk_to_curve25519(lxmf_x25519_pk, lxmf_ed25519_pk);
    crypto_sign_ed25519_sk_to_curve25519(lxmf_x25519_sk, lxmf_ed25519_sk);

    // Compute identity hash: SHA256(x25519_pub + ed25519_pub)[:16]
    compute_identity_hash(lxmf_x25519_pk, lxmf_ed25519_pk, lxmf_identity_hash);

    // Compute source hash (LXMF delivery destination hash)
    uint8_t name_hash[10];
    compute_name_hash("lxmf", "delivery", name_hash);
    compute_dest_hash(name_hash, lxmf_identity_hash, lxmf_source_hash);

    lxmf_identity_configured = true;

    // Load transport node identity (for HEADER_2 routing)
    transport_configured = lxmf_nvs_load_transport_id();
}

// ---- Minimal Msgpack Encoder ----
// Fixed-schema packer: no dynamic allocation, writes directly to output buffer.

struct MsgpackWriter {
    uint8_t *buf;
    size_t pos;
    size_t cap;

    bool ok() const { return pos <= cap; }

    void write_byte(uint8_t b) {
        if (pos < cap) buf[pos] = b;
        pos++;
    }

    void write_bytes(const uint8_t *data, size_t len) {
        for (size_t i = 0; i < len; i++) write_byte(data[i]);
    }

    // msgpack fixint (0-127)
    void pack_uint7(uint8_t v) { write_byte(v & 0x7f); }

    // msgpack uint8
    void pack_uint8(uint8_t v) { write_byte(0xcc); write_byte(v); }

    // msgpack uint16
    void pack_uint16(uint16_t v) {
        write_byte(0xcd);
        write_byte((v >> 8) & 0xff);
        write_byte(v & 0xff);
    }

    // msgpack uint32
    void pack_uint32(uint32_t v) {
        write_byte(0xce);
        write_byte((v >> 24) & 0xff);
        write_byte((v >> 16) & 0xff);
        write_byte((v >> 8) & 0xff);
        write_byte(v & 0xff);
    }

    // msgpack float64
    void pack_float64(double v) {
        write_byte(0xcb);
        union { double d; uint8_t b[8]; } u;
        u.d = v;
        // IEEE 754 big-endian
        for (int i = 7; i >= 0; i--) write_byte(u.b[i]);
    }

    // msgpack bin8 (up to 255 bytes)
    void pack_bin8(const uint8_t *data, uint8_t len) {
        write_byte(0xc4);
        write_byte(len);
        if (len > 0 && data != NULL) write_bytes(data, len);
    }

    // msgpack fixstr (up to 31 bytes)
    void pack_fixstr(const char *s, uint8_t len) {
        write_byte(0xa0 | (len & 0x1f));
        write_bytes((const uint8_t*)s, len);
    }

    // msgpack empty string
    void pack_empty_str() { write_byte(0xa0); }

    // msgpack fixarray header (up to 15 elements)
    void pack_fixarray(uint8_t n) { write_byte(0x90 | (n & 0x0f)); }

    // msgpack fixmap header (up to 15 entries)
    void pack_fixmap(uint8_t n) { write_byte(0x80 | (n & 0x0f)); }

    // msgpack false
    void pack_false() { write_byte(0xc2); }

    // msgpack nil
    void pack_nil() { write_byte(0xc0); }
};

// ---- Sideband Telemetry Packing ----
// Produces Sideband-compatible FIELD_TELEMETRY bytes (Telemeter.packed() format).
// Format: msgpack map { SID_TIME(0x01): unix_ts, SID_LOCATION(0x02): [...], SID_BATTERY(0x04): [...] }
//
// Size budget: LoRa max payload = 255 bytes.
// With HEADER_2(35) + IFAC(8) + crypto overhead(80) + PKCS7 padding,
// LXMF plaintext must be ≤ 127 bytes (pads to 128).
// Fixed: source_hash(16) + signature(64) + payload wrapper(~14) = 94.
// Max telemetry = 127 - 94 = 33 bytes.
//
// Sideband Location.unpack() builds a dict from ALL 7 array elements at once.
// Short arrays cause IndexError → unpack returns None → no location displayed.
// We send only 3 elements (lat, lon, alt) to fit the budget, and patch
// Sideband's Location.unpack() on the phone to handle short arrays.
//
// Actual: SID_TIME(6) + SID_LOCATION(22) + SID_BATTERY(5) + map hdr(1) = 34 max.
// Without battery: 1 + 6 + 22 = 29 bytes.

static size_t lxmf_pack_telemetry(uint8_t *out, size_t out_cap,
                                    double lat, double lon, double alt,
                                    double speed, double hdop,
                                    uint32_t timestamp, int bat_percent) {
    MsgpackWriter w = { out, 0, out_cap };

    bool has_bat = (bat_percent > 0);
    w.pack_fixmap(has_bat ? 3 : 2);  // SID_TIME + SID_LOCATION [+ SID_BATTERY]

    // SID_TIME = 0x01 → uint32 Unix timestamp (REQUIRED by Sideband)
    w.pack_uint7(0x01);
    w.pack_uint32(timestamp);

    // SID_LOCATION = 0x02 → fixarray[3] [lat, lon, alt]
    // (Sideband patched to handle short arrays; see patch_sideband_location.py)
    w.pack_uint7(0x02);
    w.pack_fixarray(3);

    // [0] lat: bin4 (struct.pack("!i", lat*1e6))
    int32_t lat_i = (int32_t)round(lat * 1e6);
    uint8_t lat_b[4] = {
        (uint8_t)((lat_i >> 24) & 0xff), (uint8_t)((lat_i >> 16) & 0xff),
        (uint8_t)((lat_i >> 8) & 0xff),  (uint8_t)(lat_i & 0xff)
    };
    w.pack_bin8(lat_b, 4);

    // [1] lon: bin4 (struct.pack("!i", lon*1e6))
    int32_t lon_i = (int32_t)round(lon * 1e6);
    uint8_t lon_b[4] = {
        (uint8_t)((lon_i >> 24) & 0xff), (uint8_t)((lon_i >> 16) & 0xff),
        (uint8_t)((lon_i >> 8) & 0xff),  (uint8_t)(lon_i & 0xff)
    };
    w.pack_bin8(lon_b, 4);

    // [2] alt: bin4 (struct.pack("!i", alt*1e2))
    int32_t alt_i = (int32_t)round(alt * 1e2);
    uint8_t alt_b[4] = {
        (uint8_t)((alt_i >> 24) & 0xff), (uint8_t)((alt_i >> 16) & 0xff),
        (uint8_t)((alt_i >> 8) & 0xff),  (uint8_t)(alt_i & 0xff)
    };
    w.pack_bin8(alt_b, 4);

    // SID_BATTERY = 0x04 → fixarray[3] [pct, charging, temperature]
    if (has_bat) {
        w.pack_uint7(0x04);
        w.pack_fixarray(3);
        w.pack_uint7((uint8_t)(bat_percent > 100 ? 100 : bat_percent));
        w.pack_false();  // not charging
        w.pack_nil();    // temperature unknown
    }

    if (!w.ok()) return 0;
    return w.pos;
}

// ---- LXMF Message Construction ----
// Builds the LXMF plaintext (before RNS encryption).
//
// LXMF packed format for OPPORTUNISTIC delivery:
//   source_hash(16) + signature(64) + msgpack_payload
//   (dest_hash is omitted; RNS header carries it)
//
// msgpack_payload = [timestamp_f64, title_str, content_str, fields_map]
// fields_map = { 0x02: telemetry_bytes }  (FIELD_TELEMETRY = 0x02 in LXMF)
//
// Signing (from LXMF LXMessage.pack()):
//   hashed_part = dest_hash + source_hash + msgpack_payload
//   message_hash = SHA256(hashed_part)
//   signed_part = hashed_part + message_hash
//   signature = Ed25519Sign(signed_part, ed25519_sk)

static int lxmf_build_message(uint8_t *out, size_t out_cap,
                                const uint8_t *dest_hash16,
                                double lat, double lon, double alt,
                                double speed, double hdop,
                                uint32_t timestamp, int bat_percent) {
    // Static buffers to avoid stack overflow (~1KB saved)
    // Safe because beacon functions are never called concurrently
    static uint8_t telemetry[128];
    static uint8_t payload[256];
    static uint8_t hashed_part[256 + 32];
    static uint8_t signed_part[256 + 32 + 32];

    // 1. Pack telemetry bytes
    size_t telem_len = lxmf_pack_telemetry(telemetry, sizeof(telemetry),
                                            lat, lon, alt, speed, hdop,
                                            timestamp, bat_percent);
    if (telem_len == 0) return -1;

    // 2. Build msgpack payload: [timestamp, nil, nil, {0x02: telemetry}]
    MsgpackWriter pw = { payload, 0, sizeof(payload) };

    pw.pack_fixarray(4);

    // timestamp as uint32 (saves 4 bytes vs float64)
    pw.pack_uint32(timestamp);

    // empty title and content as bin8(0) — must not be nil,
    // Sideband calls len(content) which fails on None
    pw.pack_bin8(NULL, 0);
    pw.pack_bin8(NULL, 0);

    // fields: {FIELD_TELEMETRY(0x02): telemetry_bytes}
    pw.pack_fixmap(1);
    pw.pack_uint7(0x02);
    pw.pack_bin8(telemetry, (uint8_t)telem_len);

    if (!pw.ok()) return -1;
    size_t payload_len = pw.pos;

    // 3. Compute signature
    //    hashed_part = dest_hash(16) + source_hash(16) + payload
    size_t hp_len = 16 + 16 + payload_len;
    if (hp_len > sizeof(hashed_part)) return -1;
    memcpy(hashed_part, dest_hash16, 16);
    memcpy(hashed_part + 16, lxmf_source_hash, 16);
    memcpy(hashed_part + 32, payload, payload_len);

    //    message_hash = SHA256(hashed_part)  (RNS.Identity.full_hash is single SHA256)
    uint8_t message_hash[32];
    sha256_once(hashed_part, hp_len, message_hash);

    //    signed_part = hashed_part + message_hash
    size_t sp_len = hp_len + 32;
    if (sp_len > sizeof(signed_part)) return -1;
    memcpy(signed_part, hashed_part, hp_len);
    memcpy(signed_part + hp_len, message_hash, 32);

    //    signature = Ed25519Sign(signed_part)
    uint8_t signature[64];
    unsigned long long sig_len_unused;
    crypto_sign_ed25519_detached(signature, &sig_len_unused,
                                  signed_part, sp_len, lxmf_ed25519_sk);

    // 4. Assemble LXMF wire format for OPPORTUNISTIC:
    //    source_hash(16) + signature(64) + payload
    size_t total = 16 + 64 + payload_len;
    if (total > out_cap) return -1;

    memcpy(out, lxmf_source_hash, 16);
    memcpy(out + 16, signature, 64);
    memcpy(out + 80, payload, payload_len);

    return (int)total;
}

// ---- Minimal Msgpack Reader ----
// Read-only cursor over msgpack data. No allocation.
struct MsgpackReader {
    const uint8_t *buf;
    size_t pos;
    size_t len;

    MsgpackReader(const uint8_t *data, size_t sz) : buf(data), pos(0), len(sz) {}
    bool has(size_t n) const { return pos + n <= len; }
    uint8_t peek() const { return has(1) ? buf[pos] : 0xFF; }
    uint8_t read_u8() { return has(1) ? buf[pos++] : 0; }

    // Read fixarray length (0x90-0x9F)
    int read_array_len() {
        uint8_t b = read_u8();
        if ((b & 0xF0) == 0x90) return b & 0x0F;
        if (b == 0xDC && has(2)) { uint16_t n = (buf[pos]<<8)|buf[pos+1]; pos+=2; return n; }
        return -1;
    }

    // Read uint (fixint, uint8, uint16, uint32)
    uint32_t read_uint() {
        uint8_t b = read_u8();
        if (b <= 0x7F) return b;
        if (b == 0xCC && has(1)) return buf[pos++];
        if (b == 0xCD && has(2)) { uint16_t v=(buf[pos]<<8)|buf[pos+1]; pos+=2; return v; }
        if (b == 0xCE && has(4)) { uint32_t v=(buf[pos]<<24)|(buf[pos+1]<<16)|(buf[pos+2]<<8)|buf[pos+3]; pos+=4; return v; }
        return 0;
    }

    // Read binary/string, returns pointer and sets length. Doesn't copy.
    const uint8_t *read_bin(size_t *out_len) {
        uint8_t b = read_u8();
        size_t n = 0;
        if ((b & 0xE0) == 0xA0) n = b & 0x1F;          // fixstr
        else if (b == 0xC4 && has(1)) n = buf[pos++];    // bin8
        else if (b == 0xC5 && has(2)) { n=(buf[pos]<<8)|buf[pos+1]; pos+=2; }  // bin16
        else if (b == 0xD9 && has(1)) n = buf[pos++];    // str8
        else { *out_len = 0; return NULL; }
        if (!has(n)) { *out_len = 0; return NULL; }
        const uint8_t *ptr = buf + pos;
        pos += n;
        *out_len = n;
        return ptr;
    }

    // Skip one msgpack value
    void skip() {
        uint8_t b = peek();
        if (b <= 0x7F || b >= 0xE0 || b == 0xC0 || b == 0xC2 || b == 0xC3) { pos++; return; }
        if ((b & 0xF0) == 0x90) { int n = read_array_len(); for (int i=0;i<n;i++) skip(); return; }
        if ((b & 0xF0) == 0x80) { pos++; int n = b & 0x0F; for (int i=0;i<n*2;i++) skip(); return; }
        size_t dummy; read_bin(&dummy);  // handles str/bin
    }

    // Read fixmap length
    int read_map_len() {
        uint8_t b = read_u8();
        if ((b & 0xF0) == 0x80) return b & 0x0F;
        if (b == 0xDE && has(2)) { uint16_t n=(buf[pos]<<8)|buf[pos+1]; pos+=2; return n; }
        return -1;
    }
};

// ---- LXMF Message RX Parser ----
// Parse decrypted LXMF plaintext: source_hash(16) + signature(64) + msgpack
// Returns true if parsed successfully. Fills title_out and content_out.
struct LxmfParsed {
    uint8_t  source_hash[16];
    uint32_t timestamp;
    char     title[32];
    char     content[64];
    bool     verified;  // signature verified against cached announce key
};

static bool lxmf_parse_received(const uint8_t *plaintext, size_t pt_len,
                                 LxmfParsed *out) {
    if (pt_len < 16 + 64 + 4) return false;  // minimum: hash + sig + tiny msgpack

    memcpy(out->source_hash, plaintext, 16);
    // signature at plaintext+16, 64 bytes (verified later with sender's pubkey)
    const uint8_t *msgpack_data = plaintext + 16 + 64;
    size_t msgpack_len = pt_len - 16 - 64;

    MsgpackReader r(msgpack_data, msgpack_len);

    // Expect fixarray(4): [timestamp, title, content, fields]
    int arr_len = r.read_array_len();
    if (arr_len < 3) return false;

    // [0] timestamp
    out->timestamp = r.read_uint();

    // [1] title
    size_t title_len = 0;
    const uint8_t *title_ptr = r.read_bin(&title_len);
    if (title_ptr && title_len > 0) {
        size_t n = title_len < sizeof(out->title)-1 ? title_len : sizeof(out->title)-1;
        memcpy(out->title, title_ptr, n);
        out->title[n] = '\0';
    } else {
        out->title[0] = '\0';
    }

    // [2] content
    size_t content_len = 0;
    const uint8_t *content_ptr = r.read_bin(&content_len);
    if (content_ptr && content_len > 0) {
        size_t n = content_len < sizeof(out->content)-1 ? content_len : sizeof(out->content)-1;
        memcpy(out->content, content_ptr, n);
        out->content[n] = '\0';
    } else {
        out->content[0] = '\0';
    }

    out->verified = false;  // caller must verify with sender's pubkey
    return true;
}

// Verify LXMF signature using sender's Ed25519 public key
static bool lxmf_verify_signature(const uint8_t *plaintext, size_t pt_len,
                                   const uint8_t *sender_ed25519_pub) {
    if (pt_len < 16 + 64 + 4) return false;
    const uint8_t *signature = plaintext + 16;
    const uint8_t *msgpack_data = plaintext + 16 + 64;
    size_t msgpack_len = pt_len - 16 - 64;

    // Reconstruct signed_part: dest_hash + source_hash + msgpack + SHA256(same)
    // dest_hash = our lxmf_source_hash, source_hash = plaintext[0:16]
    static uint8_t hashed_part[512];
    size_t hp_len = 16 + 16 + msgpack_len;
    if (hp_len > sizeof(hashed_part) - 32) return false;

    memcpy(hashed_part, lxmf_source_hash, 16);      // dest = us
    memcpy(hashed_part + 16, plaintext, 16);          // source
    memcpy(hashed_part + 32, msgpack_data, msgpack_len);

    uint8_t message_hash[32];
    sha256_once(hashed_part, hp_len, message_hash);

    static uint8_t signed_part[512 + 32];
    memcpy(signed_part, hashed_part, hp_len);
    memcpy(signed_part + hp_len, message_hash, 32);
    size_t sp_len = hp_len + 32;

    return crypto_sign_ed25519_verify_detached(signature, signed_part, sp_len,
                                                sender_ed25519_pub) == 0;
}

// ---- RNS Announce Packet Construction ----
// Builds a complete RNS announce packet in tbuf for transmission.
//
// RNS header: flags(1) + hops(1) + dest_hash(16) + context(1) = 19 bytes
// Announce payload:
//   public_key(64): x25519_pub + ed25519_pub
//   name_hash(10): computed for "lxmf.delivery"
//   random_hash(10): random bytes
//   signature(64): Ed25519Sign(dest_hash + public_key + name_hash + random_hash + app_data)
//   app_data: msgpack string with display name

static int lxmf_build_announce(uint8_t *out, size_t out_cap, const char *display_name) {
    // Announce FLAGS: header_type=0 (HEADER_1), propagation=0 (BROADCAST),
    //                 destination=0 (SINGLE), packet_type=1 (ANNOUNCE), transport=0
    // Bits: [header_type:2][propagation_type:2][destination_type:2][packet_type:2]
    // ANNOUNCE packet_type = 1 → 0x01 in low 2 bits
    // But RNS packs: ifac_flag(1) | header_type(1) | propagation_type(2) | destination_type(1) | packet_type(1) | transport_type(1) | context_flag(1)
    // Wait, let me use the correct bit layout from RNS:
    // header byte = (ifac_flag << 7) | (header_type << 6) | (propagation << 4) | (destination << 2) | (packet_type) | transport bit
    // For announce: ifac=0, header_type=0(HEADER_1), propagation=0(BROADCAST),
    //              destination=0(SINGLE), packet_type=1(ANNOUNCE), transport=0
    // = 0b00000010 = 0x02
    // But context_flag is separate: context byte for announce = 0x00 (CONTEXT_NONE)

    // Actually the RNS header is:
    // byte 0: [ifac_flag:1][header_type:1][propagation_type:2][destination_type:2][packet_type:2]
    // For HEADER_1 + BROADCAST + SINGLE + ANNOUNCE:
    // = 0b00_00_00_01 = 0x01
    out[0] = 0x01;  // FLAGS: HEADER_1, BROADCAST, SINGLE, ANNOUNCE
    out[1] = 0x00;  // HOPS

    // dest_hash for our LXMF delivery destination
    memcpy(&out[2], lxmf_source_hash, 16);

    out[18] = 0x00;  // CONTEXT_NONE

    size_t pos = 19;

    // public_key: x25519_pub(32) + ed25519_pub(32)
    memcpy(&out[pos], lxmf_x25519_pk, 32); pos += 32;
    memcpy(&out[pos], lxmf_ed25519_pk, 32); pos += 32;

    // name_hash: SHA256("lxmf.delivery")[:10]
    uint8_t name_hash[10];
    compute_name_hash("lxmf", "delivery", name_hash);
    memcpy(&out[pos], name_hash, 10); pos += 10;

    // random_hash: 5 random bytes + 5-byte big-endian Unix timestamp
    // RNS uses random_hash[5:10] as an "announce emitted" timebase
    // for ordering announces in the transport node's path table.
    uint8_t random_hash[10];
    esp_fill_random(random_hash, 5);
    uint32_t now_sec = 0;
    #if HAS_GPS == true
        // Use GPS time directly (always available when beaconing since gps_has_fix is checked)
        extern TinyGPSPlus gps_parser;
        if (gps_parser.date.isValid() && gps_parser.time.isValid() && gps_parser.date.year() >= 2024) {
            uint32_t days = 0;
            uint16_t yr = gps_parser.date.year();
            uint8_t mo = gps_parser.date.month();
            uint8_t dy = gps_parser.date.day();
            for (uint16_t y = 1970; y < yr; y++)
                days += (y % 4 == 0) ? 366 : 365;
            static const uint16_t mdays[] = {0,31,59,90,120,151,181,212,243,273,304,334};
            if (mo >= 1 && mo <= 12) {
                days += mdays[mo - 1];
                if (mo > 2 && (yr % 4 == 0)) days++;
            }
            days += dy - 1;
            now_sec = days * 86400UL + gps_parser.time.hour() * 3600UL
                    + gps_parser.time.minute() * 60UL + gps_parser.time.second();
        } else {
            now_sec = (uint32_t)(millis() / 1000);
        }
    #else
        now_sec = (uint32_t)(millis() / 1000);
    #endif
    // 5-byte big-endian timestamp (40-bit, fits Unix time for centuries)
    random_hash[5] = 0;  // high byte always 0 for current era
    random_hash[6] = (uint8_t)(now_sec >> 24);
    random_hash[7] = (uint8_t)(now_sec >> 16);
    random_hash[8] = (uint8_t)(now_sec >> 8);
    random_hash[9] = (uint8_t)(now_sec);
    memcpy(&out[pos], random_hash, 10); pos += 10;

    // app_data: raw UTF-8 display name (no msgpack wrapping —
    // Sideband's display_name_from_app_data() decodes as plain UTF-8)
    size_t name_len = strlen(display_name);
    uint8_t app_data[48];
    size_t app_data_len = name_len;
    memcpy(app_data, display_name, name_len);

    // signature: Ed25519Sign(dest_hash + public_key + name_hash + random_hash + app_data)
    // signed_data = dest_hash(16) + public_key(64) + name_hash(10) + random_hash(10) + app_data
    static uint8_t signed_data[256];
    size_t sd_len = 0;
    memcpy(&signed_data[sd_len], lxmf_source_hash, 16); sd_len += 16;
    memcpy(&signed_data[sd_len], lxmf_x25519_pk, 32);   sd_len += 32;
    memcpy(&signed_data[sd_len], lxmf_ed25519_pk, 32);   sd_len += 32;
    memcpy(&signed_data[sd_len], name_hash, 10);          sd_len += 10;
    memcpy(&signed_data[sd_len], random_hash, 10);        sd_len += 10;
    memcpy(&signed_data[sd_len], app_data, app_data_len); sd_len += app_data_len;

    uint8_t signature[64];
    unsigned long long sig_len_unused;
    crypto_sign_ed25519_detached(signature, &sig_len_unused,
                                  signed_data, sd_len, lxmf_ed25519_sk);

    memcpy(&out[pos], signature, 64); pos += 64;

    // app_data
    memcpy(&out[pos], app_data, app_data_len); pos += app_data_len;

    if (pos > out_cap) return -1;
    return (int)pos;
}

// ---- Public API ----

// Build and transmit an LXMF telemetry beacon.
// The LXMF message is encrypted as an RNS SINGLE packet to collector_dest_hash
// using the same ECDH encryption pipeline from BeaconCrypto.h.
static void lxmf_beacon_send(double lat, double lon, double alt,
                               double speed, double hdop,
                               uint32_t timestamp, int bat_percent) {
    if (!lxmf_identity_configured || !beacon_crypto_configured) return;

    // Build LXMF message plaintext
    uint8_t lxmf_msg[300];
    int msg_len = lxmf_build_message(lxmf_msg, sizeof(lxmf_msg),
                                      collector_dest_hash,
                                      lat, lon, alt, speed, hdop,
                                      timestamp, bat_percent);
    if (msg_len <= 0) return;

    // RNS packet: HEADER_2 is required — transport nodes only forward
    // packets with explicit transport_id. HEADER_1 packets are silently
    // dropped for transport purposes.
    int hdr_len;
    if (transport_configured) {
        // HEADER_2: flags + hops + transport_id(16) + dest_hash(16) + context
        tbuf[0] = 0x50;  // HEADER_2(1<<6) | TRANSPORT(1<<4) | SINGLE(0) | DATA(0)
        tbuf[1] = 0x00;  // HOPS
        memcpy(&tbuf[2], transport_id, 16);
        memcpy(&tbuf[18], collector_dest_hash, 16);
        tbuf[34] = 0x00; // CONTEXT_NONE
        hdr_len = 35;
    } else {
        // HEADER_1: flags + hops + dest_hash(16) + context
        tbuf[0] = 0x00;  // HEADER_1, BROADCAST, SINGLE, DATA
        tbuf[1] = 0x00;  // HOPS
        memcpy(&tbuf[2], collector_dest_hash, 16);
        tbuf[18] = 0x00; // CONTEXT_NONE
        hdr_len = 19;
    }

    int crypto_len = beacon_crypto_encrypt(
        lxmf_msg, msg_len,
        collector_pub_key, collector_identity_hash,
        &tbuf[hdr_len]
    );

    // LoRa FIFO limit is 255 bytes. After IFAC (+8 bytes), the packet
    // must still fit. Check against 247 (255 - IFAC_SIZE) to prevent
    // silent truncation that corrupts the IFAC signature.
    int pre_ifac_size = hdr_len + crypto_len;
    int max_lora = 255 - 8;  // IFAC adds 8 bytes
    if (crypto_len > 0 && pre_ifac_size <= max_lora && pre_ifac_size <= (int)MTU) {
        beacon_transmit(pre_ifac_size);
        lora_receive();
    }
}

// Build and transmit an LXMF announce packet.
static void lxmf_announce_send(const char *display_name) {
    if (!lxmf_identity_configured) return;

    int pkt_len = lxmf_build_announce(tbuf, MTU, display_name);
    if (pkt_len > 0) {
        beacon_transmit(pkt_len);
        lora_receive();
    }
}

// Check if announce is due and send it.
static void lxmf_announce_if_needed(const char *display_name) {
    if (!lxmf_identity_configured) return;

    if (lxmf_last_announce == 0 ||
        (millis() - lxmf_last_announce >= LXMF_ANNOUNCE_INTERVAL_MS)) {
        lxmf_announce_send(display_name);
        lxmf_last_announce = millis();
    }
}

// ---- CMD_LXMF_TEST: Force-trigger announce + beacon for USB testing ----
// Emits pre-encryption plaintext as CMD_DIAG KISS frames over serial,
// then transmits encrypted packets over LoRa.

// KISS framing constants (from Framing.h, repeated here to avoid include-order issues)
#ifndef LXMF_KISS_FEND
#define LXMF_KISS_FEND   0xC0
#define LXMF_KISS_FESC   0xDB
#define LXMF_KISS_TFEND  0xDC
#define LXMF_KISS_TFESC  0xDD
#define LXMF_CMD_DIAG    0x2C
#endif

// Forward declarations from main firmware
void serial_write(uint8_t byte);
bool startRadio();
void setTXPower();
void setBandwidth();
void setSpreadingFactor();
void setCodingRate();

static void kiss_emit_diag(const uint8_t *data, size_t len) {
    serial_write(LXMF_KISS_FEND);
    serial_write(LXMF_CMD_DIAG);
    for (size_t i = 0; i < len; i++) {
        uint8_t b = data[i];
        if (b == LXMF_KISS_FEND) { serial_write(LXMF_KISS_FESC); serial_write(LXMF_KISS_TFEND); }
        else if (b == LXMF_KISS_FESC) { serial_write(LXMF_KISS_FESC); serial_write(LXMF_KISS_TFESC); }
        else serial_write(b);
    }
    serial_write(LXMF_KISS_FEND);
}

// Beacon radio parameters (from Beacon.h, repeated to avoid include-order dependency)
#ifndef LXMF_BEACON_RADIO_FREQ
#define LXMF_BEACON_RADIO_FREQ  868000000
#define LXMF_BEACON_RADIO_BW    125000
#define LXMF_BEACON_RADIO_SF    7
#define LXMF_BEACON_RADIO_CR    5
#define LXMF_BEACON_RADIO_TXP   17
#endif

static void lxmf_test_send() {
    if (!lxmf_identity_configured) return;

    // Save current LoRa params
    uint32_t save_freq = lora_freq;
    uint32_t save_bw   = lora_bw;
    int      save_sf   = lora_sf;
    int      save_cr   = lora_cr;
    int      save_txp  = lora_txp;

    // Set beacon LoRa params
    lora_freq = (uint32_t)LXMF_BEACON_RADIO_FREQ;
    lora_bw   = (uint32_t)LXMF_BEACON_RADIO_BW;
    lora_sf   = LXMF_BEACON_RADIO_SF;
    lora_cr   = LXMF_BEACON_RADIO_CR;
    lora_txp  = LXMF_BEACON_RADIO_TXP;

    if (!radio_online) {
        startRadio();
    }
    if (radio_online) {
        setTXPower();
        setBandwidth();
        setSpreadingFactor();
        setCodingRate();
    }

    // 1. Build and emit announce
    int ann_len = lxmf_build_announce(tbuf, MTU, "RNode GPS Tracker");
    if (ann_len > 0) {
        // Emit announce plaintext as CMD_DIAG (pre-IFAC)
        kiss_emit_diag(tbuf, ann_len);
        // Transmit over LoRa (beacon_transmit applies IFAC)
        if (radio_online) {
            beacon_transmit(ann_len);
        }
    }

    // 2. Build and emit LXMF beacon
    if (beacon_crypto_configured) {
        // Get Unix timestamp from GPS (always available when beaconing)
        uint32_t timestamp = (uint32_t)(millis() / 1000);
        #if HAS_GPS == true
        {
            extern TinyGPSPlus gps_parser;
            if (gps_parser.date.isValid() && gps_parser.time.isValid() && gps_parser.date.year() >= 2024) {
                uint32_t days = 0;
                uint16_t yr = gps_parser.date.year();
                uint8_t mo = gps_parser.date.month();
                for (uint16_t y = 1970; y < yr; y++)
                    days += (y % 4 == 0) ? 366 : 365;
                static const uint16_t mdays[] = {0,31,59,90,120,151,181,212,243,273,304,334};
                if (mo >= 1 && mo <= 12) {
                    days += mdays[mo - 1];
                    if (mo > 2 && (yr % 4 == 0)) days++;
                }
                days += gps_parser.date.day() - 1;
                timestamp = days * 86400UL + gps_parser.time.hour() * 3600UL
                          + gps_parser.time.minute() * 60UL + gps_parser.time.second();
            }
        }
        #endif

        // Build LXMF message plaintext
        uint8_t lxmf_msg[300];
        int msg_len = lxmf_build_message(lxmf_msg, sizeof(lxmf_msg),
                                          collector_dest_hash,
                                          gps_lat, gps_lon, gps_alt,
                                          gps_speed, gps_hdop,
                                          timestamp, (int)battery_percent);

        if (msg_len > 0) {
            // Emit LXMF plaintext as CMD_DIAG (pre-encryption)
            kiss_emit_diag(lxmf_msg, msg_len);

            // Build encrypted RNS packet and transmit
            int hdr_len;
            if (transport_configured) {
                tbuf[0] = 0x50;  // HEADER_2 | TRANSPORT | SINGLE | DATA
                tbuf[1] = 0x00;
                memcpy(&tbuf[2], transport_id, 16);
                memcpy(&tbuf[18], collector_dest_hash, 16);
                tbuf[34] = 0x00;
                hdr_len = 35;
            } else {
                tbuf[0] = 0x00;  // HEADER_1 | BROADCAST | SINGLE | DATA
                tbuf[1] = 0x00;
                memcpy(&tbuf[2], collector_dest_hash, 16);
                tbuf[18] = 0x00;
                hdr_len = 19;
            }

            int crypto_len = beacon_crypto_encrypt(
                lxmf_msg, msg_len,
                collector_pub_key, collector_identity_hash,
                &tbuf[hdr_len]
            );

            if (crypto_len > 0 && (hdr_len + crypto_len) <= (int)MTU && radio_online) {
                beacon_transmit(hdr_len + crypto_len);
            }
        }
    }

    // Restore LoRa params
    lora_freq = save_freq;
    lora_bw   = save_bw;
    lora_sf   = save_sf;
    lora_cr   = save_cr;
    lora_txp  = save_txp;

    if (radio_online) {
        setTXPower();
        setBandwidth();
        setSpreadingFactor();
        setCodingRate();
    }

    lora_receive();
}

#endif // HAS_GPS
#endif // LXMF_BEACON_H
