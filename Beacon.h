// Copyright (C) 2026, GPS beacon support contributed by GlassOnTin

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

#ifndef BEACON_H
#define BEACON_H

#if HAS_GPS == true

// Beacon interval and timing
uint32_t beacon_interval_ms = 30000;    // Default 30s, configurable via Settings
#define BEACON_STARTUP_DELAY_MS  10000  // Wait 10s after boot before first beacon
// BEACON_NO_HOST_TIMEOUT_MS and last_host_activity defined in GPS.h
bool beacon_enabled = true;             // Configurable via Settings

// Beacon interval options (ms) — indexed by roller selection
const uint32_t beacon_interval_options[] = { 10000, 30000, 60000, 300000, 600000 };
#define BEACON_INTERVAL_OPTIONS_COUNT 5

// Beacon radio parameters — must match the router's LoRa interface
#define BEACON_FREQ  868000000
#define BEACON_BW    125000
#define BEACON_SF    7
#define BEACON_CR    5
#define BEACON_TXP   17

// Pre-computed RNS destination hash for PLAIN destination "rnlog.beacon"
// Computed as: SHA256(SHA256("rnlog.beacon")[:10])[:16]
const uint8_t RNS_DEST_HASH[16] = {
    0x18, 0xbc, 0xd8, 0xa3, 0xde, 0xa1, 0x6e, 0xf6,
    0x76, 0x5c, 0x6b, 0x27, 0xd0, 0x08, 0xd2, 0x20
};

// RNS packet header constants (PLAIN destination, HEADER_1, DATA)
// FLAGS: header_type=0 (HEADER_1), propagation=0 (BROADCAST),
//        destination=0 (PLAIN), packet_type=2 (DATA), transport=0
#define RNS_FLAGS   0x08
#define RNS_CONTEXT 0x00

// Beacon state
bool beacon_mode_active = false;
uint32_t last_beacon_tx = 0;

// Forward declarations from main firmware
void lora_receive();
bool startRadio();
void setTXPower();
void setBandwidth();
void setSpreadingFactor();
void setCodingRate();
void beacon_transmit(uint16_t size);

// Diagnostic: track which gate blocks beacon_update()
// 0=not called, 1=host active, 2=startup delay, 3=radio offline,
// 4=no gps fix, 5=interval wait, 6=beacon sent
uint8_t beacon_gate = 0;

void beacon_check_host_activity() {
    last_host_activity = millis();
    if (beacon_mode_active) {
        beacon_mode_active = false;
    }
}

void beacon_update() {
    if (!beacon_enabled) { beacon_gate = 0; return; }
    // Don't beacon if host has been active recently
    if (last_host_activity > 0 &&
        (millis() - last_host_activity < BEACON_NO_HOST_TIMEOUT_MS)) {
        beacon_gate = 1;
        return;
    }

    // Wait for startup delay after boot
    if (millis() < BEACON_STARTUP_DELAY_MS) {
        beacon_gate = 2;
        return;
    }

    // No point beaconing without a GPS fix — check BEFORE touching
    // radio params to avoid putting the radio into standby needlessly
    if (!gps_has_fix) {
        beacon_gate = 4;
        return;
    }

    // Radio must be online — restart if needed
    if (!radio_online) {
        lora_freq = (uint32_t)868000000;
        lora_bw   = (uint32_t)125000;
        lora_sf   = 7;
        lora_cr   = 5;
        lora_txp  = 17;
        if (!startRadio()) {
            beacon_gate = 3;
            return;
        }
    } else {
        // Radio is online but may have host/EEPROM params — force beacon settings
        lora_freq = (uint32_t)868000000;
        lora_bw   = (uint32_t)125000;
        lora_sf   = 7;
        lora_cr   = 5;
        lora_txp  = 17;
        setTXPower();
        setBandwidth();
        setSpreadingFactor();
        setCodingRate();
    }

    // Respect beacon interval
    if (last_beacon_tx > 0 &&
        (millis() - last_beacon_tx < beacon_interval_ms)) {
        beacon_gate = 5;
        return;
    }

    beacon_mode_active = true;
    beacon_gate = 6;

    // LXMF path: send proper LXMF message with FIELD_TELEMETRY directly to Sideband
    if (lxmf_identity_configured) {
        // Periodic LXMF announce (every 10 minutes)
        lxmf_announce_if_needed("RNode GPS Tracker");

        // Get Unix timestamp from GPS directly
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

        lxmf_beacon_send(gps_lat, gps_lon, gps_alt,
                          gps_speed, gps_hdop,
                          timestamp, (int)battery_percent);
        last_beacon_tx = millis();
        return;
    }

    // Legacy path: JSON payload for rnlog collector (no LXMF identity)
    char json_buf[256];
    int json_len = snprintf(json_buf, sizeof(json_buf),
        "{\"lat\":%.6f,\"lon\":%.6f,\"alt\":%.1f,"
        "\"sat\":%d,\"spd\":%.1f,\"hdop\":%.1f,"
        "\"bat\":%d,\"fix\":%s}",
        gps_lat, gps_lon, gps_alt,
        gps_sats, gps_speed, gps_hdop,
        (int)battery_percent,
        gps_has_fix ? "true" : "false");

    if (json_len <= 0 || json_len >= (int)sizeof(json_buf)) return;

    if (beacon_crypto_configured) {
        // Encrypted SINGLE packet (legacy JSON to rnlog.collector)
        tbuf[0] = 0x00;  // FLAGS: HEADER_1, BROADCAST, SINGLE, DATA
        tbuf[1] = 0x00;  // HOPS
        memcpy(&tbuf[2], collector_dest_hash, 16);
        tbuf[18] = 0x00; // CONTEXT_NONE

        int crypto_len = beacon_crypto_encrypt(
            (uint8_t*)json_buf, json_len,
            collector_pub_key, collector_identity_hash,
            &tbuf[19]
        );

        if (crypto_len > 0 && (19 + crypto_len) <= (int)MTU) {
            beacon_transmit(19 + crypto_len);
            lora_receive();
            last_beacon_tx = millis();
        }
    } else {
        // Fallback: PLAIN beacon (unencrypted, original behavior)
        tbuf[0] = RNS_FLAGS;  // 0x08 = PLAIN DATA
        tbuf[1] = 0x00;
        memcpy(&tbuf[2], RNS_DEST_HASH, 16);
        tbuf[18] = RNS_CONTEXT;

        memcpy(&tbuf[19], json_buf, json_len);
        if (19 + json_len <= (int)MTU) {
            beacon_transmit(19 + json_len);
            lora_receive();
            last_beacon_tx = millis();
        }
    }
}

#endif
#endif
