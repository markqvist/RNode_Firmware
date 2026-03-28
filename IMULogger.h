// IMU Data Logger — streams BHI260AP sensor data to SD card as CSV
// Start/stop via remote debug command 'L' or long-press button
//
// CSV format: timestamp_ms,ax,ay,az,gx,gy,gz,mx,my,mz
// Accelerometer/gyro/magnetometer in raw int16 units
// Timestamp is millis() at time of callback

#ifndef IMULOGGER_H
#define IMULOGGER_H

#if BOARD_MODEL == BOARD_TWATCH_ULT && HAS_SD

#include <SD.h>
#include "SharedSPI.h"

// Ring buffer for sensor samples (stored in PSRAM)
struct imu_sample_t {
    uint32_t timestamp;
    int16_t ax, ay, az;     // accelerometer
    int16_t gx, gy, gz;     // gyroscope
    int16_t mx, my, mz;     // magnetometer
};

#define IMU_LOG_BUF_SIZE 512  // samples before flush (~10s at 50Hz)
static imu_sample_t *imu_log_buf = NULL;
static volatile uint32_t imu_log_head = 0;   // write position
static volatile uint32_t imu_log_tail = 0;   // read position
static bool imu_logging = false;
static File imu_log_file;
static uint32_t imu_log_samples = 0;
static uint32_t imu_log_start_ms = 0;

// Latest raw values (written by callbacks, read by flush)
static volatile int16_t imu_raw_ax = 0, imu_raw_ay = 0, imu_raw_az = 0;
static volatile int16_t imu_raw_gx = 0, imu_raw_gy = 0, imu_raw_gz = 0;
static volatile int16_t imu_raw_mx = 0, imu_raw_my = 0, imu_raw_mz = 0;
static volatile bool imu_accel_new = false;

// Sensor callbacks — store latest values
void imu_log_accel_cb(uint8_t sensor_id, uint8_t *data, uint32_t size, uint64_t *timestamp, void *user_data) {
    if (size >= 6) {
        imu_raw_ax = (int16_t)(data[0] | (data[1] << 8));
        imu_raw_ay = (int16_t)(data[2] | (data[3] << 8));
        imu_raw_az = (int16_t)(data[4] | (data[5] << 8));
        imu_accel_new = true;

        // Push combined sample to ring buffer when accel fires (it's the "clock")
        if (imu_logging && imu_log_buf) {
            uint32_t next = (imu_log_head + 1) % IMU_LOG_BUF_SIZE;
            if (next != imu_log_tail) {  // not full
                imu_sample_t &s = imu_log_buf[imu_log_head];
                s.timestamp = millis();
                s.ax = imu_raw_ax; s.ay = imu_raw_ay; s.az = imu_raw_az;
                s.gx = imu_raw_gx; s.gy = imu_raw_gy; s.gz = imu_raw_gz;
                s.mx = imu_raw_mx; s.my = imu_raw_my; s.mz = imu_raw_mz;
                imu_log_head = next;
            }
        }
    }
}

void imu_log_gyro_cb(uint8_t sensor_id, uint8_t *data, uint32_t size, uint64_t *timestamp, void *user_data) {
    if (size >= 6) {
        imu_raw_gx = (int16_t)(data[0] | (data[1] << 8));
        imu_raw_gy = (int16_t)(data[2] | (data[3] << 8));
        imu_raw_gz = (int16_t)(data[4] | (data[5] << 8));
    }
}

void imu_log_mag_cb(uint8_t sensor_id, uint8_t *data, uint32_t size, uint64_t *timestamp, void *user_data) {
    if (size >= 6) {
        imu_raw_mx = (int16_t)(data[0] | (data[1] << 8));
        imu_raw_my = (int16_t)(data[2] | (data[3] << 8));
        imu_raw_mz = (int16_t)(data[4] | (data[5] << 8));
    }
}

// Forward declaration
void imu_log_flush();

bool imu_log_start(SensorBHI260AP *bhi) {
    if (imu_logging || !bhi) return false;

    // Allocate ring buffer in PSRAM
    if (!imu_log_buf) {
        imu_log_buf = (imu_sample_t *)heap_caps_malloc(
            IMU_LOG_BUF_SIZE * sizeof(imu_sample_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!imu_log_buf) return false;
    }
    imu_log_head = 0;
    imu_log_tail = 0;

    // Init SD (acquire shared SPI mutex)
    if (shared_spi_mutex) xSemaphoreTake(shared_spi_mutex, portMAX_DELAY);
    SPI.begin(SD_CLK, SD_MISO, SD_MOSI, SD_CS);
    bool sd_ok = SD.begin(SD_CS, SPI, 4000000, "/sd", 5);
    if (shared_spi_mutex) xSemaphoreGive(shared_spi_mutex);
    if (!sd_ok) {
        Serial.println("[imu_log] SD init failed");
        return false;
    }

    // Create timestamped filename
    char fname[32];
    snprintf(fname, sizeof(fname), "/imu_%lu.csv", millis() / 1000);
    imu_log_file = SD.open(fname, FILE_WRITE);
    if (!imu_log_file) {
        Serial.println("[imu_log] file open failed");
        SD.end(); SPI.end();
        return false;
    }

    // Write CSV header
    imu_log_file.println("ms,ax,ay,az,gx,gy,gz,mx,my,mz");

    // Configure sensors at 50Hz
    bhi->configure(SensorBHI260AP::ACCEL_PASSTHROUGH, 50.0, 0);
    bhi->onResultEvent(SensorBHI260AP::ACCEL_PASSTHROUGH, imu_log_accel_cb);

    bhi->configure(SensorBHI260AP::GYRO_PASSTHROUGH, 50.0, 0);
    bhi->onResultEvent(SensorBHI260AP::GYRO_PASSTHROUGH, imu_log_gyro_cb);

    bhi->configure(SensorBHI260AP::MAGNETOMETER_PASSTHROUGH, 25.0, 0);
    bhi->onResultEvent(SensorBHI260AP::MAGNETOMETER_PASSTHROUGH, imu_log_mag_cb);

    imu_logging = true;
    imu_log_samples = 0;
    imu_log_start_ms = millis();
    Serial.printf("[imu_log] started: %s\n", fname);
    return true;
}

void imu_log_stop(SensorBHI260AP *bhi) {
    if (!imu_logging) return;

    // Disable sensor streams
    if (bhi) {
        bhi->configure(SensorBHI260AP::ACCEL_PASSTHROUGH, 0, 0);
        bhi->configure(SensorBHI260AP::GYRO_PASSTHROUGH, 0, 0);
        bhi->configure(SensorBHI260AP::MAGNETOMETER_PASSTHROUGH, 0, 0);
    }

    // Flush remaining samples
    imu_log_flush();

    uint32_t duration = (millis() - imu_log_start_ms) / 1000;
    Serial.printf("[imu_log] stopped: %lu samples in %lus (%.1f Hz)\n",
                  imu_log_samples, duration,
                  duration > 0 ? (float)imu_log_samples / duration : 0);

    if (shared_spi_mutex) xSemaphoreTake(shared_spi_mutex, portMAX_DELAY);
    imu_log_file.close();
    if (shared_spi_mutex) xSemaphoreGive(shared_spi_mutex);
    imu_logging = false;
}

// Flush ring buffer to SD — call from main loop
void imu_log_flush() {
    if (!imu_logging || !imu_log_buf) return;
    if (shared_spi_mutex && xSemaphoreTake(shared_spi_mutex, pdMS_TO_TICKS(50)) != pdTRUE) return;

    char line[80];
    uint32_t flushed = 0;
    while (imu_log_tail != imu_log_head) {
        imu_sample_t &s = imu_log_buf[imu_log_tail];
        int len = snprintf(line, sizeof(line), "%lu,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
                           s.timestamp, s.ax, s.ay, s.az,
                           s.gx, s.gy, s.gz, s.mx, s.my, s.mz);
        imu_log_file.write((uint8_t *)line, len);
        imu_log_tail = (imu_log_tail + 1) % IMU_LOG_BUF_SIZE;
        imu_log_samples++;
        flushed++;
    }
    if (flushed > 0) {
        imu_log_file.flush();
    }
    if (shared_spi_mutex) xSemaphoreGive(shared_spi_mutex);
}

#endif // BOARD_MODEL == BOARD_TWATCH_ULT && HAS_SD
#endif // IMULOGGER_H
