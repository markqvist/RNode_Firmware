# R-Watch: Off-Grid Smart Watch

## Vision

The T-Watch Ultimate port turns the RNode firmware inside out. Where the T-Beam Supreme is a radio modem that gained GPS tracking, the R-Watch is a smart watch that gained off-grid communications. The primary interface is a watch face on your wrist. The LoRa radio, GPS, and Reticulum stack run underneath — always available, never in the way.

The target user puts on a watch in the morning and gets the time, the date, their step count. When they walk beyond cellular range, the same watch becomes a LoRa mesh node: relaying packets for Sideband on their phone, beaconing their location, and buzzing when an LXMF message arrives over the air.

This aligns with the RNode project's core values — sovereignty, self-replication, open hardware — while making Reticulum accessible to people who don't configure radio modems.

## Hardware Platform

**LilyGo T-Watch Ultra**

| Component | Chip | RNode Support |
|-----------|------|---------------|
| MCU | ESP32-S3 (dual-core, 16MB flash, 8MB PSRAM) | Existing |
| LoRa radio | SX1262 (sub-GHz) | Existing (`sx126x.cpp/h`) |
| GPS | u-blox MIA-M10Q (GPS/GLONASS/BeiDou/Galileo) | Adapt (`GPS.h`, 38400 baud) |
| PMU | AXP2101 | Existing (`Power.h`) |
| BLE | 5.0 (ESP32-S3 native) | Existing (`BLESerial.h`) |
| WiFi | 802.11 b/g/n | Existing |
| Display | 2.06" AMOLED, 410x502, CO5300 QSPI | **New driver needed** |
| Touch | CST9217 capacitive (I2C 0x1A) | **New driver needed** |
| IMU | BHI260AP (I2C 0x28) | **New driver needed** |
| RTC | PCF85063A (I2C 0x51) | Adapt (`RTC.h`) |
| NFC | ST25R3916 (SPI) | **New driver needed** |
| Audio | MAX98357A amp + SPM1423 PDM mic | **New driver needed** |
| Haptics | DRV2605 (I2C 0x5A) | **New driver needed** |
| GPIO expander | XL9555 (I2C 0x20) | **New driver needed** |
| SD card | MicroSD via shared SPI | Existing pattern |

## Use Cases

In priority order:

### 1. Watch Face and Timekeeping

The default screen. Time, date, battery level. The RTC syncs from GPS satellites — no phone or internet needed for accurate time. This is what users see 95% of the time.

### 2. Phone-Connected BLE RNode

A phone running Sideband or NomadNet connects to the watch over BLE. The watch becomes a transparent LoRa modem — identical to plugging a T-Beam into USB, but wireless and on your wrist. The phone handles Reticulum routing; the watch handles the radio. The existing KISS-over-BLE protocol works without modification.

### 3. RNode Status Dashboard

While the radio is active, the watch shows what's happening: link quality, channel utilization, current frequency and spreading factor, BLE connection state. Glanceable, not diagnostic. A signal-strength arc on the watch face periphery, not a terminal dump.

### 4. Standalone GPS Tracker

When no phone has connected for 15 seconds, the watch switches to autonomous mode: GPS beacon transmission and encrypted LXMF telemetry, identical to the T-Beam Supreme's standalone operation. The watch face shows GPS fix status, satellite count, and time since last beacon. Location and battery telemetry reach the Reticulum network without any other device.

### 5. On-Watch Message Notifications

When LXMF messages arrive — either direct over LoRa or relayed from a connected phone — the watch displays them and vibrates. Read them on your wrist. This is new functionality: no existing RNode shows message content.

### 6. Activity Tracking

Step counting from the accelerometer. Basic daily activity. Table-stakes for a device on your wrist, and the hardware supports it.

## UI/UX Philosophy

### Watch Face Is Home

The always-visible screen is a clock. Not a radio diagnostic panel. The 410x502 AMOLED is tall enough to show time prominently with status indicators below: a thin bar for battery, a small icon for BLE connection, a dot for GPS fix, a signal indicator for LoRa activity. Information at a glance without cluttering the time.

### Tall Display Layout

The 410x502 display is a tall rounded rectangle — more vertical space than a typical square watch. Time occupies the upper portion. Status indicators, complications, and secondary information use the lower area. The extra vertical space is an asset for message display and scrollable lists.

### Gesture Navigation

Swipe to navigate between screens. Tap to select. Long-press for context. No reliance on physical buttons for primary navigation. The screen hierarchy:

```
          [Radio Status]
                |
[GPS/Location] -- [Watch Face] -- [Messages]
                |
           [Settings]
```

Swipe left/right/up/down from the watch face to reach each screen. Each screen is designed for glanceable information — two seconds of attention, not ten.

### Dark by Default

AMOLED means black pixels are free. Dark themes with minimal lit pixels extend battery life. Bright elements are reserved for alerts and active indicators.

### Wrist-Raise Wake

The accelerometer detects wrist raise and wakes the display. No button press to check the time. When the wrist drops, the display sleeps. Simple, expected watch behaviour.

## RNode Integration Model

### Dual-Core Architecture

The ESP32-S3 has two cores. Use both:

- **Core 0 — Radio modem**: SX1262 driver, KISS framing, CSMA, packet queues, BLE serial, GPS parsing, beacon logic. This is the existing RNode firmware loop, running as a FreeRTOS task. It must never be starved by display rendering.

- **Core 1 — Watch UI**: Display rendering, touch input, gesture recognition, animations, screen transitions. This is new code, running independently. It reads shared state (radio status, GPS coordinates, battery level, message buffers) from Core 0 via thread-safe queues.

This separation means radio performance is identical to a headless RNode regardless of what the display is doing.

### Operating Modes

| Feature | Phone Connected (BLE) | Standalone |
|---------|----------------------|------------|
| Watch face / timekeeping | Yes | Yes |
| LoRa radio | Host-controlled via KISS | Autonomous beacon |
| GPS tracking | Reported to phone | Local beacon + LXMF |
| Message display | Relayed from Sideband | Direct LoRa receive |
| Message send | Via phone | Future |
| Activity tracking | Yes | Yes |
| Provisioning | Via phone or USB | Pre-provisioned |

Mode switching is automatic. When a BLE host connects, the watch becomes a transparent modem. When the host disconnects, the watch enters standalone mode after a 15-second timeout. The watch face reflects the current mode.

## Feature Tiers

### Tier 1 — First Flash

Get the watch running as an RNode with a visible clock.

- Board definition in `Boards.h` with T-Watch Ultimate pin mapping
- AMOLED display driver (CO5300, 410x502 QSPI)
- Touch input driver (basic tap and swipe)
- Watch face: time, date, battery percentage
- GPS-synced RTC timekeeping
- BLE RNode modem mode (existing KISS/BLE stack)
- AXP2101 power management
- Radio status indicator on watch face (connected / idle / TX / RX)
- BLE connection indicator
- Provisioning via `rnodeconf`
- JTAG flash target in Makefile (OpenOCD, no serial bootloader)

### Tier 2 — Core Watch

Feature parity with T-Beam Supreme, plus watch-native status display.

- Standalone GPS beacon + LXMF encrypted telemetry
- Radio status screen (frequency, SF, BW, channel utilization, RSSI)
- GPS status screen (coordinates, satellites, HDOP, fix age, minimap)
- Wrist-raise display wake via accelerometer
- Haptic feedback on radio events (message received, beacon sent)
- Display sleep/wake power management
- Battery usage optimisation (AMOLED sleep, peripheral power gating)

### Tier 3 — Smart Watch

Features that make it a daily-wear device.

- On-watch LXMF message display with haptic notification
- Step counter and daily activity tracking
- Multiple watch face designs
- Alarm and timer functions
- Settings screen (LoRa parameters, BLE pairing, display brightness)
- Audio alerts via speaker for critical events (emergency beacon ACK, low battery)

### Tier 4 — Aspirational

Longer-term possibilities.

- On-watch LXMF message composition (touch keyboard or canned responses)
- Mesh network visualisation (nearby nodes, link quality graph)
- Peer discovery and contact list
- Over-the-air firmware updates via BLE or WiFi
- Watch face customisation

## Graphics and Artwork

### Framework: LVGL

The UI is built on [LVGL](https://lvgl.io/) (Light and Versatile Graphics Library), which is MIT-licensed and fully compatible with RNode's GPL-3.0. LVGL provides gesture-driven input, smooth animations, and efficient PSRAM-backed rendering on ESP32-S3. It is the standard choice for embedded touchscreen devices.

### Original Artwork

All watch face designs, icons, and graphical assets are original work created for the R-Watch project. LilyGo's factory firmware artwork is not reused — their repositories do not clearly license graphical assets separately from code, making reuse in a GPL project legally ambiguous.

Design inspiration may be drawn from the factory UI's layout and interaction patterns (ideas are not copyrightable), but no bitmap assets, watch face graphics, or icon sets are copied.

### Visual Identity

The R-Watch visual language should:

- Feel like a watch, not an electronics project. Clean typography, considered spacing, purposeful use of colour.
- Use the AMOLED's strengths: true blacks, vibrant accent colours against dark backgrounds, thin luminous arcs and indicators.
- Incorporate Reticulum/RNode identity subtly — the mesh network aesthetic should inform the design without dominating it. A watch face, not a dashboard.
- Prioritise readability at arm's length. Large time digits, high-contrast status indicators, no fine text that requires squinting.

### Asset Licensing

Code is GPL-3.0, consistent with the RNode project. Original graphical assets (watch faces, icons, UI elements) are licensed under Creative Commons Attribution-ShareAlike 4.0 (CC-BY-SA-4.0), allowing community contribution and remixing while preserving share-alike terms.

## Technical Approach

### What We Reuse

The following existing RNode firmware modules compile for the T-Watch with a new board definition and pin mapping — no architectural changes:

- `sx126x.cpp/h` — SX1262 LoRa driver
- `GPS.h` — GPS with TinyGPSPlus parser (adapt baud rate for MIA-M10Q: 38400 vs L76K's 9600)
- `Beacon.h` — GPS beacon transmission
- `LxmfBeacon.h` — LXMF telemetry messaging
- `BeaconCrypto.h` — ECDH + AES-256-CBC encryption
- `BLESerial.h` — BLE GATT serial interface
- `Bluetooth.h` — BLE pairing and state management
- `Power.h` — AXP2101 PMU driver (same chip as T-Beam Supreme)
- `RTC.h` — RTC driver (adapt register map for PCF85063A vs T-Beam's PCF8563)
- `Config.h` — Configuration state and KISS protocol
- `Modem.h` — Radio abstraction layer
- `Framing.h` — KISS framing

### What We Build

- **AMOLED display driver** — CO5300 controller over QSPI (4 data lines). The 410x502 display at 16-bit colour requires DMA-driven QSPI and a PSRAM-backed framebuffer. The UI layer is built on LVGL (MIT-licensed), which handles gesture input and efficient rendering on ESP32-S3 with PSRAM.

- **Touch input system** — Replaces the GPIO button debounce in `Input.h`. Capacitive touch over I2C, with gesture recognition (tap, swipe direction, long-press).

- **Watch UI layer** — The watch face, status screens, message display, and settings. This is the largest piece of new code. It runs on Core 1 and reads shared state from the modem task on Core 0.

- **Board definition** — `BOARD_TWATCH_ULT` in `Boards.h` with pin mapping, feature flags, and display parameters.

- **Makefile target** — `firmware-twatch_ultimate` using `esp32:esp32:esp32s3:CDCOnBoot=cdc`, flashed via OpenOCD JTAG (the T-Watch's native USB doesn't support esptool auto-reset).

## What This Is Not

- **Not Android or WearOS.** This is bare-metal firmware on an ESP32-S3. No app store, no Play Services, no Wear compatibility. The trade-off is sovereignty and radio capability.

- **Not a phone replacement.** The phone running Sideband remains the primary Reticulum host. The watch is the radio and the display, not the router.

- **Not a general-purpose LoRa development board.** It is a watch. The LoRa radio serves the watch's communication features, not the other way around.
