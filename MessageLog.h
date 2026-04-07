// MessageLog.h — Received packet log and RNS announce parser
// Stores recent LoRa packets for the Messages screen viewer

#ifndef MESSAGELOG_H
#define MESSAGELOG_H

#if BOARD_MODEL == BOARD_TWATCH_ULT

#include "mbedtls/sha256.h"

// RNS packet type bits (flags byte, bits 1-0)
#define RNS_TYPE_DATA     0x00
#define RNS_TYPE_ANNOUNCE 0x01
// RNS header type (flags byte, bit 6)
#define RNS_HEADER_1      0x00
#define RNS_HEADER_2      0x40
// IFAC flag (flags byte, bit 7)
#define RNS_IFAC_FLAG     0x80

// msg_entry_t struct defined in Gui.h (included earlier)
// MSG_LOG_SIZE and MSG_NAME_LEN also defined there
#define MSG_DEDUP_MS  60000  // ignore same sender within 60s

msg_entry_t msg_log[MSG_LOG_SIZE];
uint8_t msg_log_head = 0;
uint8_t msg_log_count = 0;
uint32_t msg_log_last_change = 0;

// Find existing entry for this sender (for de-dup / update)
static int msg_log_find_sender(const uint8_t *hash) {
    for (int i = 0; i < msg_log_count; i++) {
        int idx = (msg_log_head - 1 - i + MSG_LOG_SIZE) % MSG_LOG_SIZE;
        if (memcmp(msg_log[idx].sender_hash, hash, 16) == 0) return idx;
    }
    return -1;
}

// Add or update a message entry
static void msg_log_add(const uint8_t *sender_hash, const char *name,
                         int16_t rssi, int8_t snr, uint8_t pkt_type,
                         uint16_t pkt_len, bool is_announce) {
    // De-dup: update existing entry for same sender
    int existing = msg_log_find_sender(sender_hash);
    if (existing >= 0) {
        msg_entry_t &e = msg_log[existing];
        if (millis() - e.timestamp < MSG_DEDUP_MS && !is_announce) return;
        // Update existing
        e.timestamp = millis();
        e.rssi = rssi;
        e.snr = snr;
        e.pkt_type = pkt_type;
        e.pkt_len = pkt_len;
        if (is_announce && name[0]) {
            strncpy(e.display_name, name, MSG_NAME_LEN - 1);
            e.display_name[MSG_NAME_LEN - 1] = '\0';
            e.is_announce = true;
        }
        msg_log_last_change = millis();
        return;
    }

    // New entry
    msg_entry_t &e = msg_log[msg_log_head];
    e.timestamp = millis();
    e.rssi = rssi;
    e.snr = snr;
    e.pkt_type = pkt_type;
    e.pkt_len = pkt_len;
    e.is_announce = is_announce;
    memcpy(e.sender_hash, sender_hash, 16);
    if (name && name[0]) {
        strncpy(e.display_name, name, MSG_NAME_LEN - 1);
        e.display_name[MSG_NAME_LEN - 1] = '\0';
    } else {
        // Short hex of sender hash
        snprintf(e.display_name, MSG_NAME_LEN, "%02x%02x%02x..%02x%02x",
                 sender_hash[0], sender_hash[1], sender_hash[2],
                 sender_hash[14], sender_hash[15]);
    }
    msg_log_head = (msg_log_head + 1) % MSG_LOG_SIZE;
    if (msg_log_count < MSG_LOG_SIZE) msg_log_count++;
    msg_log_last_change = millis();
}

// Parse an RNS packet and log it
// Called from main loop after dequeue, before kiss_write_packet
// pkt points to raw LoRa payload (after split reassembly), len is total length
static void msg_log_parse_packet(const uint8_t *pkt, uint16_t len,
                                  int16_t rssi, int8_t snr) {
    if (len < 19) return;  // too short for RNS header

    uint8_t flags = pkt[0];
    bool has_ifac = (flags & RNS_IFAC_FLAG) != 0;
    uint8_t pkt_type = flags & 0x03;

    // IFAC packets: the real header starts after unmasking, but we can
    // still extract the dest_hash from bytes 2-17 (masked but deterministic position)
    // For announces, the IFAC is stripped by the receiver's IFAC handler.
    // For our purposes, we parse what we can.

    int hdr_offset = 0;  // where the header starts after IFAC
    if (has_ifac) {
        // IFAC-wrapped: bytes 0-1 are masked header, bytes 2-9 are IFAC signature,
        // rest is masked payload. We can't parse without unmasking.
        // But we still log the packet with the dest_hash from bytes 2-17
        // (these are the IFAC sig bytes, not the actual dest_hash)
        // For now, just log as generic packet with flags info
        uint8_t generic_hash[16];
        memcpy(generic_hash, pkt + 2, 16);
        msg_log_add(generic_hash, NULL, rssi, snr, pkt_type, len, false);
        return;
    }

    // Non-IFAC packet — parse RNS header directly
    const uint8_t *dest_hash = pkt + 2;  // bytes 2-17

    if (pkt_type == RNS_TYPE_ANNOUNCE && len > 167) {
        // ANNOUNCE packet structure (HEADER_1, 19-byte header):
        // [flags(1) hops(1) dest_hash(16) context(1)]
        // [pub_keys(64) = x25519(32) + ed25519(32)]
        // [name_hash(10)]
        // [random_hash(10)]
        // [signature(64)]
        // [app_data(variable) = display name]

        // Compute identity hash from public keys
        const uint8_t *pub_keys = pkt + 19;  // 64 bytes
        uint8_t identity_hash[32];
        mbedtls_sha256(pub_keys, 64, identity_hash, 0);

        // Extract display name from app_data
        char name[MSG_NAME_LEN] = {0};
        int app_data_offset = 19 + 64 + 10 + 10 + 64;  // = 167
        int app_data_len = len - app_data_offset;
        if (app_data_len > 0 && app_data_len < MSG_NAME_LEN) {
            memcpy(name, pkt + app_data_offset, app_data_len);
            name[app_data_len] = '\0';
        }

        msg_log_add(identity_hash, name, rssi, snr, pkt_type, len, true);
    } else {
        // DATA or short packet — log with dest_hash
        msg_log_add(dest_hash, NULL, rssi, snr, pkt_type, len, false);
    }
}

#endif // BOARD_MODEL == BOARD_TWATCH_ULT
#endif // MESSAGELOG_H
