// USB Mass Storage — exposes SD card via USB alongside CDC serial
// Requires USBMode=default (TinyUSB) in the FQBN build flags.
// Uses shared_spi_mutex from SharedSPI.h to coordinate with LoRa radio.

#ifndef USBSD_H
#define USBSD_H

#if BOARD_MODEL == BOARD_TWATCH_ULT && HAS_SD && !ARDUINO_USB_MODE

#include "USB.h"
#include "USBMSC.h"
#include <SD.h>
#include "SharedSPI.h"

static USBMSC usb_msc;
bool usb_sd_ready = false;

uint32_t usb_sd_read_count = 0;
uint32_t usb_sd_read_fail = 0;

static int32_t usb_sd_read(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
    if (!usb_sd_ready) { usb_sd_read_fail++; return -1; }
    if (xSemaphoreTake(shared_spi_mutex, pdMS_TO_TICKS(500)) != pdTRUE) {
        usb_sd_read_fail++;
        return -1;
    }
    int32_t result = bufsize;
    uint32_t sec = lba + (offset / 512);
    uint32_t cnt = bufsize / 512;
    for (uint32_t i = 0; i < cnt; i++) {
        if (!SD.readRAW((uint8_t *)buffer + i * 512, sec + i)) {
            usb_sd_read_fail++;
            result = -1;
            break;
        }
    }
    xSemaphoreGive(shared_spi_mutex);
    usb_sd_read_count++;
    return result;
}

static int32_t usb_sd_write(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
    if (!usb_sd_ready) return -1;
    if (xSemaphoreTake(shared_spi_mutex, pdMS_TO_TICKS(200)) != pdTRUE) return -1;
    int32_t result = bufsize;
    uint32_t sec = lba + (offset / 512);
    uint32_t cnt = bufsize / 512;
    for (uint32_t i = 0; i < cnt; i++) {
        if (!SD.writeRAW(buffer + i * 512, sec + i)) { result = -1; break; }
    }
    xSemaphoreGive(shared_spi_mutex);
    return result;
}

static bool usb_sd_start_stop(uint8_t power_condition, bool start, bool load_eject) {
    return true;
}

bool usb_sd_init() {
    usb_msc.vendorID("RNode");
    usb_msc.productID("R-Watch SD");
    usb_msc.productRevision("1.0");
    usb_msc.onRead(usb_sd_read);
    usb_msc.onWrite(usb_sd_write);
    usb_msc.onStartStop(usb_sd_start_stop);

    // Init SD card — acquire mutex since LoRa may already be using the bus
    if (shared_spi_mutex) xSemaphoreTake(shared_spi_mutex, portMAX_DELAY);
    SPI.begin(SD_CLK, SD_MISO, SD_MOSI, SD_CS);
    bool ok = SD.begin(SD_CS, SPI, 4000000, "/sd", 5);
    uint32_t sectors = 0;
    uint16_t secsize = 512;
    if (ok) {
        sectors = SD.numSectors();
        secsize = SD.sectorSize();
    }
    if (shared_spi_mutex) xSemaphoreGive(shared_spi_mutex);

    if (!ok || sectors == 0) {
        usb_msc.mediaPresent(false);
        usb_msc.begin(0, 512);
        return false;
    }

    usb_msc.mediaPresent(true);
    usb_msc.begin(sectors, secsize);
    usb_sd_ready = true;
    return true;
}

#endif
#endif
