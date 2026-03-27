// MAX98357A I2S Speaker Driver — Minimal tone generator for T-Watch Ultra
// Plays simple alert tones via I2S. No WAV files needed.

#ifndef SPEAKER_H
#define SPEAKER_H

#if BOARD_MODEL == BOARD_TWATCH_ULT

#include "driver/i2s.h"
#include <math.h>

#define SPK_BCLK   I2S_BCLK
#define SPK_WCLK   I2S_WCLK
#define SPK_DOUT   I2S_DOUT
#define SPK_I2S_PORT I2S_NUM_0
#define SPK_SAMPLE_RATE 16000
#define SPK_TONE_BUF_SIZE 512

static bool speaker_ready = false;

bool speaker_init() {
  i2s_config_t i2s_config = {};
  i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  i2s_config.sample_rate = SPK_SAMPLE_RATE;
  i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  i2s_config.channel_format = I2S_CHANNEL_FMT_ALL_RIGHT;
  i2s_config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  i2s_config.dma_buf_count = 4;
  i2s_config.dma_buf_len = 512;
  i2s_config.use_apll = false;
  i2s_config.tx_desc_auto_clear = true;

  i2s_pin_config_t pin_config = {};
  pin_config.bck_io_num = SPK_BCLK;
  pin_config.ws_io_num = SPK_WCLK;
  pin_config.data_out_num = SPK_DOUT;
  pin_config.data_in_num = I2S_PIN_NO_CHANGE;

  if (i2s_driver_install(SPK_I2S_PORT, &i2s_config, 0, NULL) != ESP_OK) {
    return false;
  }
  if (i2s_set_pin(SPK_I2S_PORT, &pin_config) != ESP_OK) {
    i2s_driver_uninstall(SPK_I2S_PORT);
    return false;
  }

  speaker_ready = true;
  return true;
}

void speaker_end() {
  if (speaker_ready) {
    i2s_driver_uninstall(SPK_I2S_PORT);
    speaker_ready = false;
  }
}

// Play a tone at given frequency (Hz) for given duration (ms) at volume (0-100)
void speaker_tone(uint16_t freq, uint16_t duration_ms, uint8_t volume) {
  if (!speaker_ready || freq == 0) return;

  float vol = (float)volume / 100.0f;
  int16_t amplitude = (int16_t)(16000 * vol);  // ~half of int16 max
  uint32_t total_samples = (uint32_t)SPK_SAMPLE_RATE * duration_ms / 1000;
  int16_t buf[SPK_TONE_BUF_SIZE];

  uint32_t samples_written = 0;
  while (samples_written < total_samples) {
    uint32_t chunk = total_samples - samples_written;
    if (chunk > SPK_TONE_BUF_SIZE) chunk = SPK_TONE_BUF_SIZE;

    for (uint32_t i = 0; i < chunk; i++) {
      float t = (float)(samples_written + i) / (float)SPK_SAMPLE_RATE;
      buf[i] = (int16_t)(amplitude * sinf(2.0f * M_PI * freq * t));
    }

    size_t bytes_written = 0;
    i2s_write(SPK_I2S_PORT, buf, chunk * sizeof(int16_t), &bytes_written, portMAX_DELAY);
    samples_written += chunk;
  }

  // Brief silence to flush DMA buffer
  memset(buf, 0, sizeof(buf));
  size_t dummy;
  i2s_write(SPK_I2S_PORT, buf, sizeof(buf), &dummy, portMAX_DELAY);
}

// Predefined alert tones
void speaker_beep() {
  speaker_tone(1000, 100, 50);  // Short 1kHz beep
}

void speaker_alert() {
  speaker_tone(800, 200, 70);
  speaker_tone(1200, 200, 70);
}

void speaker_success() {
  speaker_tone(523, 100, 40);  // C5
  speaker_tone(659, 100, 40);  // E5
  speaker_tone(784, 150, 40);  // G5
}

void speaker_error() {
  speaker_tone(300, 300, 60);
  speaker_tone(200, 400, 60);
}

#endif // BOARD_MODEL == BOARD_TWATCH_ULT
#endif // SPEAKER_H
