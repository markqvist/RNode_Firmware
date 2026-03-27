// SPM1423 PDM Microphone Driver — Minimal audio input for T-Watch Ultra
// PDM input on I2S_NUM_0 (hardware constraint for PDM on ESP32-S3)

#ifndef MICROPHONE_H
#define MICROPHONE_H

#if BOARD_MODEL == BOARD_TWATCH_ULT

#include "driver/i2s.h"

#define MIC_CLK_PIN   17         // PDM clock (WS pin in I2S terms)
#define MIC_DAT_PIN   18         // PDM data input
#define MIC_I2S_PORT  I2S_NUM_0  // PDM only works on I2S0
#define MIC_SAMPLE_RATE 16000    // 16kHz max for PDM mic
#define MIC_BUF_SIZE  1024

static bool mic_ready = false;

bool mic_init() {
  i2s_config_t i2s_config = {};
  i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM);
  i2s_config.sample_rate = MIC_SAMPLE_RATE;
  i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT;
  i2s_config.communication_format = I2S_COMM_FORMAT_STAND_PCM_SHORT;
  i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  i2s_config.dma_buf_count = 6;
  i2s_config.dma_buf_len = 512;
  i2s_config.use_apll = true;

  i2s_pin_config_t pin_config = {};
  pin_config.bck_io_num = I2S_PIN_NO_CHANGE;
  pin_config.ws_io_num = MIC_CLK_PIN;
  pin_config.data_out_num = I2S_PIN_NO_CHANGE;
  pin_config.data_in_num = MIC_DAT_PIN;
  pin_config.mck_io_num = I2S_PIN_NO_CHANGE;

  if (i2s_driver_install(MIC_I2S_PORT, &i2s_config, 0, NULL) != ESP_OK) {
    return false;
  }
  if (i2s_set_pin(MIC_I2S_PORT, &pin_config) != ESP_OK) {
    i2s_driver_uninstall(MIC_I2S_PORT);
    return false;
  }

  mic_ready = true;
  return true;
}

void mic_end() {
  if (mic_ready) {
    i2s_driver_uninstall(MIC_I2S_PORT);
    mic_ready = false;
  }
}

// Read raw audio samples into buffer. Returns bytes read.
size_t mic_read(int16_t *buf, size_t samples) {
  if (!mic_ready) return 0;
  size_t bytes_read = 0;
  i2s_read(MIC_I2S_PORT, buf, samples * sizeof(int16_t), &bytes_read, portMAX_DELAY);
  return bytes_read / sizeof(int16_t);
}

// Get current audio level (RMS of a short sample). Returns 0-32767.
uint16_t mic_level() {
  if (!mic_ready) return 0;

  int16_t buf[256];
  size_t samples = mic_read(buf, 256);
  if (samples == 0) return 0;

  uint64_t sum_sq = 0;
  for (size_t i = 0; i < samples; i++) {
    int32_t s = buf[i];
    sum_sq += s * s;
  }
  return (uint16_t)sqrt((double)sum_sq / samples);
}

#endif // BOARD_MODEL == BOARD_TWATCH_ULT
#endif // MICROPHONE_H
