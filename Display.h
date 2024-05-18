// Copyright (C) 2023, Mark Qvist

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "Graphics.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Fonts/Org_01.h"
#define DISP_W 128
#define DISP_H 64
#if BOARD_MODEL == BOARD_RNODE_NG_20 || BOARD_MODEL == BOARD_LORA32_V2_0
  #define DISP_RST -1
  #define DISP_ADDR 0x3C
#elif BOARD_MODEL == BOARD_TBEAM
  #define DISP_RST 13
  #define DISP_ADDR 0x3C
  #define DISP_CUSTOM_ADDR true
#elif BOARD_MODEL == BOARD_HELTEC32_V2 || BOARD_MODEL == BOARD_LORA32_V1_0
  #define DISP_RST 16
  #define DISP_ADDR 0x3C
  #define SCL_OLED 15
  #define SDA_OLED 4
#elif BOARD_MODEL == BOARD_HELTEC32_V3
  #define DISP_RST 21
  #define DISP_ADDR 0x3C
  #define SCL_OLED 18
  #define SDA_OLED 17
#elif BOARD_MODEL == BOARD_RNODE_NG_21
  #define DISP_RST -1
  #define DISP_ADDR 0x3C
#elif BOARD_MODEL == BOARD_RNODE_NG_22
  #define DISP_RST 21
  #define DISP_ADDR 0x3C
  #define SCL_OLED 17
  #define SDA_OLED 18
#else
  #define DISP_RST -1
  #define DISP_ADDR 0x3C
  #define DISP_CUSTOM_ADDR true
#endif

#define SMALL_FONT &Org_01

Adafruit_SSD1306 display(DISP_W, DISP_H, &Wire, DISP_RST);

#define DISP_MODE_UNKNOWN   0x00
#define DISP_MODE_LANDSCAPE 0x01
#define DISP_MODE_PORTRAIT  0x02
#define DISP_PIN_SIZE   6
uint8_t disp_mode = DISP_MODE_UNKNOWN;
uint8_t disp_ext_fb = false;
unsigned char fb[512];
uint32_t last_disp_update = 0;
uint8_t disp_target_fps = 7;
int disp_update_interval = 1000/disp_target_fps;
uint32_t last_page_flip = 0;
int page_interval = 4000;
bool device_signatures_ok();
bool device_firmware_ok();

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

uint8_t display_contrast = 0x00;
void set_contrast(Adafruit_SSD1306 *display, uint8_t contrast) {
    display->ssd1306_command(SSD1306_SETCONTRAST);
    display->ssd1306_command(contrast);
}

bool display_init() {
  #if HAS_DISPLAY
    #if BOARD_MODEL == BOARD_RNODE_NG_20 || BOARD_MODEL == BOARD_LORA32_V2_0
      int pin_display_en = 16;
      digitalWrite(pin_display_en, LOW);
      delay(50);
      digitalWrite(pin_display_en, HIGH);
    #elif BOARD_MODEL == BOARD_RNODE_NG_22
      Wire.begin(SDA_OLED, SCL_OLED);
    #elif BOARD_MODEL == BOARD_HELTEC32_V2
      Wire.begin(SDA_OLED, SCL_OLED);
    #elif BOARD_MODEL == BOARD_HELTEC32_V3
      // enable vext / pin 36
      pinMode(Vext, OUTPUT);
      digitalWrite(Vext, LOW);
      delay(50);
      int pin_display_en = 21;
      pinMode(pin_display_en, OUTPUT);
      digitalWrite(pin_display_en, LOW);
      delay(50);
      digitalWrite(pin_display_en, HIGH);
      delay(50);
      Wire.begin(SDA_OLED, SCL_OLED);
    #elif BOARD_MODEL == BOARD_LORA32_V1_0
      int pin_display_en = 16;
      digitalWrite(pin_display_en, LOW);
      delay(50);
      digitalWrite(pin_display_en, HIGH);
      Wire.begin(SDA_OLED, SCL_OLED);
    #endif

    #if DISP_CUSTOM_ADDR == true
      #if HAS_EEPROM
      uint8_t display_address = EEPROM.read(eeprom_addr(ADDR_CONF_DADR));
      #elif MCU_VARIANT == MCU_NRF52
      uint8_t display_address = eeprom_read(eeprom_addr(ADDR_CONF_DADR));
      #endif
      if (display_address == 0xFF) display_address = DISP_ADDR;
    #else
      uint8_t display_address = DISP_ADDR;
    #endif

    
    if(!display.begin(SSD1306_SWITCHCAPVCC, display_address)) {
      return false;
    } else {
      set_contrast(&display, display_contrast);
      #if BOARD_MODEL == BOARD_RNODE_NG_20
        disp_mode = DISP_MODE_PORTRAIT;
        display.setRotation(3);
      #elif BOARD_MODEL == BOARD_RNODE_NG_21
        disp_mode = DISP_MODE_PORTRAIT;
        display.setRotation(3);
      #elif BOARD_MODEL == BOARD_LORA32_V1_0
        disp_mode = DISP_MODE_PORTRAIT;
        display.setRotation(3);
      #elif BOARD_MODEL == BOARD_LORA32_V2_0
        disp_mode = DISP_MODE_PORTRAIT;
        display.setRotation(3);
      #elif BOARD_MODEL == BOARD_LORA32_V2_1
        disp_mode = DISP_MODE_LANDSCAPE;
        display.setRotation(0);
      #elif BOARD_MODEL == BOARD_TBEAM
        disp_mode = DISP_MODE_LANDSCAPE;
        display.setRotation(0);
      #elif BOARD_MODEL == BOARD_HELTEC32_V2
        disp_mode = DISP_MODE_PORTRAIT;
        display.setRotation(1);
      #elif BOARD_MODEL == BOARD_HELTEC32_V3
        disp_mode = DISP_MODE_PORTRAIT;
        // Antenna conx up
        display.setRotation(1);
        // USB-C up
        // display.setRotation(3);
      #else
        disp_mode = DISP_MODE_PORTRAIT;
        display.setRotation(3);
      #endif

      update_area_positions();
      for (int i = 0; i < WATERFALL_SIZE; i++) {
        waterfall[i] = 0;
      }

      last_page_flip = millis();

      stat_area.cp437(true);
      disp_area.cp437(true);
      display.cp437(true);

      #if HAS_EEPROM
      uint8_t display_intensity = EEPROM.read(eeprom_addr(ADDR_CONF_DINT));
      #elif MCU_VARIANT == MCU_NRF52
      uint8_t display_intensity = eeprom_read(eeprom_addr(ADDR_CONF_DINT));
      #endif

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
  } else {
    stat_area.drawBitmap(px, py, bm_bt+0*32, 16, 16, SSD1306_WHITE, SSD1306_BLACK);
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
    if (battery_ready) {
      if (battery_installed) {
      float battery_value = battery_percent;
        
        if (battery_state == BATTERY_STATE_CHARGING) {
          battery_value = charge_tick;
          charge_tick += 3;
          if (charge_tick > 100) charge_tick = 0;
        }

        if (battery_indeterminate && battery_state == BATTERY_STATE_CHARGING) {
          stat_area.fillRect(px-2, py-2, 18, 7, SSD1306_BLACK);
          stat_area.drawBitmap(px-2, py-2, bm_plug, 17, 7, SSD1306_WHITE, SSD1306_BLACK);
        } else {
          if (battery_state == BATTERY_STATE_CHARGED) {
            stat_area.fillRect(px-2, py-2, 18, 7, SSD1306_BLACK);
            stat_area.drawBitmap(px-2, py-2, bm_plug, 17, 7, SSD1306_WHITE, SSD1306_BLACK);
          } else {
            // stat_area.fillRect(px, py, 14, 3, SSD1306_BLACK);
            stat_area.fillRect(px-2, py-2, 18, 7, SSD1306_BLACK);
            stat_area.drawRect(px-2, py-2, 17, 7, SSD1306_WHITE);
            stat_area.drawLine(px+15, py, px+15, py+3, SSD1306_WHITE);
            if (battery_value > 7) stat_area.drawLine(px, py, px, py+2, SSD1306_WHITE);
            if (battery_value > 20) stat_area.drawLine(px+1*2, py, px+1*2, py+2, SSD1306_WHITE);
            if (battery_value > 33) stat_area.drawLine(px+2*2, py, px+2*2, py+2, SSD1306_WHITE);
            if (battery_value > 46) stat_area.drawLine(px+3*2, py, px+3*2, py+2, SSD1306_WHITE);
            if (battery_value > 59) stat_area.drawLine(px+4*2, py, px+4*2, py+2, SSD1306_WHITE);
            if (battery_value > 72) stat_area.drawLine(px+5*2, py, px+5*2, py+2, SSD1306_WHITE);
            if (battery_value > 85) stat_area.drawLine(px+6*2, py, px+6*2, py+2, SSD1306_WHITE);
          }
        }
      } else {
        stat_area.fillRect(px-2, py-2, 18, 7, SSD1306_BLACK);
        stat_area.drawBitmap(px-2, py-2, bm_plug, 17, 7, SSD1306_WHITE, SSD1306_BLACK);
      }
    }
  } else {
    stat_area.fillRect(px-2, py-2, 18, 7, SSD1306_BLACK);
    stat_area.drawBitmap(px-2, py-2, bm_plug, 17, 7, SSD1306_WHITE, SSD1306_BLACK);
  }
}

#define Q_SNR_STEP 2.0
#define Q_SNR_MIN_BASE -9.0
#define Q_SNR_MAX 6.0
void draw_quality_bars(int px, int py) {
  signed char t_snr = (signed int)last_snr_raw;
  int snr_int = (int)t_snr;
  float snr_min = Q_SNR_MIN_BASE-(int)lora_sf*Q_SNR_STEP;
  float snr_span = (Q_SNR_MAX-snr_min);
  float snr = ((int)snr_int) * 0.25;
  float quality = ((snr-snr_min)/(snr_span))*100;
  if (quality > 100.0) quality = 100.0;
  if (quality < 0.0) quality = 0.0;

  stat_area.fillRect(px, py, 13, 7, SSD1306_BLACK);
  // Serial.printf("Last SNR: %.2f\n, quality: %.2f\n", snr, quality);
  if (quality > 0)  stat_area.drawLine(px+0*2, py+7, px+0*2, py+6, SSD1306_WHITE);
  if (quality > 15) stat_area.drawLine(px+1*2, py+7, px+1*2, py+5, SSD1306_WHITE);
  if (quality > 30) stat_area.drawLine(px+2*2, py+7, px+2*2, py+4, SSD1306_WHITE);
  if (quality > 45) stat_area.drawLine(px+3*2, py+7, px+3*2, py+3, SSD1306_WHITE);
  if (quality > 60) stat_area.drawLine(px+4*2, py+7, px+4*2, py+2, SSD1306_WHITE);
  if (quality > 75) stat_area.drawLine(px+5*2, py+7, px+5*2, py+1, SSD1306_WHITE);
  if (quality > 90) stat_area.drawLine(px+6*2, py+7, px+6*2, py+0, SSD1306_WHITE);
}

#define S_RSSI_MIN -135.0
#define S_RSSI_MAX -75.0
#define S_RSSI_SPAN (S_RSSI_MAX-S_RSSI_MIN)
void draw_signal_bars(int px, int py) {
  int rssi_val = last_rssi;
  if (rssi_val < S_RSSI_MIN) rssi_val = S_RSSI_MIN;
  if (rssi_val > S_RSSI_MAX) rssi_val = S_RSSI_MAX;
  int signal = ((rssi_val - S_RSSI_MIN)*(1.0/S_RSSI_SPAN))*100.0;

  if (signal > 100.0) signal = 100.0;
  if (signal < 0.0) signal = 0.0;

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
  if (device_init_done) {
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
}

void update_stat_area() {
  if (eeprom_ok && !firmware_update_mode && !console_active) {
    
    draw_stat_area();
    if (disp_mode == DISP_MODE_PORTRAIT) {
      display.drawBitmap(p_as_x, p_as_y, stat_area.getBuffer(), stat_area.width(), stat_area.height(), SSD1306_WHITE, SSD1306_BLACK);
    } else if (disp_mode == DISP_MODE_LANDSCAPE) {
      display.drawBitmap(p_as_x+2, p_as_y, stat_area.getBuffer(), stat_area.width(), stat_area.height(), SSD1306_WHITE, SSD1306_BLACK);
      if (device_init_done && !disp_ext_fb) display.drawLine(p_as_x, 0, p_as_x, 64, SSD1306_WHITE);
    }

  } else {
    if (firmware_update_mode) {
      display.drawBitmap(p_as_x, p_as_y, bm_updating, stat_area.width(), stat_area.height(), SSD1306_BLACK, SSD1306_WHITE);
    } else if (console_active && device_init_done) {
      display.drawBitmap(p_as_x, p_as_y, bm_console, stat_area.width(), stat_area.height(), SSD1306_BLACK, SSD1306_WHITE);
      if (disp_mode == DISP_MODE_LANDSCAPE) {
        display.drawLine(p_as_x, 0, p_as_x, 64, SSD1306_WHITE);
      }
    }
  }
}

#define START_PAGE 0
const uint8_t pages = 3;
uint8_t disp_page = START_PAGE;
void draw_disp_area() {
  if (!device_init_done || firmware_update_mode) {
    uint8_t p_by = 37;
    if (disp_mode == DISP_MODE_LANDSCAPE || firmware_update_mode) {
      p_by = 18;
      disp_area.fillRect(0, 0, disp_area.width(), disp_area.height(), SSD1306_BLACK);
    }
    if (!device_init_done) disp_area.drawBitmap(0, p_by, bm_boot, disp_area.width(), 27, SSD1306_WHITE, SSD1306_BLACK);
    if (firmware_update_mode) disp_area.drawBitmap(0, p_by, bm_fw_update, disp_area.width(), 27, SSD1306_WHITE, SSD1306_BLACK);
  } else {
    if (!disp_ext_fb or bt_ssp_pin != 0) {
      if (radio_online && display_diagnostics) {
        disp_area.fillRect(0,8,disp_area.width(),37, SSD1306_BLACK); disp_area.fillRect(0,37,disp_area.width(),27, SSD1306_WHITE);
        disp_area.setFont(SMALL_FONT); disp_area.setTextWrap(false); disp_area.setTextColor(SSD1306_WHITE);

        disp_area.setCursor(2, 13);
        disp_area.print("On");
        disp_area.setCursor(14, 13);
        disp_area.print("@");
        disp_area.setCursor(21, 13);
        disp_area.printf("%.1fKbps", (float)lora_bitrate/1000.0);

        //disp_area.setCursor(31, 23-1);
        disp_area.setCursor(2, 23-1);
        disp_area.print("Airtime:");
        
        disp_area.setCursor(11, 33-1);
        if (total_channel_util < 0.099) {
          //disp_area.printf("%.1f%%", total_channel_util*100.0);
          disp_area.printf("%.1f%%", airtime*100.0);
        } else {
          //disp_area.printf("%.0f%%", total_channel_util*100.0);
          disp_area.printf("%.0f%%", airtime*100.0);
        }
        disp_area.drawBitmap(2, 26-1, bm_hg_low, 5, 9, SSD1306_WHITE, SSD1306_BLACK);

        disp_area.setCursor(32+11, 33-1);
        if (longterm_channel_util < 0.099) {
          //disp_area.printf("%.1f%%", longterm_channel_util*100.0);
          disp_area.printf("%.1f%%", longterm_airtime*100.0);
        } else {
          //disp_area.printf("%.0f%%", longterm_channel_util*100.0);
          disp_area.printf("%.0f%%", longterm_airtime*100.0);
        }
        disp_area.drawBitmap(32+2, 26-1, bm_hg_high, 5, 9, SSD1306_WHITE, SSD1306_BLACK);


        disp_area.setTextColor(SSD1306_BLACK);
        disp_area.setCursor(2, 46);
        disp_area.print("Channel");
        disp_area.setCursor(38, 46);
        disp_area.print("Load:");
        
        disp_area.setCursor(11, 57);
        if (total_channel_util < 0.099) {
          //disp_area.printf("%.1f%%", airtime*100.0);
          disp_area.printf("%.1f%%", total_channel_util*100.0);
        } else {
          //disp_area.printf("%.0f%%", airtime*100.0);
          disp_area.printf("%.0f%%", total_channel_util*100.0);
        }
        disp_area.drawBitmap(2, 50, bm_hg_low, 5, 9, SSD1306_BLACK, SSD1306_WHITE);

        disp_area.setCursor(32+11, 57);
        if (longterm_channel_util < 0.099) {
          //disp_area.printf("%.1f%%", longterm_airtime*100.0);
          disp_area.printf("%.1f%%", longterm_channel_util*100.0);
        } else {
          //disp_area.printf("%.0f%%", longterm_airtime*100.0);
          disp_area.printf("%.0f%%", longterm_channel_util*100.0);
        }
        disp_area.drawBitmap(32+2, 50, bm_hg_high, 5, 9, SSD1306_BLACK, SSD1306_WHITE);

      } else {
        if (device_signatures_ok()) {
          disp_area.drawBitmap(0, 0, bm_def_lc, disp_area.width(), 37, SSD1306_WHITE, SSD1306_BLACK);      
        } else {
          disp_area.drawBitmap(0, 0, bm_def, disp_area.width(), 37, SSD1306_WHITE, SSD1306_BLACK);      
        }
      }

      if (!hw_ready || radio_error || !device_firmware_ok()) {
        if (!device_firmware_ok()) {
          disp_area.drawBitmap(0, 37, bm_fw_corrupt, disp_area.width(), 27, SSD1306_WHITE, SSD1306_BLACK);
        } else {
          if (!modem_installed) {
            disp_area.drawBitmap(0, 37, bm_no_radio, disp_area.width(), 27, SSD1306_WHITE, SSD1306_BLACK);
          } else {
            disp_area.drawBitmap(0, 37, bm_hwfail, disp_area.width(), 27, SSD1306_WHITE, SSD1306_BLACK);
          }
        }
      } else if (bt_state == BT_STATE_PAIRING and bt_ssp_pin != 0) {
        char *pin_str = (char*)malloc(DISP_PIN_SIZE+1);
        sprintf(pin_str, "%06d", bt_ssp_pin);

        disp_area.drawBitmap(0, 37, bm_pairing, disp_area.width(), 27, SSD1306_WHITE, SSD1306_BLACK);
        for (int i = 0; i < DISP_PIN_SIZE; i++) {
          uint8_t numeric = pin_str[i]-48;
          uint8_t offset = numeric*5;
          disp_area.drawBitmap(7+9*i, 37+16, bm_n_uh+offset, 8, 5, SSD1306_WHITE, SSD1306_BLACK);
        }
        free(pin_str);
      } else {
        if (millis()-last_page_flip >= page_interval) {
          disp_page = (++disp_page%pages);
          last_page_flip = millis();
          if (not community_fw and disp_page == 0) disp_page = 1;
        }

        if (radio_online) {
          if (!display_diagnostics) {
            disp_area.drawBitmap(0, 37, bm_online, disp_area.width(), 27, SSD1306_WHITE, SSD1306_BLACK);
          }
        } else {
          if (disp_page == 0) {
            if (true || device_signatures_ok()) {
              disp_area.drawBitmap(0, 37, bm_checks, disp_area.width(), 27, SSD1306_WHITE, SSD1306_BLACK);
            } else {
              disp_area.drawBitmap(0, 37, bm_nfr, disp_area.width(), 27, SSD1306_WHITE, SSD1306_BLACK);
            }
          } else if (disp_page == 1) {
            if (!console_active) {
              disp_area.drawBitmap(0, 37, bm_hwok, disp_area.width(), 27, SSD1306_WHITE, SSD1306_BLACK);
            } else {
              disp_area.drawBitmap(0, 37, bm_console_active, disp_area.width(), 27, SSD1306_WHITE, SSD1306_BLACK);
            }
          } else if (disp_page == 2) {
            disp_area.drawBitmap(0, 37, bm_version, disp_area.width(), 27, SSD1306_WHITE, SSD1306_BLACK);
            char *v_str = (char*)malloc(3+1);
            sprintf(v_str, "%01d%02d", MAJ_VERS, MIN_VERS);
            for (int i = 0; i < 3; i++) {
              uint8_t numeric = v_str[i]-48; uint8_t bm_offset = numeric*5;
              uint8_t dxp = 20;
              if (i == 1) dxp += 9*1+4;
              if (i == 2) dxp += 9*2+4;
              disp_area.drawBitmap(dxp, 37+16, bm_n_uh+bm_offset, 8, 5, SSD1306_WHITE, SSD1306_BLACK);
            }
            free(v_str);
            disp_area.drawLine(27, 37+19, 28, 37+19, SSD1306_BLACK);
            disp_area.drawLine(27, 37+20, 28, 37+20, SSD1306_BLACK);
          }
        }
      }
    } else {
      disp_area.drawBitmap(0, 0, fb, disp_area.width(), disp_area.height(), SSD1306_WHITE, SSD1306_BLACK);
    }
  }
}

void update_disp_area() {
  draw_disp_area();
  display.drawBitmap(p_ad_x, p_ad_y, disp_area.getBuffer(), disp_area.width(), disp_area.height(), SSD1306_WHITE, SSD1306_BLACK);
  if (disp_mode == DISP_MODE_LANDSCAPE) {
    if (device_init_done && !firmware_update_mode && !disp_ext_fb) {
      display.drawLine(0, 0, 0, 63, SSD1306_WHITE);
    }
  }
}

void update_display(bool blank = false) {
  if (blank) {
    if (display_contrast != display_intensity) {
      display_contrast = display_intensity;
      set_contrast(&display, display_contrast);
    }
    display.clearDisplay();
    display.display();    
  } else {
    if (millis()-last_disp_update >= disp_update_interval) {
      if (display_contrast != display_intensity) {
        display_contrast = display_intensity;
        set_contrast(&display, display_contrast);
      }
      display.clearDisplay();
      update_stat_area();
      update_disp_area();
      display.display();
      last_disp_update = millis();
    }
  }
}

void ext_fb_enable() {
  disp_ext_fb = true;
}

void ext_fb_disable() {
  disp_ext_fb = false;
}
