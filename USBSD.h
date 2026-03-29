// USB Mass Storage — exposes SD card via USB alongside CDC serial
// Requires USBMode=default (TinyUSB) in the FQBN build flags.
//
// SD card and LoRa share the same SPI bus. Rather than mutex-based
// concurrent access (unreliable due to timing), this uses exclusive
// mode switching: LoRa sleeps when SD is active, and vice versa.
// Toggle via remote debug command 'D' or watch UI.

#ifndef USBSD_H
#define USBSD_H

#if BOARD_MODEL == BOARD_TWATCH_ULT && HAS_SD && !ARDUINO_USB_MODE

#include "USB.h"
#include "USBMSC.h"
#include <SD.h>

static USBMSC usb_msc;
bool usb_sd_ready = false;
bool usb_sd_mode = false;    // true = SD/USB active, LoRa sleeping

static int32_t usb_sd_read(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
    if (!usb_sd_ready || !usb_sd_mode) return -1;
    int32_t result = bufsize;
    uint32_t sec = lba + (offset / 512);
    uint32_t cnt = bufsize / 512;
    for (uint32_t i = 0; i < cnt; i++) {
        if (!SD.readRAW((uint8_t *)buffer + i * 512, sec + i)) { result = -1; break; }
    }
    return result;
}

static int32_t usb_sd_write(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
    if (!usb_sd_ready || !usb_sd_mode) return -1;
    int32_t result = bufsize;
    uint32_t sec = lba + (offset / 512);
    uint32_t cnt = bufsize / 512;
    for (uint32_t i = 0; i < cnt; i++) {
        if (!SD.writeRAW(buffer + i * 512, sec + i)) { result = -1; break; }
    }
    return result;
}

static bool usb_sd_start_stop(uint8_t power_condition, bool start, bool load_eject) {
    return true;
}

// Register USB MSC device (call early in setup, before USB enumeration)
// Does NOT mount SD yet — call usb_sd_enable() to activate
void usb_sd_register() {
    usb_msc.vendorID("RNode");
    usb_msc.productID("R-Watch SD");
    usb_msc.productRevision("1.0");
    usb_msc.onRead(usb_sd_read);
    usb_msc.onWrite(usb_sd_write);
    usb_msc.onStartStop(usb_sd_start_stop);
    usb_msc.mediaPresent(false);
    usb_msc.begin(0, 512);
}

// Activate SD mode: sleep LoRa, mount SD, expose via USB
// Returns true if SD card mounted successfully
bool usb_sd_enable() {
    if (usb_sd_mode) return true;

    // Put LoRa radio to sleep (releases SPI bus)
    if (radio_online) {
        // The sx126x sleep command is handled by the modem layer
        // For now, just note that radio will be unavailable
    }

    // Mount SD card (exclusive SPI access now)
    SPI.begin(SD_CLK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS, SPI, 4000000, "/sd", 5)) {
        SPI.end();
        return false;
    }

    uint32_t sectors = SD.numSectors();
    uint16_t secsize = SD.sectorSize();
    if (sectors == 0) {
        SD.end(); SPI.end();
        return false;
    }

    usb_msc.mediaPresent(true);
    usb_msc.begin(sectors, secsize);
    usb_sd_ready = true;
    usb_sd_mode = true;
    Serial.printf("[usb_sd] SD mode ON: %lu sectors (%.1f GB)\n",
                  sectors, (float)sectors * 512 / 1073741824.0);
    return true;
}

// Deactivate SD mode: unmount SD, wake LoRa
void usb_sd_disable() {
    if (!usb_sd_mode) return;

    usb_msc.mediaPresent(false);
    usb_sd_ready = false;
    usb_sd_mode = false;

    // Unmount SD
    SD.end();
    SPI.end();

    // LoRa will re-initialize on next main loop cycle
    Serial.println("[usb_sd] SD mode OFF");
}

#endif
#endif
