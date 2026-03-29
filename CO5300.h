// CO5300 QSPI AMOLED Display Driver for T-Watch Ultra
// 410x502 pixels, 16-bit RGB565, QSPI interface
// Based on LilyGoLib display implementation (MIT license)

#ifndef CO5300_H
#define CO5300_H

#if BOARD_MODEL == BOARD_TWATCH_ULT

#include <Arduino.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"

#define CO5300_CMD_SLPIN      0x10
#define CO5300_CMD_SLPOUT     0x11
#define CO5300_CMD_CASET      0x2A
#define CO5300_CMD_RASET      0x2B
#define CO5300_CMD_RAMWR      0x2C
#define CO5300_CMD_MADCTL     0x36
#define CO5300_CMD_BRIGHTNESS 0x51

#define CO5300_WIDTH          410
#define CO5300_HEIGHT         502
#define CO5300_OFFSET_X       22
#define CO5300_OFFSET_Y       0

#define CO5300_SPI_HOST       SPI3_HOST
#define CO5300_SPI_FREQ_MHZ   45
#define CO5300_SEND_BUF_SIZE  16384

// Init command table entry
typedef struct {
  uint8_t cmd;
  uint8_t data[20];
  uint8_t len;  // bit 7 = delay 120ms after command
} co5300_cmd_t;

static const co5300_cmd_t co5300_init_cmds[] = {
  {0xFE, {0x00}, 0x01},
  {0xC4, {0x80}, 0x01},
  {0x3A, {0x55}, 0x01},                          // RGB565
  {0x35, {0x00}, 0x01},                          // Tearing effect on
  {0x53, {0x20}, 0x01},                          // Brightness control enable
  {0x63, {0xFF}, 0x01},
  {0x2A, {0x00, 0x16, 0x01, 0xAF}, 0x04},       // Column: 22 to 431
  {0x2B, {0x00, 0x00, 0x01, 0xF5}, 0x04},       // Row: 0 to 501
  {0x11, {0}, 0x80},                              // Sleep out + delay
  {0x29, {0}, 0x80},                              // Display on + delay
  {0x51, {0x00}, 0x01},                          // Brightness = 0
};
#define CO5300_INIT_CMD_COUNT (sizeof(co5300_init_cmds) / sizeof(co5300_init_cmds[0]))

static spi_device_handle_t co5300_spi = NULL;
static bool co5300_ready = false;
static uint8_t co5300_brightness = 0;

// Send a command with optional data bytes via QSPI (DMA-based)
static void co5300_write_cmd(uint8_t cmd, uint8_t *data, uint32_t len) {
  digitalWrite(DISP_CS, LOW);
  spi_transaction_t t = {};
  t.flags = SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR;
  t.cmd = 0x02;
  t.addr = cmd << 8;
  if (len > 0 && data) {
    t.tx_buffer = data;
    t.length = 8 * len;
  }
  spi_device_transmit(co5300_spi, &t);
  digitalWrite(DISP_CS, HIGH);
}

// Set the pixel address window
static void co5300_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
  x1 += CO5300_OFFSET_X;
  x2 += CO5300_OFFSET_X;
  y1 += CO5300_OFFSET_Y;
  y2 += CO5300_OFFSET_Y;

  uint8_t caset[] = {highByte(x1), lowByte(x1), highByte(x2), lowByte(x2)};
  uint8_t raset[] = {highByte(y1), lowByte(y1), highByte(y2), lowByte(y2)};
  co5300_write_cmd(CO5300_CMD_CASET, caset, 4);
  co5300_write_cmd(CO5300_CMD_RASET, raset, 4);
  co5300_write_cmd(CO5300_CMD_RAMWR, NULL, 0);
}

// Push pixel data to the display (RGB565, DMA-based, blocking)
void co5300_push_pixels(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *pixels) {
  if (!co5300_ready) return;

  co5300_set_window(x, y, x + w - 1, y + h - 1);

  uint32_t total = w * h;
  uint16_t *p = pixels;
  bool first = true;

  digitalWrite(DISP_CS, LOW);
  while (total > 0) {
    uint32_t chunk = (total > CO5300_SEND_BUF_SIZE) ? CO5300_SEND_BUF_SIZE : total;
    spi_transaction_ext_t t = {};
    if (first) {
      t.base.flags = SPI_TRANS_MODE_QIO;
      t.base.cmd = 0x32;
      t.base.addr = 0x002C00;
      first = false;
    } else {
      t.base.flags = SPI_TRANS_MODE_QIO | SPI_TRANS_VARIABLE_CMD |
                     SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
      t.command_bits = 0;
      t.address_bits = 0;
      t.dummy_bits = 0;
    }
    t.base.tx_buffer = p;
    t.base.length = chunk * 16;
    spi_device_transmit(co5300_spi, (spi_transaction_t *)&t);
    p += chunk;
    total -= chunk;
  }
  digitalWrite(DISP_CS, HIGH);
}

// --- Async pixel push (display SPI3 is independent from LoRa/SD SPI) ---
// Queue all DMA transactions on SPI3 and return immediately.
// co5300_push_wait() blocks until complete.
// Safe to run LoRa/SD on the other SPI bus while this runs.
#define CO5300_MAX_ASYNC_TXNS 14
static spi_transaction_ext_t co5300_async_txns[CO5300_MAX_ASYNC_TXNS];
static int co5300_async_pending = 0;

void co5300_push_start(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *pixels) {
  if (!co5300_ready) return;

  // Window setup — blocking but fast (3 small SPI commands)
  co5300_set_window(x, y, x + w - 1, y + h - 1);

  uint32_t total = w * h;
  uint16_t *p = pixels;
  bool first = true;
  co5300_async_pending = 0;

  digitalWrite(DISP_CS, LOW);
  while (total > 0 && co5300_async_pending < CO5300_MAX_ASYNC_TXNS) {
    uint32_t chunk = (total > CO5300_SEND_BUF_SIZE) ? CO5300_SEND_BUF_SIZE : total;
    int i = co5300_async_pending;
    memset(&co5300_async_txns[i], 0, sizeof(spi_transaction_ext_t));
    if (first) {
      co5300_async_txns[i].base.flags = SPI_TRANS_MODE_QIO;
      co5300_async_txns[i].base.cmd = 0x32;
      co5300_async_txns[i].base.addr = 0x002C00;
      first = false;
    } else {
      co5300_async_txns[i].base.flags = SPI_TRANS_MODE_QIO | SPI_TRANS_VARIABLE_CMD |
                                         SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
      co5300_async_txns[i].command_bits = 0;
      co5300_async_txns[i].address_bits = 0;
      co5300_async_txns[i].dummy_bits = 0;
    }
    co5300_async_txns[i].base.tx_buffer = p;
    co5300_async_txns[i].base.length = chunk * 16;
    spi_device_queue_trans(co5300_spi, (spi_transaction_t *)&co5300_async_txns[i], portMAX_DELAY);
    p += chunk;
    total -= chunk;
    co5300_async_pending++;
  }
  // DMA now running in background on SPI3 — return immediately
}

void co5300_push_wait() {
  spi_transaction_t *rtrans;
  while (co5300_async_pending > 0) {
    spi_device_get_trans_result(co5300_spi, &rtrans, portMAX_DELAY);
    co5300_async_pending--;
  }
  digitalWrite(DISP_CS, HIGH);
}

bool co5300_push_done() {
  if (co5300_async_pending == 0) return true;
  spi_transaction_t *rtrans;
  while (co5300_async_pending > 0) {
    if (spi_device_get_trans_result(co5300_spi, &rtrans, 0) == ESP_OK) {
      co5300_async_pending--;
    } else {
      return false;
    }
  }
  digitalWrite(DISP_CS, HIGH);
  return true;
}

// Fill a rectangle with a solid colour
void co5300_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
  if (!co5300_ready) return;

  uint32_t total = w * h;
  uint32_t buf_size = (total > CO5300_SEND_BUF_SIZE) ? CO5300_SEND_BUF_SIZE : total;
  uint16_t *buf = (uint16_t *)heap_caps_malloc(buf_size * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!buf) {
    buf = (uint16_t *)malloc(buf_size * 2);
    if (!buf) return;
  }

  for (uint32_t i = 0; i < buf_size; i++) buf[i] = color;

  co5300_set_window(x, y, x + w - 1, y + h - 1);

  uint32_t remaining = total;
  bool first = true;
  digitalWrite(DISP_CS, LOW);
  while (remaining > 0) {
    uint32_t chunk = (remaining > buf_size) ? buf_size : remaining;
    spi_transaction_ext_t t = {};
    if (first) {
      t.base.flags = SPI_TRANS_MODE_QIO;
      t.base.cmd = 0x32;
      t.base.addr = 0x002C00;
      first = false;
    } else {
      t.base.flags = SPI_TRANS_MODE_QIO | SPI_TRANS_VARIABLE_CMD |
                     SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
      t.command_bits = 0;
      t.address_bits = 0;
      t.dummy_bits = 0;
    }
    t.base.tx_buffer = buf;
    t.base.length = chunk * 16;
    spi_device_transmit(co5300_spi, (spi_transaction_t *)&t);
    remaining -= chunk;
  }
  digitalWrite(DISP_CS, HIGH);

  heap_caps_free(buf);
}

// Clear the entire display to black
void co5300_clear() {
  co5300_fill_rect(0, 0, CO5300_WIDTH, CO5300_HEIGHT, 0x0000);
}

void co5300_set_brightness(uint8_t level) {
  if (!co5300_ready) return;
  co5300_brightness = level;
  co5300_write_cmd(CO5300_CMD_BRIGHTNESS, &level, 1);
}

void co5300_sleep() {
  if (!co5300_ready) return;
  co5300_write_cmd(CO5300_CMD_SLPIN, NULL, 0);
}

void co5300_wakeup() {
  if (!co5300_ready) return;
  co5300_write_cmd(CO5300_CMD_SLPOUT, NULL, 0);
  delay(120);
}

bool co5300_init() {
  pinMode(DISP_CS, OUTPUT);
  digitalWrite(DISP_CS, HIGH);

  // Hardware reset on GPIO 37 (direct pin, not XL9555 expander)
  pinMode(DISP_RST, OUTPUT);
  digitalWrite(DISP_RST, HIGH);
  delay(200);
  digitalWrite(DISP_RST, LOW);
  delay(300);
  digitalWrite(DISP_RST, HIGH);
  delay(200);

  // Configure QSPI bus
  spi_bus_config_t bus_cfg = {};
  bus_cfg.data0_io_num = DISP_D0;
  bus_cfg.data1_io_num = DISP_D1;
  bus_cfg.sclk_io_num  = DISP_SCK;
  bus_cfg.data2_io_num = DISP_D2;
  bus_cfg.data3_io_num = DISP_D3;
  bus_cfg.data4_io_num = -1;
  bus_cfg.data5_io_num = -1;
  bus_cfg.data6_io_num = -1;
  bus_cfg.data7_io_num = -1;
  bus_cfg.max_transfer_sz = 0x40000 + 8;
  bus_cfg.flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS;

  spi_device_interface_config_t dev_cfg = {};
  dev_cfg.command_bits = 8;
  dev_cfg.address_bits = 24;
  dev_cfg.mode = SPI_MODE0;
  dev_cfg.clock_speed_hz = CO5300_SPI_FREQ_MHZ * 1000 * 1000;
  dev_cfg.spics_io_num = -1;  // CS managed manually
  dev_cfg.flags = SPI_DEVICE_HALFDUPLEX;
  dev_cfg.queue_size = 17;

  if (spi_bus_initialize(CO5300_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO) != ESP_OK) {
    return false;
  }
  if (spi_bus_add_device(CO5300_SPI_HOST, &dev_cfg, &co5300_spi) != ESP_OK) {
    return false;
  }

  // Send init sequence (twice, per LilyGoLib pattern)
  for (int pass = 0; pass < 2; pass++) {
    for (uint32_t i = 0; i < CO5300_INIT_CMD_COUNT; i++) {
      co5300_write_cmd(co5300_init_cmds[i].cmd,
                       (uint8_t *)co5300_init_cmds[i].data,
                       co5300_init_cmds[i].len & 0x1F);
      if (co5300_init_cmds[i].len & 0x80) {
        delay(120);
      }
    }
  }

  // Set rotation to portrait (default for watch)
  uint8_t madctl = 0x00;  // RGB order, no mirror/swap
  co5300_write_cmd(CO5300_CMD_MADCTL, &madctl, 1);

  co5300_ready = true;

  // Clear screen to black
  co5300_clear();

  // Set initial brightness
  co5300_set_brightness(128);

  return true;
}

void co5300_end() {
  if (co5300_spi) {
    spi_bus_remove_device(co5300_spi);
    spi_bus_free(CO5300_SPI_HOST);
    co5300_spi = NULL;
  }
  co5300_ready = false;
}

// ---- Simple text rendering using Adafruit GFX fonts ----
// Only available when HAS_DISPLAY is true (Display.h includes Adafruit_GFX.h)

#if HAS_DISPLAY

// Draw a single character using Adafruit GFX font at a given position
// Returns the advance width in pixels
static uint16_t co5300_draw_char(uint16_t *fb, uint16_t fb_w, uint16_t fb_h,
                                  int16_t cx, int16_t cy, char c,
                                  uint16_t fg, const GFXfont *font) {
  if (c < font->first || c > font->last) return 0;
  GFXglyph *glyph = &font->glyph[c - font->first];
  uint8_t *bitmap = font->bitmap;
  uint16_t bo = glyph->bitmapOffset;
  uint8_t gw = glyph->width, gh = glyph->height;
  int8_t xo = glyph->xOffset, yo = glyph->yOffset;
  uint8_t bits = 0, bit = 0;

  for (int16_t yy = 0; yy < gh; yy++) {
    for (int16_t xx = 0; xx < gw; xx++) {
      if (!(bit++ & 7)) bits = bitmap[bo++];
      if (bits & 0x80) {
        int16_t px = cx + xo + xx;
        int16_t py = cy + yo + yy;
        if (px >= 0 && px < fb_w && py >= 0 && py < fb_h) {
          fb[py * fb_w + px] = fg;
        }
      }
      bits <<= 1;
    }
  }
  return glyph->xAdvance;
}

// Draw a string using Adafruit GFX font, returns total width
uint16_t co5300_draw_string(uint16_t *fb, uint16_t fb_w, uint16_t fb_h,
                            int16_t x, int16_t y, const char *str,
                            uint16_t fg, const GFXfont *font) {
  int16_t cx = x;
  while (*str) {
    cx += co5300_draw_char(fb, fb_w, fb_h, cx, y, *str, fg, font);
    str++;
  }
  return cx - x;
}

// RGB565 colour helpers
#define CO5300_BLACK   0x0000
#define CO5300_WHITE   0xFFFF
#define CO5300_RED     0xF800
#define CO5300_GREEN   0x07E0
#define CO5300_BLUE    0x001F
#define CO5300_CYAN    0x07FF
#define CO5300_YELLOW  0xFFE0
#define CO5300_GREY    0x7BEF

#endif // HAS_DISPLAY

#endif // BOARD_MODEL == BOARD_TWATCH_ULT
#endif // CO5300_H
