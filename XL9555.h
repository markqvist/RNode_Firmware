// XL9555 I2C GPIO Expander - Minimal driver for T-Watch Ultra
// Two 8-bit ports (0-7 = port 0, 8-15 = port 1)
// Registers: 0x00/0x01 input, 0x02/0x03 output, 0x06/0x07 direction (1=input, 0=output)

#ifndef XL9555_H
#define XL9555_H

#include <Wire.h>

#define XL9555_ADDR       0x20
#define XL9555_REG_IN0    0x00
#define XL9555_REG_IN1    0x01
#define XL9555_REG_OUT0   0x02
#define XL9555_REG_OUT1   0x03
#define XL9555_REG_DIR0   0x06
#define XL9555_REG_DIR1   0x07

// T-Watch Ultra expander pin assignments (matching LilyGoLib numbering)
#define EXPANDS_DRV_EN      6    // Port 0, bit 6 — haptic driver enable
#define EXPANDS_DISP_EN     14   // Port 1, bit 6 — display power gate
#define EXPANDS_DISP_RST    15   // Port 1, bit 7 — display reset
#define EXPANDS_TOUCH_RST   16   // Extended pin — touch panel reset
#define EXPANDS_SD_DET      12   // Port 1, bit 4 — SD card detect (input)
#define EXPANDS_LORA_RF_SW  11   // Port 1, bit 3 — LoRa RF switch

static bool xl9555_ready = false;
static uint8_t xl9555_out[2] = {0xFF, 0xFF};  // output register cache

static uint8_t xl9555_read_reg(uint8_t reg) {
  Wire.beginTransmission(XL9555_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)XL9555_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0xFF;
}

static void xl9555_write_reg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(XL9555_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

bool xl9555_init() {
  Wire.beginTransmission(XL9555_ADDR);
  if (Wire.endTransmission() != 0) return false;

  // Set output pin directions (0 = output, 1 = input)
  // Port 0: pin 6 (DRV_EN) as output, rest input
  uint8_t dir0 = 0xFF & ~(1 << 6);
  // Port 1: pins 3 (LORA_RF_SW), 6 (DISP_EN), 7 (DISP_RST) as output
  uint8_t dir1 = 0xFF & ~((1 << 3) | (1 << 6) | (1 << 7));

  xl9555_write_reg(XL9555_REG_DIR0, dir0);
  xl9555_write_reg(XL9555_REG_DIR1, dir1);

  // Read current output state
  xl9555_out[0] = xl9555_read_reg(XL9555_REG_OUT0);
  xl9555_out[1] = xl9555_read_reg(XL9555_REG_OUT1);

  xl9555_ready = true;
  return true;
}

void xl9555_set(uint8_t pin, bool value) {
  if (!xl9555_ready) return;
  uint8_t port = (pin >= 8) ? 1 : 0;
  uint8_t bit = pin % 8;

  if (value) {
    xl9555_out[port] |= (1 << bit);
  } else {
    xl9555_out[port] &= ~(1 << bit);
  }
  xl9555_write_reg(port == 0 ? XL9555_REG_OUT0 : XL9555_REG_OUT1, xl9555_out[port]);
}

bool xl9555_get(uint8_t pin) {
  if (!xl9555_ready) return false;
  uint8_t port = (pin >= 8) ? 1 : 0;
  uint8_t bit = pin % 8;
  uint8_t val = xl9555_read_reg(port == 0 ? XL9555_REG_IN0 : XL9555_REG_IN1);
  return (val >> bit) & 1;
}

// Convenience functions for sleep entry
void xl9555_sleep_prepare() {
  if (!xl9555_ready) return;
  xl9555_set(EXPANDS_DISP_RST, false);   // hold display in reset
  xl9555_set(EXPANDS_DRV_EN, false);      // disable haptic
  xl9555_set(EXPANDS_DISP_EN, false);     // gate display power
}

void xl9555_wake_display() {
  if (!xl9555_ready) return;
  xl9555_set(EXPANDS_DISP_EN, true);      // enable display power
  xl9555_set(EXPANDS_DISP_RST, false);    // reset pulse
  delay(50);
  xl9555_set(EXPANDS_DISP_RST, true);
}

void xl9555_enable_lora_antenna() {
  xl9555_set(EXPANDS_LORA_RF_SW, true);   // select built-in antenna
}

#endif
