// Sensor Data Logger — streams all sensor data to SD card as CSV
// Start/stop via remote debug command 'L' or Settings screen toggle
//
// CSV format: timestamp_ms,type,d0,d1,d2,d3,d4,d5,d6,d7,d8
// Type: A=accel, G=gyro, M=mag, S=step, W=wrist_tilt, P=GPS, T=touch
// IMU values in raw int16 units, GPS in scaled integers
// Timestamp is millis() at time of callback

#ifndef IMULOGGER_H
#define IMULOGGER_H

#if BOARD_MODEL == BOARD_TWATCH_ULT && HAS_SD

#include <SD.h>
#include "SharedSPI.h"

// Ring buffer for sensor samples (stored in PSRAM)
// Tagged samples: type char + up to 9 int32 fields
struct sensor_sample_t {
    uint32_t timestamp;
    char type;              // A=accel, G=gyro, M=mag, S=step, W=wrist, P=gps, T=touch
    int32_t d[9];           // data fields (meaning depends on type)
};

#define IMU_LOG_BUF_SIZE 512  // samples before flush (~10s at 50Hz)
static sensor_sample_t *imu_log_buf = NULL;
static volatile uint32_t imu_log_head = 0;   // write position
static volatile uint32_t imu_log_tail = 0;   // read position
bool imu_logging = false;
static File imu_log_file;
uint32_t imu_log_samples = 0;
uint32_t imu_log_start_ms = 0;

// Push a tagged sample to the ring buffer (safe from callbacks)
static void sensor_log_push(char type, int32_t d0=0, int32_t d1=0, int32_t d2=0,
                             int32_t d3=0, int32_t d4=0, int32_t d5=0,
                             int32_t d6=0, int32_t d7=0, int32_t d8=0) {
    if (!imu_logging || !imu_log_buf) return;
    uint32_t next = (imu_log_head + 1) % IMU_LOG_BUF_SIZE;
    if (next == imu_log_tail) return;  // full
    sensor_sample_t &s = imu_log_buf[imu_log_head];
    s.timestamp = millis();
    s.type = type;
    s.d[0]=d0; s.d[1]=d1; s.d[2]=d2; s.d[3]=d3; s.d[4]=d4;
    s.d[5]=d5; s.d[6]=d6; s.d[7]=d7; s.d[8]=d8;
    imu_log_head = next;
}

// Sensor callbacks — push individual tagged samples
void imu_log_accel_cb(uint8_t sensor_id, uint8_t *data, uint32_t size, uint64_t *timestamp, void *user_data) {
    if (size >= 6) {
        int16_t ax = (int16_t)(data[0] | (data[1] << 8));
        int16_t ay = (int16_t)(data[2] | (data[3] << 8));
        int16_t az = (int16_t)(data[4] | (data[5] << 8));
        sensor_log_push('A', ax, ay, az);
    }
}

void imu_log_gyro_cb(uint8_t sensor_id, uint8_t *data, uint32_t size, uint64_t *timestamp, void *user_data) {
    if (size >= 6) {
        int16_t gx = (int16_t)(data[0] | (data[1] << 8));
        int16_t gy = (int16_t)(data[2] | (data[3] << 8));
        int16_t gz = (int16_t)(data[4] | (data[5] << 8));
        sensor_log_push('G', gx, gy, gz);
    }
}

void imu_log_mag_cb(uint8_t sensor_id, uint8_t *data, uint32_t size, uint64_t *timestamp, void *user_data) {
    if (size >= 6) {
        int16_t mx = (int16_t)(data[0] | (data[1] << 8));
        int16_t my = (int16_t)(data[2] | (data[3] << 8));
        int16_t mz = (int16_t)(data[4] | (data[5] << 8));
        sensor_log_push('M', mx, my, mz);
    }
}

// Log step counter event
void sensor_log_step(uint32_t count) {
    sensor_log_push('S', (int32_t)count);
}

// Log wrist tilt event
void sensor_log_wrist_tilt() {
    sensor_log_push('W');
}

// Log GPS fix (call at 1Hz from main loop when logging)
void sensor_log_gps(double lat, double lon, double alt, double speed, double hdop, uint8_t sats) {
    sensor_log_push('P',
        (int32_t)(lat * 1e6), (int32_t)(lon * 1e6), (int32_t)(alt * 10),
        (int32_t)(speed * 100), (int32_t)(hdop * 100), sats);
}

// Log touch event
void sensor_log_touch(int16_t x, int16_t y, bool pressed) {
    sensor_log_push('T', x, y, pressed ? 1 : 0);
}

// Forward declaration
void imu_log_flush();

bool imu_log_start(SensorBHI260AP *bhi) {
    if (imu_logging || !bhi) return false;

    // Allocate ring buffer in PSRAM
    if (!imu_log_buf) {
        imu_log_buf = (sensor_sample_t *)heap_caps_malloc(
            IMU_LOG_BUF_SIZE * sizeof(sensor_sample_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
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
    imu_log_file.println("ms,type,d0,d1,d2,d3,d4,d5,d6,d7,d8");

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

    char line[128];
    uint32_t flushed = 0;
    while (imu_log_tail != imu_log_head) {
        sensor_sample_t &s = imu_log_buf[imu_log_tail];
        int len = snprintf(line, sizeof(line), "%lu,%c,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld\n",
                           s.timestamp, s.type,
                           s.d[0], s.d[1], s.d[2], s.d[3], s.d[4],
                           s.d[5], s.d[6], s.d[7], s.d[8]);
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
