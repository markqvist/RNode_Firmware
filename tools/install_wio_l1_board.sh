#!/usr/bin/env bash
set -euo pipefail

ARDUINO_DATA_DIR="${ARDUINO_DATA_DIR:-$HOME/Library/Arduino15}"
CORE_BASE="${ARDUINO_DATA_DIR}/packages/Seeeduino/hardware/nrf52"

if [[ ! -d "${CORE_BASE}" ]]; then
  echo "Seeeduino nrf52 core not found in ${CORE_BASE}"
  echo "Install it with: arduino-cli core install Seeeduino:nrf52"
  exit 1
fi

CORE_DIR="$(ls -d "${CORE_BASE}"/* | sort -V | tail -n1)"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VARIANT_SRC="${SCRIPT_DIR}/../boards/seeed_wio_tracker_L1"
VARIANT_DST="${CORE_DIR}/variants/seeed_wio_tracker_L1"
BOARDS_TXT="${CORE_DIR}/boards.txt"

mkdir -p "${VARIANT_DST}"
cp "${VARIANT_SRC}/variant.h" "${VARIANT_DST}/variant.h"
cp "${VARIANT_SRC}/variant.cpp" "${VARIANT_DST}/variant.cpp"

tmp_boards="$(mktemp)"
awk '
  /^# WIO_L1_BEGIN/ {skip=1; next}
  /^# WIO_L1_END/ {skip=0; next}
  skip==1 {next}
  /^seeed_wio_tracker_L1\./ {next}
  {print}
' "${BOARDS_TXT}" > "${tmp_boards}"
mv "${tmp_boards}" "${BOARDS_TXT}"

cp "${BOARDS_TXT}" "${BOARDS_TXT}.bak"
cat >> "${BOARDS_TXT}" <<'EOF'

# WIO_L1_BEGIN
seeed_wio_tracker_L1.name=Seeed Wio Tracker L1

seeed_wio_tracker_L1.vid.0=0x2886
seeed_wio_tracker_L1.pid.0=0x1668
seeed_wio_tracker_L1.vid.1=0x2886
seeed_wio_tracker_L1.pid.1=0x1667

seeed_wio_tracker_L1.bootloader.tool=bootburn
seeed_wio_tracker_L1.upload.tool=nrfutil
seeed_wio_tracker_L1.upload.protocol=nrfutil
seeed_wio_tracker_L1.upload.use_1200bps_touch=true
seeed_wio_tracker_L1.upload.wait_for_upload_port=true
seeed_wio_tracker_L1.upload.maximum_size=815104
seeed_wio_tracker_L1.upload.maximum_data_size=248832

seeed_wio_tracker_L1.build.mcu=cortex-m4
seeed_wio_tracker_L1.build.f_cpu=64000000
seeed_wio_tracker_L1.build.board=Seeed_Wio_Tracker_L1
seeed_wio_tracker_L1.build.core=nRF5
seeed_wio_tracker_L1.build.variant=seeed_wio_tracker_L1
seeed_wio_tracker_L1.build.usb_manufacturer="Seeed"
seeed_wio_tracker_L1.build.usb_product="Wio Tracker L1"
seeed_wio_tracker_L1.build.extra_flags=-DNRF52840_XXAA -DARDUINO_WIO_TRACKER_1110 -DARDUINO_MDBT50Q_RX {build.flags.usb}
seeed_wio_tracker_L1.build.ldscript=nrf52840_s140_v7.ld
seeed_wio_tracker_L1.build.vid=0x2886
seeed_wio_tracker_L1.build.pid=0x1668

seeed_wio_tracker_L1.menu.softdevice.s140v6=S140 7.3.0
seeed_wio_tracker_L1.menu.softdevice.s140v6.build.sd_name=s140
seeed_wio_tracker_L1.menu.softdevice.s140v6.build.sd_version=7.3.0
seeed_wio_tracker_L1.menu.softdevice.s140v6.build.sd_fwid=0x0123

seeed_wio_tracker_L1.menu.debug.l0=Level 0 (Release)
seeed_wio_tracker_L1.menu.debug.l0.build.debug_flags=-DCFG_DEBUG=0
seeed_wio_tracker_L1.menu.debug.l1=Level 1 (Error Message)
seeed_wio_tracker_L1.menu.debug.l1.build.debug_flags=-DCFG_DEBUG=1
seeed_wio_tracker_L1.menu.debug.l2=Level 2 (Full Debug)
seeed_wio_tracker_L1.menu.debug.l2.build.debug_flags=-DCFG_DEBUG=2
seeed_wio_tracker_L1.menu.debug.l3=Level 3 (Segger SystemView)
seeed_wio_tracker_L1.menu.debug.l3.build.debug_flags=-DCFG_DEBUG=3
seeed_wio_tracker_L1.menu.debug.l3.build.sysview_flags=-DCFG_SYSVIEW=1
# WIO_L1_END
EOF

echo "Installed Wio Tracker L1 variant into ${CORE_DIR}"
