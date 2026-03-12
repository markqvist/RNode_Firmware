# GPS Beacon Mode

RNode devices with GPS hardware (e.g. T-Beam Supreme, Heltec V4) can autonomously transmit GPS position beacons over LoRa when no host computer is connected. This is useful for vehicle tracking, field asset monitoring, or any scenario where the RNode operates standalone on battery power.

## Quick start

Minimum steps to get a beacon transmitting and a receiver collecting data:

```
1. Build & flash firmware     make firmware-tbeam_supreme && make upload-tbeam_supreme
2. Install rnlog              cd rns-collector && pip install .
3. Provision collector key    rnlog provision-lxmf --dest <SIDEBAND_HASH> --port /dev/ttyACM0
4. Provision IFAC key         rnlog provision-ifac --name helv4net --passphrase 'R3ticulum-priv8-m3sh' --port /dev/ttyACM0
5. Start receiver             rnlog serve
6. Disconnect USB & wait      RNode enters beacon mode after 15s of no host activity
```

The rest of this document explains each step, the beacon modes, and how to verify things are working.

## How it works

When no KISS host activity is detected for 15 seconds, the RNode enters beacon mode and transmits a GPS position every 30 seconds. When a host reconnects (e.g. laptop running `rnsd`), beaconing stops automatically and normal RNode operation resumes.

## Beacon modes

There are two beacon paths. The firmware selects automatically based on what has been provisioned.

### LXMF beacon (recommended)

When an LXMF identity and collector key are provisioned, the RNode:

1. Sends RNS **announce** packets (so the receiver learns the device's identity)
2. Sends **LXMF messages** with `FIELD_TELEMETRY` containing lat, lon, alt, speed, battery
3. Messages are encrypted per-packet with X25519 ECDH + AES-256-CBC + HMAC

These are proper Reticulum announces and LXMF messages — the receiver can be `rnsd`, Sideband, or any LXMF-aware application.

If IFAC is also provisioned, all packets are tagged with an authentication code before transmission. Receivers with matching `network_name`/`passphrase` accept the packets; others silently drop them.

### Legacy JSON beacon

Without LXMF provisioning, the RNode falls back to sending raw JSON payloads:

```json
{"lat":51.507400,"lon":-0.127800,"alt":15.0,"sat":8,"spd":0.5,"hdop":1.2,"bat":87,"fix":true}
```

- **Plaintext**: Sent to well-known PLAIN destination `rnlog.beacon`. Zero configuration needed, but anyone in range can read it.
- **Encrypted**: When provisioned with `rnlog provision` key, sent as SINGLE packets. Only the matching collector can decrypt.

## Hardware

### Tested boards

| Board | GPS | LoRa | Notes |
|-------|-----|------|-------|
| LilyGO T-Beam Supreme S3 | L76K (UART) | SX1262 | Best option. RTC, large battery connector |
| Heltec LoRa32 V4 | External (UART) | SX1262 | Needs external GPS module |

### Wiring (T-Beam Supreme)

No external wiring needed — GPS and LoRa are integrated. Connect via USB-C for provisioning and flashing.

## Building firmware

### Prerequisites

- Arduino CLI (installed by `make prep-esp32`)
- Python 3 with `pyserial`
- USB cable to the device

### Build and flash

```sh
cd RNode_Firmware

# First time: install Arduino cores and libraries
make prep-esp32

# Build
make firmware-tbeam_supreme

# Flash (device must be connected via USB)
make upload-tbeam_supreme PORT=/dev/ttyACM0
```

Other board targets: `firmware-tbeam`, `firmware-heltec32_v4`, `firmware-lora32_v21`, etc. Run `make` with no arguments to see available targets.

## Provisioning

Provisioning configures the RNode with cryptographic keys over USB. Keys are stored in NVS (ESP32) or EEPROM and persist across power cycles. You only need to do this once per device.

There are three independent provisioning steps. Each is optional but recommended:

### 1. LXMF identity + collector key

This gives the RNode an LXMF identity (Ed25519 keypair) and tells it where to send telemetry.

```sh
# Install rnlog if not already done
cd rns-collector && pip install .

# Provision (requires rnsd running with a path to the Sideband destination)
rnlog provision-lxmf --dest <SIDEBAND_DEST_HASH> --port /dev/ttyACM0
```

What this does:
- Resolves the Sideband destination hash via Reticulum to get its public key
- Sends the key material to the RNode via KISS `CMD_BCN_KEY` (0x86)
- The RNode generates an Ed25519 identity (stored in NVS) and prints its source hash
- Reads back the RNode's LXMF source hash via `CMD_LXMF_HASH` (0x87)

After provisioning, the OLED display shows an "LXMF Identity" page (page 5) with the source hash and target info.

### 2. IFAC authentication

If your receiver's RNodeInterface uses `network_name` and `passphrase` (IFAC), the beacon must send matching authentication tags or packets will be silently dropped.

```sh
rnlog provision-ifac \
  --name helv4net \
  --passphrase 'R3ticulum-priv8-m3sh' \
  --port /dev/ttyACM0
```

The `--name` and `--passphrase` must match the receiver's Reticulum config:

```ini
# Receiver's ~/.reticulum/config
[[LoRa 868 MHz]]
  type = RNodeInterface
  network_name = helv4net
  passphrase = R3ticulum-priv8-m3sh
  ...
```

What this does:
- Derives a 64-byte IFAC key: `HKDF-SHA256(SHA256(network_name) + SHA256(passphrase), IFAC_SALT, 64)`
- Sends to RNode via KISS `CMD_IFAC_KEY` (0x89)
- The RNode derives an Ed25519 signing keypair from the key and applies IFAC tags to all transmitted packets

### 3. Legacy collector key (alternative to LXMF)

If you don't need LXMF and just want encrypted JSON beacons:

```sh
rnlog provision
# Copy the 64-byte hex output, then send via CMD_BCN_KEY — see the Python
# snippet in the rns-collector README.
```

## Receiver setup

The receiver is any machine running `rnsd` with an RNode interface on the same LoRa parameters.

### Reticulum config

```ini
# ~/.reticulum/config
[reticulum]
  enable_transport = yes

[interfaces]
  [[LoRa 868 MHz]]
    type = RNodeInterface
    interface_enabled = True
    port = /dev/ttyACM0
    frequency = 868000000
    bandwidth = 125000
    spreadingfactor = 7
    txpower = 14
    # Optional but recommended — must match beacon provisioning:
    network_name = helv4net
    passphrase = R3ticulum-priv8-m3sh
```

### Start the receiver

```sh
# Start rnsd (if not already running as a service)
rnsd

# In another terminal, start the telemetry collector
rnlog serve
```

Or to also relay GPS data to a Sideband app:

```sh
rnlog serve --sideband-dest <SIDEBAND_DEST_HASH>
```

### Query collected data

```sh
# Recent readings
rnlog query -n 10

# Last hour, as JSON
rnlog -j query -s 1h

# Database summary
rnlog summary

# Export to CSV
rnlog export -f csv > telemetry.csv
```

## Testing and verification

### USB test (device connected)

Trigger a test beacon over USB without waiting for the 30s interval:

```sh
rnlog test-lxmf --port /dev/ttyACM0
```

This sends `CMD_LXMF_TEST` (0x88) to the RNode, which immediately transmits an announce + LXMF beacon and emits the pre-encryption plaintext as `CMD_DIAG` frames back over USB. The test tool validates:

- Announce packet structure and Ed25519 signature
- LXMF message structure and Fernet decryption
- FIELD_TELEMETRY parsing (lat, lon, alt, speed, battery)

### Over-the-air verification

To verify the receiver is accepting IFAC-authenticated packets:

```python
import RNS

reticulum = RNS.Reticulum()
target = bytes.fromhex("YOUR_RNODE_DEST_HASH")  # from provision-lxmf output

# Check if the receiver has seen the announce
identity = RNS.Identity.recall(target)
if identity:
    print(f"Identity known: {identity}")
else:
    print("Not yet received — trigger a test beacon or wait for next announce")
```

### Checking the OLED display

The T-Beam Supreme has 6 display pages (cycle with the button):

| Page | Content |
|------|---------|
| 0-1 | Radio diagnostics (standard RNode) |
| 2-4 | GPS (coordinates, satellites, altitude) |
| 5 | LXMF status (identity hash, target, announce/beacon timing) |

When LXMF is active, page 2 shows `LX` instead of `BCN` next to the satellite count.

## Radio parameters

Beacons use fixed LoRa parameters that must match the receiver's RNode interface:

| Parameter        | Value   |
|------------------|---------|
| Frequency        | 868 MHz |
| Bandwidth        | 125 kHz |
| Spreading Factor | 7       |
| Coding Rate      | 4/5     |
| TX Power         | 17 dBm  |

These are set in `Beacon.h`. If your receiver uses different radio parameters, update the `BEACON_*` defines and rebuild.

## Timing

| Parameter                | Default | Define                      |
|--------------------------|---------|-----------------------------|
| Beacon interval          | 30s     | `BEACON_INTERVAL_MS`        |
| Announce interval        | 10min   | `LXMF_ANNOUNCE_INTERVAL_MS` |
| Startup delay            | 10s     | `BEACON_STARTUP_DELAY_MS`   |
| Host inactivity timeout  | 15s     | `BEACON_NO_HOST_TIMEOUT_MS` |

## Packet sizes

| Mode                | Header | Payload              | Total  |
|---------------------|--------|----------------------|--------|
| Plaintext JSON      | 19B    | ~93B JSON            | ~112B  |
| Encrypted JSON      | 19B    | 32+16+96+32 = 176B   | ~195B  |
| LXMF announce       | 2B     | 183B (+ 8B IFAC tag) | ~193B  |
| LXMF beacon         | 2B     | ~250B encrypted      | ~260B  |

All fit within the RNS MTU of 508 bytes.

## Troubleshooting

### Receiver doesn't see any packets

1. **Radio mismatch**: Verify frequency, bandwidth, and spreading factor match between beacon and receiver
2. **IFAC mismatch**: If the receiver has `network_name`/`passphrase` set, the beacon must be provisioned with matching IFAC key. Packets without valid IFAC are silently dropped — no error is logged
3. **Range**: LoRa SF7 at 868 MHz has limited range in urban environments. Try line-of-sight first

### `rnlog provision-lxmf` fails to resolve destination

The Sideband destination must be reachable via Reticulum. Ensure `rnsd` is running and has a path to the destination (either direct or via transport nodes).

### `has_path()` returns False but identity is recalled

This is expected. `has_path()` checks the RNS path table (populated by Transport routing), while `Identity.recall()` checks `known_destinations` (populated when any announce is validated). For local interfaces, the path table entry may not be created, but the identity is still usable.

### Device shows "GPS searching" indefinitely

The T-Beam Supreme GPS needs clear sky view for first fix. Cold start takes 30-60 seconds outdoors, longer indoors. The RTC retains time across reboots once synced, speeding subsequent fixes.

## Architecture

```
┌─────────────────────────┐        LoRa 868 MHz        ┌────────────────────────┐
│  T-Beam Supreme         │  ──────────────────────►    │  Receiver              │
│                         │   IFAC-tagged packets       │  (rnsd + RNode)        │
│  GPS ──► Beacon.h       │                             │                        │
│           ├─ announce    │   Announce (185B + 8B IFAC) │  rnsd validates IFAC   │
│           └─ LXMF msg   │   LXMF    (250B + 8B IFAC) │  ├─ known_destinations │
│                         │                             │  └─ LXMF delivery      │
│  Provisioned via USB:   │                             │                        │
│  - Collector key (64B)  │                             │  rnlog serve           │
│  - IFAC key (64B)       │                             │  ├─ SQLite database    │
│  - LXMF identity (NVS)  │                             │  └─ Sideband relay     │
└─────────────────────────┘                             └────────────────────────┘
```

## Files

| File             | Purpose                                               |
|------------------|-------------------------------------------------------|
| `Beacon.h`       | Beacon state machine, LXMF/JSON path selection        |
| `BeaconCrypto.h` | X25519 ECDH (libsodium), HKDF, AES-256-CBC, HMAC     |
| `LxmfBeacon.h`   | LXMF identity, announce construction, beacon messages |
| `IfacAuth.h`     | IFAC key storage (NVS), Ed25519 signing, tag masking  |
| `GPS.h`          | GPS parsing (TinyGPS++)                               |
| `ROM.h`          | EEPROM addresses for beacon key storage               |
| `Framing.h`      | KISS command definitions (CMD_BCN_KEY, CMD_IFAC_KEY, etc.) |
| `Display.h`      | OLED display pages including LXMF status              |
