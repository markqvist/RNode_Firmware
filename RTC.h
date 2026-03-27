// Copyright (C) 2026, GPS/RTC support contributed by GlassOnTin

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

#ifndef RTC_H
#define RTC_H

#if HAS_RTC == true

#include <Wire.h>

// RTC I2C address (shared by PCF8563 and PCF85063A)
#define RTC_I2C_ADDR     0x51

#if BOARD_MODEL == BOARD_TWATCH_ULT
  // PCF85063A registers (T-Watch Ultra)
  #define RTC_REG_CTRL1  0x00
  #define RTC_REG_CTRL2  0x01
  #define RTC_REG_SEC    0x04
  #define RTC_REG_MIN    0x05
  #define RTC_REG_HOUR   0x06
  #define RTC_REG_DAY    0x07
  #define RTC_REG_WDAY   0x08
  #define RTC_REG_MON    0x09
  #define RTC_REG_YEAR   0x0A
#else
  // PCF8563 registers (T-Beam Supreme, etc.)
  #define RTC_REG_CTRL1  0x00
  #define RTC_REG_CTRL2  0x01
  #define RTC_REG_SEC    0x02
  #define RTC_REG_MIN    0x03
  #define RTC_REG_HOUR   0x04
  #define RTC_REG_DAY    0x05
  #define RTC_REG_WDAY   0x06
  #define RTC_REG_MON    0x07
  #define RTC_REG_YEAR   0x08
#endif

bool rtc_ready = false;
bool rtc_synced = false;   // true once GPS time has been written to RTC
uint32_t rtc_last_sync = 0;
#define RTC_SYNC_INTERVAL 3600000  // re-sync from GPS every hour

// Cached time from last RTC read
uint8_t rtc_hour = 0;
uint8_t rtc_minute = 0;
uint8_t rtc_second = 0;
uint8_t rtc_day = 0;
uint8_t rtc_month = 0;
uint16_t rtc_year = 0;

static uint8_t bcd_to_dec(uint8_t bcd) { return (bcd >> 4) * 10 + (bcd & 0x0F); }
static uint8_t dec_to_bcd(uint8_t dec) { return ((dec / 10) << 4) | (dec % 10); }

void rtc_setup() {
  // Wire is already initialised by Power.h (PMU init) or Display.h.
  Wire.beginTransmission(RTC_I2C_ADDR);
  if (Wire.endTransmission() == 0) {
    rtc_ready = true;

    // Clear control registers (normal mode, no alarms)
    Wire.beginTransmission(RTC_I2C_ADDR);
    Wire.write(RTC_REG_CTRL1);
    Wire.write(0x00);  // normal mode
    Wire.write(0x00);  // control/status 2: no alarms/timer
    Wire.endTransmission();
  }
}

bool rtc_read_time() {
  if (!rtc_ready) return false;

  Wire.beginTransmission(RTC_I2C_ADDR);
  Wire.write(RTC_REG_SEC);
  if (Wire.endTransmission() != 0) return false;

  Wire.requestFrom((uint8_t)RTC_I2C_ADDR, (uint8_t)7);
  if (Wire.available() < 7) return false;

  uint8_t sec  = Wire.read();
  uint8_t min  = Wire.read();
  uint8_t hour = Wire.read();
  uint8_t day  = Wire.read();
  Wire.read();  // weekday — skip
  uint8_t mon  = Wire.read();
  uint8_t year = Wire.read();

  // Check oscillator stop bit (sec register bit 7)
  // Set on both PCF8563 and PCF85063A when clock integrity is lost
  if (sec & 0x80) return false;

  rtc_second = bcd_to_dec(sec & 0x7F);
  rtc_minute = bcd_to_dec(min & 0x7F);
  rtc_hour   = bcd_to_dec(hour & 0x3F);
  rtc_day    = bcd_to_dec(day & 0x3F);
  rtc_month  = bcd_to_dec(mon & 0x1F);
  rtc_year   = 2000 + bcd_to_dec(year);

  return true;
}

bool rtc_write_time(uint16_t year, uint8_t month, uint8_t day,
                    uint8_t hour, uint8_t minute, uint8_t second) {
  if (!rtc_ready) return false;

  Wire.beginTransmission(RTC_I2C_ADDR);
  Wire.write(RTC_REG_SEC);
  Wire.write(dec_to_bcd(second));
  Wire.write(dec_to_bcd(minute));
  Wire.write(dec_to_bcd(hour));
  Wire.write(dec_to_bcd(day));
  Wire.write(0x00);  // weekday (not used)
  Wire.write(dec_to_bcd(month));
  Wire.write(dec_to_bcd(year - 2000));
  return Wire.endTransmission() == 0;
}

// Called from gps_update() when GPS has a valid time fix.
// Syncs RTC from GPS time at most once per RTC_SYNC_INTERVAL.
void rtc_sync_from_gps(TinyGPSPlus &gps) {
  if (!rtc_ready) return;
  if (!gps.date.isValid() || !gps.time.isValid()) return;
  if (gps.date.year() < 2024) return;  // sanity check

  uint32_t now = millis();
  if (rtc_synced && (now - rtc_last_sync < RTC_SYNC_INTERVAL)) return;

  if (rtc_write_time(gps.date.year(), gps.date.month(), gps.date.day(),
                     gps.time.hour(), gps.time.minute(), gps.time.second())) {
    rtc_synced = true;
    rtc_last_sync = now;
  }
}

#endif
#endif
