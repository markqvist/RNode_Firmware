// SharedSPI — global mutex for the shared SPI bus (pins MISO=33, MOSI=34, CLK=35)
//
// Users of this bus MUST acquire shared_spi_mutex before any SPI transaction
// and release it after. This prevents race conditions between:
//   - SX1262 LoRa radio (CS=36) — main loop, continuous polling
//   - SD card (CS=21) — USB MSC callbacks (TinyUSB task), IMU logger, screenshots
//   - ST25R3916 NFC (CS=4) — future, not yet implemented
//
// The CO5300 display uses a separate SPI3 bus and does NOT need this mutex.

#ifndef SHARED_SPI_H
#define SHARED_SPI_H

#include <freertos/semphr.h>

extern SemaphoreHandle_t shared_spi_mutex;

inline void shared_spi_init() {
    if (!shared_spi_mutex) {
        shared_spi_mutex = xSemaphoreCreateMutex();
    }
}

#endif // SHARED_SPI_H
