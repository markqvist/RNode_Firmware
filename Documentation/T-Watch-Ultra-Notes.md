# T-Watch Ultra Development Notes

Hard-won lessons from porting RNode firmware to the LilyGo T-Watch Ultra.

## I2C Pin Default Mismatch (Critical)

The generic Arduino ESP32-S3 FQBN (`esp32:esp32:esp32s3`) defines default I2C pins as **SDA=8, SCL=9** (in `variants/esp32s3/pins_arduino.h`). The T-Watch Ultra uses **SDA=3, SCL=2**.

The XPowersLib `XPowersAXP2101` constructor stores the variant's default SDA/SCL internally:

```cpp
// BROKEN — uses default SDA=8, SCL=9 from variant
PMU = new XPowersAXP2101(Wire);
PMU->init();  // Calls Wire.begin(8, 9) — wrong pins!

// CORRECT — pass explicit pins
PMU = new XPowersAXP2101(Wire, I2C_SDA, I2C_SCL);
PMU->init();  // Calls Wire.begin(3, 2) — correct pins
```

**Symptom**: `Wire.begin(3, 2)` returns true, but no I2C device responds. SDA/SCL lines read HIGH (pullups working). Looks like a hardware failure but is entirely a firmware bug.

**Root cause**: `XPowersCommon::begin()` calls `__wire->begin(__sda, __scl)` where `__sda` and `__scl` were set to 8/9 by the constructor defaults. This silently reinitialises the I2C bus on the wrong pins.

This applies to **any ESP32-S3 board using non-default I2C pins with XPowersLib**. Always pass explicit SDA/SCL to the constructor.

## Flash Workflow

The T-Watch Ultra's built-in USB JTAG/serial interface does NOT support esptool auto-reset. Two flash methods:

### Method 1: esptool (faster, for large images)
1. Hold **BOOT** button, press **RST**, release BOOT → download mode
2. Flash: `esptool --chip esp32s3 --port /dev/ttyACM4 --baud 921600 write_flash 0x0 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 app.bin`
3. Press **RST** to exit download mode (battery keeps ESP32 running after USB cycle)

### Method 2: JTAG via OpenOCD (for small images <2MB)
```bash
openocd -f board/esp32s3-builtin.cfg -c "program_esp app.bin 0x10000 verify reset exit"
```
Full flash images (16MB factory binaries) exceed JTAG flash stub limits.

## Physical Buttons

- **BOOT**: Left edge of PCB (GPIO 0) — download mode entry
- **RST**: Right edge of PCB — hardware reset
- **<< and >|**: Side buttons — media/navigation, NOT power key
- **Power key**: Connected to AXP2101 PWR_KEY input — controls PMU power on/off

## Deep Sleep Caution

Do NOT call `PMU->enableSleep()` before `esp_deep_sleep_start()`. The AXP2101 retains its sleep mode state across battery-backed resets. If the PMU enters sleep mode, its I2C slave interface may become unresponsive, and the only recovery is a full battery disconnect (including the RTC button cell on the PCB).

Setting GPIO pins to `OPEN_DRAIN` before deep sleep is correct for power savings, but do NOT set the I2C pins (GPIO 2/3) to OPEN_DRAIN — this can leave the bus in a state that's difficult to recover from.

## Display Power Gate

The CO5300 AMOLED display's VCI power is gated by the XL9555 GPIO expander (VC_EN signal, confirmed from schematic sheet 3). The display will retain its last frame buffer content as long as ALDO2 (display power rail) stays on from the battery.

The XL9555 pin mapping for VC_EN needs verification — the BHI260AP GPIO enum values (6, 14, 15) used by LilyGoLib may not directly correspond to XL9555 port pin numbers. The schematic (sheet 3) shows the exact wiring.

## GPS Module

The MIA-M10Q (u-blox) outputs standard NMEA at 38400 baud by default. No vendor-specific init commands needed (unlike the L76K which requires PCAS commands). TinyGPSPlus parses the output without modification.

## RTC

The PCF85063A has time registers at offset 0x04-0x0A (vs PCF8563's 0x02-0x08). Same I2C address (0x51), similar BCD encoding, but different control register layout. The oscillator stop bit is at seconds register bit 7 (same as PCF8563).

## I2C Bus Architecture

All I2C devices share a single bus on GPIO 3 (SDA) and GPIO 2 (SCL) with 2.2K pull-up resistors to VDD3V3. The bus routes through the LCD connector (pins 1-2), meaning the display flex cable must be properly seated for I2C to function.

The I2C bus is used by: AXP2101 (PMU), PCF85063A (RTC), CST9217 (touch), BHI260AP (IMU), DRV2605 (haptic), XL9555 (GPIO expander).
