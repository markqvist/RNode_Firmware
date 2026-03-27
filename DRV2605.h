// DRV2605 Haptic Driver — Minimal I2C driver for T-Watch Ultra
// ERM vibration motor with 117 built-in effects
// EN pin controlled by XL9555 EXPANDS_DRV_EN (port 0, pin 6)

#ifndef DRV2605_H
#define DRV2605_H

#if BOARD_MODEL == BOARD_TWATCH_ULT

#include <Wire.h>

#define DRV2605_ADDR       0x5A

// Registers
#define DRV2605_STATUS     0x00
#define DRV2605_MODE       0x01
#define DRV2605_RTPIN      0x02
#define DRV2605_LIBRARY    0x03
#define DRV2605_WAVESEQ1   0x04
#define DRV2605_GO         0x0C
#define DRV2605_OVERDRIVE  0x0D
#define DRV2605_SUSTAINPOS 0x0E
#define DRV2605_SUSTAINNEG 0x0F
#define DRV2605_BRAKE      0x10
#define DRV2605_FEEDBACK   0x1A
#define DRV2605_CONTROL3   0x1D

// Named effects for watch use cases (ERM Library 1, effects 1-117)
#define HAPTIC_STRONG_CLICK   1   // Strong Click - 100%
#define HAPTIC_MEDIUM_CLICK   2   // Strong Click - 60%
#define HAPTIC_LIGHT_CLICK    3   // Strong Click - 30%
#define HAPTIC_SHARP_CLICK    4   // Sharp Click - 100%
#define HAPTIC_SOFT_BUMP      7   // Soft Bump - 100%
#define HAPTIC_DOUBLE_CLICK  10   // Double Click - 100%
#define HAPTIC_TRIPLE_CLICK  12   // Triple Click - 100%
#define HAPTIC_BUZZ          14   // Strong Buzz - 100%
#define HAPTIC_ALERT         15   // 750ms Alert - 100%
#define HAPTIC_LONG_ALERT    16   // 1000ms Alert - 100%
#define HAPTIC_TICK           4   // Sharp Click (subtle tick)
#define HAPTIC_TRANSITION    47   // Transition Click - 100%

static bool drv2605_ready = false;

static void drv2605_write(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(DRV2605_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

static uint8_t drv2605_read(uint8_t reg) {
  Wire.beginTransmission(DRV2605_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)DRV2605_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0;
}

bool drv2605_init() {
  // Probe device
  Wire.beginTransmission(DRV2605_ADDR);
  if (Wire.endTransmission() != 0) return false;

  // Verify chip ID (bits 7:5 of STATUS should be 3 or 7)
  uint8_t id = drv2605_read(DRV2605_STATUS) >> 5;
  if (id != 3 && id != 7) return false;

  // Exit standby
  drv2605_write(DRV2605_MODE, 0x00);

  // Disable real-time playback input
  drv2605_write(DRV2605_RTPIN, 0x00);

  // Select ERM mode: clear bit 7 of FEEDBACK register
  uint8_t fb = drv2605_read(DRV2605_FEEDBACK);
  drv2605_write(DRV2605_FEEDBACK, fb & 0x7F);

  // Enable open-loop drive: set bit 5 of CONTROL3
  uint8_t ctrl3 = drv2605_read(DRV2605_CONTROL3);
  drv2605_write(DRV2605_CONTROL3, ctrl3 | 0x20);

  // Select ERM effect library 1
  drv2605_write(DRV2605_LIBRARY, 1);

  // Clear timing offsets
  drv2605_write(DRV2605_OVERDRIVE, 0);
  drv2605_write(DRV2605_SUSTAINPOS, 0);
  drv2605_write(DRV2605_SUSTAINNEG, 0);
  drv2605_write(DRV2605_BRAKE, 0);

  // Clear all waveform slots
  for (uint8_t i = 0; i < 8; i++) {
    drv2605_write(DRV2605_WAVESEQ1 + i, 0);
  }

  drv2605_ready = true;
  return true;
}

// Play a single effect (1-117 from ERM library)
void drv2605_play(uint8_t effect) {
  if (!drv2605_ready) return;
  drv2605_write(DRV2605_MODE, 0x00);          // Internal trigger mode
  drv2605_write(DRV2605_WAVESEQ1, effect);    // Effect in slot 1
  drv2605_write(DRV2605_WAVESEQ1 + 1, 0);    // End sequence
  drv2605_write(DRV2605_GO, 1);               // Start playback
}

// Play a sequence of up to 8 effects
void drv2605_sequence(const uint8_t *effects, uint8_t count) {
  if (!drv2605_ready || count == 0) return;
  if (count > 8) count = 8;
  drv2605_write(DRV2605_MODE, 0x00);
  for (uint8_t i = 0; i < count; i++) {
    drv2605_write(DRV2605_WAVESEQ1 + i, effects[i]);
  }
  if (count < 8) {
    drv2605_write(DRV2605_WAVESEQ1 + count, 0);  // Terminate
  }
  drv2605_write(DRV2605_GO, 1);
}

// Stop any playing effect
void drv2605_stop() {
  if (!drv2605_ready) return;
  drv2605_write(DRV2605_GO, 0);
}

// Check if an effect is still playing
bool drv2605_busy() {
  if (!drv2605_ready) return false;
  return drv2605_read(DRV2605_GO) & 1;
}

#endif // BOARD_MODEL == BOARD_TWATCH_ULT
#endif // DRV2605_H
