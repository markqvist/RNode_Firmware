#include "Graphics.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define DISP_W 128
#define DISP_H 64
#define DISP_RST -1
#define DISP_ADDR 0x3c
Adafruit_SSD1306 display(DISP_W, DISP_H, &Wire, DISP_RST);

#define DISP_MODE_UNKNOWN   0x00
#define DISP_MODE_LANDSCAPE 0x01
#define DISP_MODE_PORTRAIT  0x02
uint8_t disp_mode = DISP_MODE_UNKNOWN;
uint8_t disp_ext_fb = false;
unsigned char fb[512];
uint32_t last_disp_update = 0;
uint8_t disp_target_fps = 7;
int disp_update_interval = 1000/disp_target_fps;

#define WATERFALL_SIZE 46
int waterfall[WATERFALL_SIZE];
int waterfall_head = 0;

int p_ad_x = 0;
int p_ad_y = 0;
int p_as_x = 0;
int p_as_y = 0;

GFXcanvas1 stat_area(64, 64);
GFXcanvas1 disp_area(64, 64);

void update_area_positions() {
  if (disp_mode == DISP_MODE_PORTRAIT) {
    p_ad_x = 0;
    p_ad_y = 0;
    p_as_x = 0;
    p_as_y = 64;
  } else if (disp_mode == DISP_MODE_LANDSCAPE) {
    p_ad_x = 0;
    p_ad_y = 0;
    p_as_x = 64;
    p_as_y = 0;
  }
}

bool display_init() {
  #if HAS_DISPLAY
    #if BOARD_MODEL == BOARD_RNODE_NG_20 || BOARD_MODEL == BOARD_LORA32_V2_0
      int pin_display_en = 16;
      digitalWrite(pin_display_en, LOW);
      delay(50);
      digitalWrite(pin_display_en, HIGH);
    #endif

    if(!display.begin(SSD1306_SWITCHCAPVCC, DISP_ADDR)) {
      return false;
    } else {
      #if BOARD_MODEL == BOARD_RNODE_NG_20
        disp_mode = DISP_MODE_PORTRAIT;
        display.setRotation(3);
      #elif BOARD_MODEL == BOARD_RNODE_NG_21
        disp_mode = DISP_MODE_PORTRAIT;
        display.setRotation(3);
      #else
        disp_mode = DISP_MODE_PORTRAIT;
        display.setRotation(3);
      #endif

      // display.dim(true);
      update_area_positions();
      for (int i = 0; i < WATERFALL_SIZE; i++) {
        waterfall[i] = 0;
      }

      return true;
    }
  #else
    return false;
  #endif
}

void draw_cable_icon(int px, int py) {
  if (cable_state == CABLE_STATE_DISCONNECTED) {
    stat_area.drawBitmap(px, py, bm_cable+0*32, 16, 16, SSD1306_WHITE, SSD1306_BLACK);
  } else if (cable_state == CABLE_STATE_CONNECTED) {
    stat_area.drawBitmap(px, py, bm_cable+1*32, 16, 16, SSD1306_WHITE, SSD1306_BLACK);
  }
}

void draw_bt_icon(int px, int py) {
  if (bt_state == BT_STATE_OFF) {
    stat_area.drawBitmap(px, py, bm_bt+0*32, 16, 16, SSD1306_WHITE, SSD1306_BLACK);
  } else if (bt_state == BT_STATE_ON) {
    stat_area.drawBitmap(px, py, bm_bt+1*32, 16, 16, SSD1306_WHITE, SSD1306_BLACK);
  } else if (bt_state == BT_STATE_PAIRING) {
    stat_area.drawBitmap(px, py, bm_bt+2*32, 16, 16, SSD1306_WHITE, SSD1306_BLACK);
  } else if (bt_state == BT_STATE_CONNECTED) {
    stat_area.drawBitmap(px, py, bm_bt+3*32, 16, 16, SSD1306_WHITE, SSD1306_BLACK);
  }
}

void draw_lora_icon(int px, int py) {
  if (radio_online) {
    stat_area.drawBitmap(px, py, bm_rf+1*32, 16, 16, SSD1306_WHITE, SSD1306_BLACK);
  } else {
    stat_area.drawBitmap(px, py, bm_rf+0*32, 16, 16, SSD1306_WHITE, SSD1306_BLACK);
  }
}

void draw_mw_icon(int px, int py) {
  if (mw_radio_online) {
    stat_area.drawBitmap(px, py, bm_rf+3*32, 16, 16, SSD1306_WHITE, SSD1306_BLACK);
  } else {
    stat_area.drawBitmap(px, py, bm_rf+2*32, 16, 16, SSD1306_WHITE, SSD1306_BLACK);
  }
}

uint8_t charge_tick = 0;
void draw_battery_bars(int px, int py) {  
  if (pmu_ready) {
    float battery_value = battery_percent;
    if (battery_state == BATTERY_STATE_CHARGING) {
      battery_value = charge_tick;
      charge_tick += 3;
      if (charge_tick > 100) charge_tick = 0;
    }

    stat_area.fillRect(px, py, 14, 3, SSD1306_BLACK);
    if (battery_value > 7) stat_area.drawLine(px, py, px, py+2, SSD1306_WHITE);
    if (battery_value > 20) stat_area.drawLine(px+1*2, py, px+1*2, py+2, SSD1306_WHITE);
    if (battery_value > 33) stat_area.drawLine(px+2*2, py, px+2*2, py+2, SSD1306_WHITE);
    if (battery_value > 46) stat_area.drawLine(px+3*2, py, px+3*2, py+2, SSD1306_WHITE);
    if (battery_value > 59) stat_area.drawLine(px+4*2, py, px+4*2, py+2, SSD1306_WHITE);
    if (battery_value > 72) stat_area.drawLine(px+5*2, py, px+5*2, py+2, SSD1306_WHITE);
    if (battery_value > 85) stat_area.drawLine(px+6*2, py, px+6*2, py+2, SSD1306_WHITE);
  }
}

#define Q_SNR_MIN -10.0
#define Q_SNR_MAX 8.0
#define Q_SNR_SPAN (Q_SNR_MAX-Q_SNR_MIN)
void draw_quality_bars(int px, int py) {
  signed char t_snr = (signed int)last_snr_raw;
  int snr_int = (int)t_snr;
  float snr = ((int)snr_int) * 0.25;
  float quality = ((snr-Q_SNR_MIN)/(Q_SNR_SPAN))*100;
  if (quality > 100.0) quality = 100.0;

  stat_area.fillRect(px, py, 13, 7, SSD1306_BLACK);
  // Serial.printf("Last SNR: %.2f\n, quality: %.2f\n", snr, quality);
  if (quality > 7)  stat_area.drawLine(px+0*2, py+7, px+0*2, py+6, SSD1306_WHITE);
  if (quality > 20) stat_area.drawLine(px+1*2, py+7, px+1*2, py+5, SSD1306_WHITE);
  if (quality > 33) stat_area.drawLine(px+2*2, py+7, px+2*2, py+4, SSD1306_WHITE);
  if (quality > 46) stat_area.drawLine(px+3*2, py+7, px+3*2, py+3, SSD1306_WHITE);
  if (quality > 59) stat_area.drawLine(px+4*2, py+7, px+4*2, py+2, SSD1306_WHITE);
  if (quality > 72) stat_area.drawLine(px+5*2, py+7, px+5*2, py+1, SSD1306_WHITE);
  if (quality > 85) stat_area.drawLine(px+6*2, py+7, px+6*2, py+0, SSD1306_WHITE);
}

#define S_RSSI_MIN -135.0
#define S_RSSI_MAX -60.0
#define S_RSSI_SPAN (S_RSSI_MAX-S_RSSI_MIN)
void draw_signal_bars(int px, int py) {
  int rssi_val = last_rssi;
  if (rssi_val < S_RSSI_MIN) rssi_val = S_RSSI_MIN;
  if (rssi_val > S_RSSI_MAX) rssi_val = S_RSSI_MAX;
  int signal = ((rssi_val - S_RSSI_MIN)*(1.0/S_RSSI_SPAN))*100.0;

  if (signal > 100.0) signal = 100.0;

  stat_area.fillRect(px, py, 13, 7, SSD1306_BLACK);
  // Serial.printf("Last SNR: %.2f\n, quality: %.2f\n", snr, quality);
  if (signal > 85) stat_area.drawLine(px+0*2, py+7, px+0*2, py+0, SSD1306_WHITE);
  if (signal > 72) stat_area.drawLine(px+1*2, py+7, px+1*2, py+1, SSD1306_WHITE);
  if (signal > 59) stat_area.drawLine(px+2*2, py+7, px+2*2, py+2, SSD1306_WHITE);
  if (signal > 46) stat_area.drawLine(px+3*2, py+7, px+3*2, py+3, SSD1306_WHITE);
  if (signal > 33) stat_area.drawLine(px+4*2, py+7, px+4*2, py+4, SSD1306_WHITE);
  if (signal > 20) stat_area.drawLine(px+5*2, py+7, px+5*2, py+5, SSD1306_WHITE);
  if (signal > 7)  stat_area.drawLine(px+6*2, py+7, px+6*2, py+6, SSD1306_WHITE);
}

#define WF_RSSI_MAX -60
#define WF_RSSI_MIN -135
#define WF_RSSI_SPAN (WF_RSSI_MAX-WF_RSSI_MIN)
#define WF_PIXEL_WIDTH 10
void draw_waterfall(int px, int py) {
  int rssi_val = current_rssi;
  if (rssi_val < WF_RSSI_MIN) rssi_val = WF_RSSI_MIN;
  if (rssi_val > WF_RSSI_MAX) rssi_val = WF_RSSI_MAX;
  int rssi_normalised = ((rssi_val - WF_RSSI_MIN)*(1.0/WF_RSSI_SPAN))*WF_PIXEL_WIDTH;

  waterfall[waterfall_head++] = rssi_normalised;
  if (waterfall_head >= WATERFALL_SIZE) waterfall_head = 0;

  stat_area.fillRect(px,py,WF_PIXEL_WIDTH, WATERFALL_SIZE, SSD1306_BLACK);
  for (int i = 0; i < WATERFALL_SIZE; i++){
    int wi = (waterfall_head+i)%WATERFALL_SIZE;
    int ws = waterfall[wi];
    if (ws > 0) {
      stat_area.drawLine(px, py+i, px+ws-1, py+i, SSD1306_WHITE);
    }
  }
}

bool stat_area_intialised = false;
void draw_stat_area() {
  if (!stat_area_intialised) {
    stat_area.drawBitmap(0, 0, bm_frame, 64, 64, SSD1306_WHITE, SSD1306_BLACK);
    stat_area_intialised = true;
  }

  draw_cable_icon(3, 8);
  draw_bt_icon(3, 30);
  draw_lora_icon(45, 8);
  draw_mw_icon(45, 30);
  draw_battery_bars(4, 58);
  if (radio_online) {
    draw_quality_bars(28, 56);
    draw_signal_bars(44, 56);
    draw_waterfall(27, 4);
  }
}

void update_stat_area() {
  draw_stat_area();
  display.drawBitmap(p_as_x, p_as_y, stat_area.getBuffer(), stat_area.width(), stat_area.height(), SSD1306_WHITE, SSD1306_BLACK);
}

void draw_disp_area() {
  if (!disp_ext_fb) {
    disp_area.drawBitmap(0, 0, bm_def, disp_area.width(), disp_area.height(), SSD1306_WHITE, SSD1306_BLACK);
  } else {
    disp_area.drawBitmap(0, 0, fb, disp_area.width(), disp_area.height(), SSD1306_WHITE, SSD1306_BLACK);
  }
}

void update_disp_area() {
  draw_disp_area();
  display.drawBitmap(p_ad_x, p_ad_y, disp_area.getBuffer(), disp_area.width(), disp_area.height(), SSD1306_WHITE, SSD1306_BLACK);
}

void update_display() {
  if (millis()-last_disp_update >= disp_update_interval) {
    display.clearDisplay();
    update_stat_area();
    update_disp_area();
    display.display();
    last_disp_update = millis();
  }
}

void ext_fb_enable() {
  disp_ext_fb = true;
}

void ext_fb_disable() {
  disp_ext_fb = false;
}
