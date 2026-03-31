// Copyright (C) 2024, Mark Qvist
// Copyright (C) 2026, GPS support contributed by GlassOnTin

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

#ifndef GPS_H
#define GPS_H

#if HAS_GPS == true

#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

TinyGPSPlus gps_parser;
HardwareSerial gps_serial(1);

bool gps_ready = false;
bool gps_has_fix = false;
uint8_t gps_sats = 0;
double gps_lat = 0.0;
double gps_lon = 0.0;
double gps_alt = 0.0;
double gps_speed = 0.0;
double gps_hdop = 0.0;
uint32_t gps_last_update = 0;
uint32_t gps_last_report = 0;
#define GPS_REPORT_INTERVAL_MS 30000  // Report GPS stats to host every 30s

void kiss_indicate_stat_gps();

// Host activity tracking — shared with Beacon.h
// GPS telemetry and beacon mode both suppress when host is active.
#define BEACON_NO_HOST_TIMEOUT_MS 15000
uint32_t last_host_activity = 0;

void gps_power_on() {
  #if defined(PIN_GPS_EN)
    pinMode(PIN_GPS_EN, OUTPUT);
    #if GPS_EN_ACTIVE == LOW
      digitalWrite(PIN_GPS_EN, LOW);
    #else
      digitalWrite(PIN_GPS_EN, HIGH);
    #endif
  #endif

  #if defined(PIN_GPS_RST)
    // Keep reset HIGH (inactive) to preserve backup RAM (ephemeris/almanac).
    // This allows warm/hot starts with much faster time-to-fix.
    pinMode(PIN_GPS_RST, OUTPUT);
    digitalWrite(PIN_GPS_RST, HIGH);
  #endif

  #if defined(PIN_GPS_STANDBY)
    pinMode(PIN_GPS_STANDBY, OUTPUT);
    digitalWrite(PIN_GPS_STANDBY, HIGH);
  #endif
}

void gps_power_off() {
  #if defined(PIN_GPS_EN)
    #if GPS_EN_ACTIVE == LOW
      digitalWrite(PIN_GPS_EN, HIGH);
    #else
      digitalWrite(PIN_GPS_EN, LOW);
    #endif
  #endif
}

void gps_setup() {
  gps_power_on();
  delay(1000);  // Allow GPS module time to boot
  // PIN_GPS_TX/RX named from ESP32 perspective:
  // PIN_GPS_RX = ESP32 receives FROM GPS module
  // PIN_GPS_TX = ESP32 transmits TO GPS module
  gps_serial.begin(GPS_BAUD_RATE, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
  delay(250);

  #if BOARD_MODEL == BOARD_TWATCH_ULT
    // MIA-M10Q (u-blox): outputs NMEA at 38400 baud by default.
    // GGA, GSA, GSV, RMC are enabled out of the box.
    // No vendor-specific init commands needed.
  #else
    // L76K init: force internal antenna (ceramic patch)
    gps_serial.print("$PCAS15,0*19\r\n");
    delay(250);
    // Hot start — use cached ephemeris/almanac if available in L76K backup RAM
    gps_serial.print("$PCAS10,0*1C\r\n");
    delay(500);
    // Enable GPS+GLONASS+BeiDou
    gps_serial.print("$PCAS04,7*1E\r\n");
    delay(250);
    // Output GGA, GSA, GSV, and RMC
    gps_serial.print("$PCAS03,1,0,1,1,1,0,0,0,0,0,,,0,0*02\r\n");
    delay(250);
    // Set navigation mode to Portable (general purpose, works stationary and moving)
    gps_serial.print("$PCAS11,0*1D\r\n");
    delay(250);
  #endif

  gps_ready = true;
}

// GPS dynamic model options — indexed by roller selection
const uint8_t gps_model_ubx[] = { 0, 2, 3, 4 };  // Portable, Stationary, Pedestrian, Automotive
#define GPS_MODEL_OPTIONS_COUNT 4
uint8_t gps_dynamic_model = 0;  // Current model index (default: Portable)

#if BOARD_MODEL == BOARD_TWATCH_ULT
void gps_set_dynamic_model(uint8_t model_index) {
  if (model_index >= GPS_MODEL_OPTIONS_COUNT) return;
  uint8_t dyn = gps_model_ubx[model_index];
  gps_dynamic_model = model_index;
  uint8_t msg[] = {
    0xB5, 0x62, 0x06, 0x8A, 0x09, 0x00,
    0x00, 0x01, 0x00, 0x00,
    0x21, 0x00, 0x11, 0x20,
    dyn, 0x00, 0x00
  };
  uint8_t ck_a = 0, ck_b = 0;
  for (int i = 2; i < (int)sizeof(msg) - 2; i++) { ck_a += msg[i]; ck_b += ck_a; }
  msg[sizeof(msg) - 2] = ck_a;
  msg[sizeof(msg) - 1] = ck_b;
  gps_serial.write(msg, sizeof(msg));
}
#else
void gps_set_dynamic_model(uint8_t model_index) {
  if (model_index >= GPS_MODEL_OPTIONS_COUNT) return;
  gps_dynamic_model = model_index;
  const char *cmds[] = { "$PCAS11,0*1D\r\n", "$PCAS11,1*1C\r\n",
                         "$PCAS11,2*1F\r\n", "$PCAS11,3*1E\r\n" };
  gps_serial.print(cmds[model_index]);
}
#endif

void gps_update() {
  if (!gps_ready) return;

  while (gps_serial.available() > 0) {
    gps_parser.encode(gps_serial.read());
  }

  if (gps_parser.location.isUpdated()) {
    gps_has_fix = gps_parser.location.isValid();
    if (gps_has_fix) {
      gps_lat = gps_parser.location.lat();
      gps_lon = gps_parser.location.lng();
      gps_last_update = millis();
    }
  }

  if (gps_parser.altitude.isUpdated() && gps_parser.altitude.isValid()) {
    gps_alt = gps_parser.altitude.meters();
  }

  if (gps_parser.speed.isUpdated() && gps_parser.speed.isValid()) {
    gps_speed = gps_parser.speed.kmph();
  }

  if (gps_parser.satellites.isUpdated()) {
    gps_sats = gps_parser.satellites.value();
  }

  if (gps_parser.hdop.isUpdated()) {
    gps_hdop = gps_parser.hdop.hdop();
  }

  // Mark fix as stale after 10 seconds without update
  if (gps_has_fix && (millis() - gps_last_update > 10000)) {
    gps_has_fix = false;
  }

  // Periodically report GPS stats to host (like battery/temp in Power.h).
  // rnsd parses CMD_STAT_GPS frames and exposes them in interface stats.
  if (millis() - gps_last_report >= GPS_REPORT_INTERVAL_MS) {
    response_channel = data_channel;
    kiss_indicate_stat_gps();
    gps_last_report = millis();
  }
}

void gps_teardown() {
  gps_serial.end();
  gps_power_off();
  gps_ready = false;
}

#endif
#endif
