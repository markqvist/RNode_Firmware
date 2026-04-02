// R-Watch GUI — LVGL integration for T-Watch Ultimate
// Tileview navigation with watch face, radio status, GPS, messages, settings
// Requires: CO5300.h (display), XL9555.h (power control), DRV2605.h (haptic)

#ifndef GUI_H
#define GUI_H

#if BOARD_MODEL == BOARD_TWATCH_ULT

#include <lvgl.h>
#include "soc/rtc_cntl_reg.h"

// Custom fonts: generated with lv_font_conv --no-compress from Montserrat Bold
// IMPORTANT: must use --no-compress or set LV_USE_FONT_COMPRESSED=1 in lv_conf.h
namespace _f96 {
  #include "Fonts/montserrat_bold_96.c"
}
namespace _f28 {
  #include "Fonts/montserrat_bold_28.c"
}
static const lv_font_t &font_time = _f96::montserrat_bold_96;
static const lv_font_t &font_mid  = _f28::montserrat_bold_28;

// ---------------------------------------------------------------------------
// Color palette (24-bit hex for lv_color_hex)
// ---------------------------------------------------------------------------
#define GUI_COL_BLACK   0x000000
#define GUI_COL_WHITE   0xFFFFEF   // Bone white
#define GUI_COL_DIM     0x404040   // Dividers, inactive labels
#define GUI_COL_MID     0x808080   // Secondary text
#define GUI_COL_AMBER   0xFFA500   // LoRa / radio accent
#define GUI_COL_TEAL    0x00FFC0   // GPS accent
#define GUI_COL_BLUE    0x4080FF   // BLE accent
#define GUI_COL_RED     0xFF0000   // Warnings
#define GUI_COL_GREEN   0x00FF00   // Confirmations

// ---------------------------------------------------------------------------
// Layout constants (410x502 display)
// ---------------------------------------------------------------------------
#define GUI_W             CO5300_WIDTH
#define GUI_H             CO5300_HEIGHT
#define GUI_PAD           24

// Watch face zones (adjusted for 96px time font)
#define GUI_STATUS_Y      10
#define GUI_TIME_Y        40
#define GUI_DATE_Y        150
#define GUI_RULE1_Y       190
#define GUI_COMP_Y        200
#define GUI_COMP_H        90
#define GUI_RULE2_Y       295

// ---------------------------------------------------------------------------
// LVGL core objects
// ---------------------------------------------------------------------------
static lv_display_t *gui_display = NULL;
static lv_indev_t   *gui_indev   = NULL;

// Full-frame double buffer in PSRAM — tear-free rendering.
// LVGL only re-renders dirty areas within the buffer but always
// flushes the complete frame (~18ms via DMA SPI, CPU yields).
// Two buffers: 410*502*2 = 411,640 bytes each (823KB total).
#define GUI_BUF_LINES GUI_H
static uint8_t *gui_buf1 = NULL;
static uint8_t *gui_buf2 = NULL;

// Tileview and tiles
static lv_obj_t *gui_tileview    = NULL;
static lv_obj_t *gui_tile_watch  = NULL;
static lv_obj_t *gui_tile_radio  = NULL;
static lv_obj_t *gui_tile_gps    = NULL;
static lv_obj_t *gui_tile_msg    = NULL;
static lv_obj_t *gui_tile_set    = NULL;

// Convenience alias for display_unblank invalidation
static lv_obj_t *gui_screen      = NULL;

// Watch face widgets
static lv_obj_t *gui_mode_label  = NULL;
static lv_obj_t *gui_batt_label  = NULL;
static lv_obj_t *gui_time_label  = NULL;
static lv_obj_t *gui_date_label  = NULL;
static lv_obj_t *gui_lora_value  = NULL;
static lv_obj_t *gui_lora_label  = NULL;
static lv_obj_t *gui_gps_value   = NULL;
static lv_obj_t *gui_gps_label   = NULL;
static lv_obj_t *gui_batt_value  = NULL;  // battery detail in complications
static lv_obj_t *gui_batt_detail = NULL;
static lv_obj_t *gui_step_label  = NULL;  // step count below complications

// Radio status widgets
static lv_obj_t *gui_radio_freq  = NULL;
static lv_obj_t *gui_radio_params = NULL;
static lv_obj_t *gui_radio_rssi_bar = NULL;
static lv_obj_t *gui_radio_rssi_lbl = NULL;
static lv_obj_t *gui_radio_util  = NULL;
static lv_obj_t *gui_radio_ble   = NULL;
static lv_obj_t *gui_radio_pkts  = NULL;

// Radio screen additional widgets
static lv_obj_t *gui_radio_temp = NULL;
static lv_obj_t *gui_radio_batt = NULL;

// GPS screen widgets
static lv_obj_t *gui_gps_coords  = NULL;
static lv_obj_t *gui_gps_fix     = NULL;
static lv_obj_t *gui_gps_alt     = NULL;
static lv_obj_t *gui_gps_beacon  = NULL;

// Bubble level widgets
static lv_obj_t *gui_level_ring  = NULL;   // outer circle
static lv_obj_t *gui_level_dot   = NULL;   // moving bubble
static lv_obj_t *gui_level_cross_h = NULL; // crosshair horizontal
static lv_obj_t *gui_level_cross_v = NULL; // crosshair vertical
static lv_obj_t *gui_level_angle = NULL;   // tilt angle text
#define GUI_LEVEL_SIZE  140                 // ring diameter
#define GUI_LEVEL_DOT   16                  // bubble diameter
#define GUI_LEVEL_Y     340                 // vertical position on watch face

// Touch input via function pointer (set by .ino after touch init)
typedef bool (*gui_touch_fn_t)(int16_t *x, int16_t *y);
static gui_touch_fn_t gui_touch_fn = NULL;

// Data update throttle
static uint32_t gui_last_data_update = 0;
#define GUI_DATA_UPDATE_MS 500

// Track current tile for haptic feedback
static uint8_t gui_last_tile_col = 1;
static uint8_t gui_last_tile_row = 1;
static bool gui_was_scrolling = false;

// Frame timing metrics
static uint32_t gui_frame_count = 0;

// Main loop profiling (set from .ino, read by metrics command)
uint32_t prof_radio_us = 0;
uint32_t prof_serial_us = 0;
uint32_t prof_display_us = 0;
uint32_t prof_pmu_us = 0;
uint32_t prof_gps_us = 0;
uint32_t prof_bt_us = 0;
uint32_t prof_imu_us = 0;
uint32_t prof_other_us = 0;
static uint32_t gui_flush_us_total = 0;
static uint32_t gui_flush_us_last = 0;
static uint32_t gui_render_us_last = 0;
static uint32_t gui_render_start = 0;
static uint32_t gui_loop_us_last = 0;     // time between gui_update() calls
static uint32_t gui_loop_us_max = 0;      // worst case loop time
static uint32_t gui_last_update_us = 0;

// Remote touch injection
static int16_t gui_inject_x = -1;
static int16_t gui_inject_y = -1;
static bool    gui_inject_pressed = false;
static uint32_t gui_inject_until = 0;  // millis() deadline for injected touch

// ---------------------------------------------------------------------------
// LVGL display flush callback
// ---------------------------------------------------------------------------
// Shadow framebuffer for screenshots (RGB565 swapped / big-endian — same as display)
uint16_t *gui_screenshot_buf = NULL;

// Forward declarations — defined in Display.h / Power.h / .ino after Gui.h is included
void display_unblank();
extern float pmu_temperature;
extern volatile uint32_t imu_step_count;
// Sensor logger toggle — set by .ino after IMULogger.h is included
typedef bool (*gui_log_toggle_fn_t)();
static gui_log_toggle_fn_t gui_log_toggle_fn = NULL;
// SD file listing and download — set by .ino after SD is available
typedef void (*gui_list_files_fn_t)();
static gui_list_files_fn_t gui_list_files_fn = NULL;
typedef void (*gui_download_file_fn_t)(uint8_t index);
static gui_download_file_fn_t gui_download_file_fn = NULL;
static bool gui_imu_logging = false;
// Forward declarations for IMULogger.h variables (defined later in compilation)
extern bool imu_logging;
extern uint32_t imu_log_samples;
extern uint32_t imu_log_start_ms;
// Forward declaration for touch logging (defined in IMULogger.h)
void sensor_log_touch(int16_t x, int16_t y, bool pressed);
// Forward declarations for filtered accel (defined in .ino)
extern volatile float imu_ax_f, imu_ay_f, imu_az_f;
#ifndef PMU_TEMP_MIN
#define PMU_TEMP_MIN -30
#endif
static volatile bool gui_screenshot_pending = false;  // set true to capture next frame

static void gui_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint16_t x1 = area->x1;
    uint16_t y1 = area->y1;
    uint16_t w  = area->x2 - area->x1 + 1;
    uint16_t h  = area->y2 - area->y1 + 1;
    uint16_t *pixels = (uint16_t *)px_map;

    // Copy to shadow framebuffer when screenshot capture is active
    if (gui_screenshot_buf && gui_screenshot_pending) {
        for (uint16_t row = 0; row < h; row++) {
            memcpy(&gui_screenshot_buf[(y1 + row) * GUI_W + x1],
                   &pixels[row * w], w * sizeof(uint16_t));
        }
    }

    uint32_t t0 = micros();
    co5300_push_pixels(x1, y1, w, h, pixels);
    gui_flush_us_last = micros() - t0;
    gui_flush_us_total += gui_flush_us_last;
    gui_frame_count++;
    lv_display_flush_ready(disp);
}

// ---------------------------------------------------------------------------
// LVGL touch input read callback
// ---------------------------------------------------------------------------
static void gui_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    // Check for injected remote touch first
    if (gui_inject_pressed && millis() < gui_inject_until) {
        data->point.x = gui_inject_x;
        data->point.y = gui_inject_y;
        data->state = LV_INDEV_STATE_PRESSED;
        last_unblank_event = millis();
        return;
    }
    gui_inject_pressed = false;

    // Real touch hardware
    int16_t tx, ty;
    if (gui_touch_fn && gui_touch_fn(&tx, &ty)) {
        data->point.x = tx;
        data->point.y = ty;
        data->state = LV_INDEV_STATE_PRESSED;
        last_unblank_event = millis();
        #if HAS_SD
          sensor_log_touch(tx, ty, true);
        #endif
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void gui_set_touch_handler(gui_touch_fn_t fn) {
    gui_touch_fn = fn;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void gui_style_black_container(lv_obj_t *obj) {
    lv_obj_set_style_bg_color(obj, lv_color_hex(GUI_COL_BLACK), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t *gui_create_rule(lv_obj_t *parent, lv_coord_t y) {
    static lv_point_precise_t rule_pts[] = {
        {GUI_PAD, 0}, {GUI_W - GUI_PAD, 0}
    };
    lv_obj_t *line = lv_line_create(parent);
    lv_line_set_points(line, rule_pts, 2);
    lv_obj_set_style_line_color(line, lv_color_hex(GUI_COL_DIM), 0);
    lv_obj_set_style_line_width(line, 1, 0);
    lv_obj_set_pos(line, 0, y);
    return line;
}

static lv_obj_t *gui_label(lv_obj_t *parent, const lv_font_t *font,
                            uint32_t color, const char *text) {
    lv_obj_t *lbl = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(color), 0);
    lv_label_set_text(lbl, text);
    return lbl;
}

static lv_obj_t *gui_label_at(lv_obj_t *parent, const lv_font_t *font,
                               uint32_t color, const char *text,
                               lv_coord_t x, lv_coord_t y) {
    lv_obj_t *lbl = gui_label(parent, font, color, text);
    lv_obj_set_pos(lbl, x, y);
    return lbl;
}

static void gui_create_complication(lv_obj_t *parent, lv_coord_t x, lv_coord_t w,
                                     uint32_t color, const char *label_text,
                                     lv_obj_t **value_out, lv_obj_t **label_out) {
    lv_obj_t *cell = lv_obj_create(parent);
    lv_obj_remove_style_all(cell);
    lv_obj_set_size(cell, w, GUI_COMP_H);
    lv_obj_set_pos(cell, x, 0);

    lv_obj_t *val = gui_label(cell, &font_mid, color, "--");
    lv_obj_set_width(val, w);
    lv_obj_set_style_text_align(val, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(val, LV_ALIGN_TOP_MID, 0, 4);

    lv_obj_t *lbl = gui_label(cell, &lv_font_montserrat_14, GUI_COL_DIM, label_text);
    lv_obj_set_width(lbl, w);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 46);

    if (value_out) *value_out = val;
    if (label_out) *label_out = lbl;
}

// ---------------------------------------------------------------------------
// Screen: Watch Face (center tile)
// ---------------------------------------------------------------------------
static void gui_create_watchface(lv_obj_t *parent) {
    gui_style_black_container(parent);

    // Status bar
    gui_mode_label = gui_label_at(parent, &lv_font_montserrat_14, GUI_COL_MID, "IDLE", GUI_PAD, GUI_STATUS_Y);
    gui_batt_label = gui_label(parent, &lv_font_montserrat_14, GUI_COL_MID, "--%");
    lv_obj_align(gui_batt_label, LV_ALIGN_TOP_RIGHT, -GUI_PAD, GUI_STATUS_Y);

    // Time (96px custom font — digits and colon only)
    gui_time_label = gui_label(parent, &font_time, GUI_COL_WHITE, "00:00");
    lv_obj_set_style_text_letter_space(gui_time_label, 2, 0);
    lv_obj_set_width(gui_time_label, GUI_W);
    lv_obj_set_style_text_align(gui_time_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(gui_time_label, 0, GUI_TIME_Y);

    // Date
    gui_date_label = gui_label(parent, &font_mid, GUI_COL_MID, "--- -- ---");
    lv_obj_set_width(gui_date_label, GUI_W);
    lv_obj_set_style_text_align(gui_date_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(gui_date_label, 0, GUI_DATE_Y);

    // Rule 1
    gui_create_rule(parent, GUI_RULE1_Y);

    // Complications
    lv_obj_t *comp = lv_obj_create(parent);
    lv_obj_remove_style_all(comp);
    lv_obj_set_size(comp, GUI_W, GUI_COMP_H);
    lv_obj_set_pos(comp, 0, GUI_COMP_Y);
    lv_obj_clear_flag(comp, LV_OBJ_FLAG_SCROLLABLE);

    int cw = (GUI_W - GUI_PAD * 2) / 3;
    gui_create_complication(comp, GUI_PAD,          cw, GUI_COL_AMBER, "LoRa",  &gui_lora_value,  &gui_lora_label);
    gui_create_complication(comp, GUI_PAD + cw,     cw, GUI_COL_TEAL,  "GPS",   &gui_gps_value,   &gui_gps_label);
    gui_create_complication(comp, GUI_PAD + cw * 2, cw, GUI_COL_WHITE, "Batt", &gui_batt_value, &gui_batt_detail);

    // Rule 2
    gui_create_rule(parent, GUI_RULE2_Y);

    // Step counter below complications
    gui_step_label = gui_label(parent, &lv_font_montserrat_20, GUI_COL_DIM, "");
    lv_obj_set_width(gui_step_label, GUI_W);
    lv_obj_set_style_text_align(gui_step_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(gui_step_label, 0, GUI_RULE2_Y + 15);

    // Bubble level
    int lx = (GUI_W - GUI_LEVEL_SIZE) / 2;

    // Outer ring
    gui_level_ring = lv_obj_create(parent);
    lv_obj_remove_style_all(gui_level_ring);
    lv_obj_set_size(gui_level_ring, GUI_LEVEL_SIZE, GUI_LEVEL_SIZE);
    lv_obj_set_pos(gui_level_ring, lx, GUI_LEVEL_Y);
    lv_obj_set_style_radius(gui_level_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(gui_level_ring, lv_color_hex(GUI_COL_DIM), 0);
    lv_obj_set_style_border_width(gui_level_ring, 1, 0);
    lv_obj_set_style_bg_opa(gui_level_ring, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(gui_level_ring, LV_OBJ_FLAG_SCROLLABLE);

    // Crosshairs
    static lv_point_precise_t ch_pts[] = {{0,0},{GUI_LEVEL_SIZE,0}};
    gui_level_cross_h = lv_line_create(gui_level_ring);
    lv_line_set_points(gui_level_cross_h, ch_pts, 2);
    lv_obj_set_style_line_color(gui_level_cross_h, lv_color_hex(0x202020), 0);
    lv_obj_set_style_line_width(gui_level_cross_h, 1, 0);
    lv_obj_center(gui_level_cross_h);

    static lv_point_precise_t cv_pts[] = {{0,0},{0,GUI_LEVEL_SIZE}};
    gui_level_cross_v = lv_line_create(gui_level_ring);
    lv_line_set_points(gui_level_cross_v, cv_pts, 2);
    lv_obj_set_style_line_color(gui_level_cross_v, lv_color_hex(0x202020), 0);
    lv_obj_set_style_line_width(gui_level_cross_v, 1, 0);
    lv_obj_center(gui_level_cross_v);

    // Bubble dot
    gui_level_dot = lv_obj_create(gui_level_ring);
    lv_obj_remove_style_all(gui_level_dot);
    lv_obj_set_size(gui_level_dot, GUI_LEVEL_DOT, GUI_LEVEL_DOT);
    lv_obj_set_style_radius(gui_level_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(gui_level_dot, lv_color_hex(GUI_COL_GREEN), 0);
    lv_obj_set_style_bg_opa(gui_level_dot, LV_OPA_COVER, 0);
    lv_obj_center(gui_level_dot);

    // Angle text below ring
    gui_level_angle = gui_label(parent, &lv_font_montserrat_14, GUI_COL_DIM, "");
    lv_obj_set_width(gui_level_angle, GUI_W);
    lv_obj_set_style_text_align(gui_level_angle, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(gui_level_angle, 0, GUI_LEVEL_Y + GUI_LEVEL_SIZE + 5);
}

// ---------------------------------------------------------------------------
// Screen: Radio Status (top tile — swipe down from watch face)
// ---------------------------------------------------------------------------
static void gui_create_radio_screen(lv_obj_t *parent) {
    gui_style_black_container(parent);

    gui_label_at(parent, &lv_font_montserrat_14, GUI_COL_DIM, "RADIO STATUS", GUI_PAD, 12);

    // Frequency
    gui_radio_freq = gui_label_at(parent, &font_mid, GUI_COL_AMBER, "--- MHz", GUI_PAD, 40);

    // LoRa parameters
    gui_radio_params = gui_label_at(parent, &font_mid, GUI_COL_MID, "SF- BW- CR-", GUI_PAD, 80);

    // RSSI
    gui_create_rule(parent, 115);
    gui_label_at(parent, &lv_font_montserrat_14, GUI_COL_DIM, "RSSI", GUI_PAD, 125);
    gui_radio_rssi_lbl = gui_label(parent, &font_mid, GUI_COL_AMBER, "---");
    lv_obj_align(gui_radio_rssi_lbl, LV_ALIGN_TOP_RIGHT, -GUI_PAD, 122);

    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_set_size(bar, GUI_W - GUI_PAD * 2, 20);
    lv_obj_set_pos(bar, GUI_PAD, 150);
    lv_bar_set_range(bar, -140, -40);
    lv_bar_set_value(bar, -140, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, lv_color_hex(GUI_COL_DIM), 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(GUI_COL_AMBER), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    gui_radio_rssi_bar = bar;

    // Channel utilization
    gui_create_rule(parent, 185);
    gui_label_at(parent, &lv_font_montserrat_14, GUI_COL_DIM, "CHANNEL", GUI_PAD, 198);
    gui_radio_util = gui_label(parent, &font_mid, GUI_COL_MID, "-- %");
    lv_obj_align(gui_radio_util, LV_ALIGN_TOP_RIGHT, -GUI_PAD, 195);

    // BLE status
    gui_create_rule(parent, 230);
    gui_label_at(parent, &lv_font_montserrat_14, GUI_COL_DIM, "BLE", GUI_PAD, 243);
    gui_radio_ble = gui_label(parent, &font_mid, GUI_COL_BLUE, "---");
    lv_obj_align(gui_radio_ble, LV_ALIGN_TOP_RIGHT, -GUI_PAD, 240);

    // Packet counts
    gui_create_rule(parent, 275);
    gui_radio_pkts = gui_label_at(parent, &font_mid, GUI_COL_MID,
                                   "RX: 0  TX: 0", GUI_PAD, 290);

    // Battery and temperature
    gui_create_rule(parent, 325);
    gui_label_at(parent, &lv_font_montserrat_14, GUI_COL_DIM, "BATTERY", GUI_PAD, 338);
    gui_radio_batt = gui_label(parent, &font_mid, GUI_COL_MID, "---");
    lv_obj_align(gui_radio_batt, LV_ALIGN_TOP_RIGHT, -GUI_PAD, 335);

    gui_create_rule(parent, 370);
    gui_label_at(parent, &lv_font_montserrat_14, GUI_COL_DIM, "TEMPERATURE", GUI_PAD, 383);
    gui_radio_temp = gui_label(parent, &font_mid, GUI_COL_MID, "---");
    lv_obj_align(gui_radio_temp, LV_ALIGN_TOP_RIGHT, -GUI_PAD, 380);
}

// ---------------------------------------------------------------------------
// Screen: GPS (right tile — swipe right from watch face)
// ---------------------------------------------------------------------------
static void gui_create_gps_screen(lv_obj_t *parent) {
    gui_style_black_container(parent);

    lv_obj_t *gps_title = gui_label(parent, &lv_font_montserrat_14, GUI_COL_DIM, "GPS");
    lv_obj_set_width(gps_title, GUI_W);
    lv_obj_set_style_text_align(gps_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(gps_title, 0, 12);

    // Coordinates — centered for rounded corner clearance
    gui_gps_coords = gui_label(parent, &font_mid, GUI_COL_TEAL, "-- --");
    lv_obj_set_width(gui_gps_coords, GUI_W);
    lv_obj_set_style_text_align(gui_gps_coords, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(gui_gps_coords, 0, 40);

    // Fix quality
    gui_create_rule(parent, 110);
    gui_gps_fix = gui_label_at(parent, &font_mid, GUI_COL_MID,
                                "Sats: --  HDOP: --", GUI_PAD, 125);

    // Altitude and speed
    gui_create_rule(parent, 160);
    gui_gps_alt = gui_label_at(parent, &font_mid, GUI_COL_MID,
                                "Alt: --  Spd: --", GUI_PAD, 175);

    // Beacon status
    gui_create_rule(parent, 215);
    gui_gps_beacon = gui_label_at(parent, &font_mid, GUI_COL_AMBER,
                                   "Beacon: --", GUI_PAD, 230);
}

// ---------------------------------------------------------------------------
// Screen: Messages (left tile — swipe left from watch face)
// ---------------------------------------------------------------------------
static void gui_create_msg_screen(lv_obj_t *parent) {
    gui_style_black_container(parent);
    gui_label_at(parent, &lv_font_montserrat_14, GUI_COL_DIM, "MESSAGES", GUI_PAD, 12);
    lv_obj_t *lbl = gui_label(parent, &font_mid, GUI_COL_DIM, "No messages");
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
}

// ---------------------------------------------------------------------------
// Screen: Settings (bottom tile — swipe up from watch face)
// ---------------------------------------------------------------------------
static lv_obj_t *gui_set_disp_slider = NULL;
static lv_obj_t *gui_set_disp_val = NULL;
static lv_obj_t *gui_set_bcn_roller = NULL;
static lv_obj_t *gui_set_gps_roller = NULL;
static lv_obj_t *gui_set_bcn_sw = NULL;
static lv_obj_t *gui_set_log_sw = NULL;
static lv_obj_t *gui_set_log_status = NULL;

static void gui_set_disp_cb(lv_event_t *e) {
    lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
    int32_t val = lv_slider_get_value(slider);
    display_blanking_timeout = (uint32_t)val * 1000;
    char buf[8]; snprintf(buf, sizeof(buf), "%lds", (long)val);
    lv_label_set_text(gui_set_disp_val, buf);
    EEPROM.write(config_addr(ADDR_CONF_DISP_TIMEOUT), (uint8_t)val);
    EEPROM.commit();
}

static void gui_set_bcn_en_cb(lv_event_t *e) {
    lv_obj_t *sw = (lv_obj_t *)lv_event_get_target(e);
    beacon_enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    EEPROM.write(config_addr(ADDR_CONF_BCN_EN), beacon_enabled ? 1 : 0);
    EEPROM.commit();
}

static void gui_set_bcn_int_cb(lv_event_t *e) {
    lv_obj_t *roller = (lv_obj_t *)lv_event_get_target(e);
    uint16_t idx = lv_roller_get_selected(roller);
    if (idx < BEACON_INTERVAL_OPTIONS_COUNT) {
        beacon_interval_ms = beacon_interval_options[idx];
        EEPROM.write(config_addr(ADDR_CONF_BCN_INT), (uint8_t)idx);
        EEPROM.commit();
    }
}

static void gui_set_gps_model_cb(lv_event_t *e) {
    lv_obj_t *roller = (lv_obj_t *)lv_event_get_target(e);
    uint16_t idx = lv_roller_get_selected(roller);
    if (idx < GPS_MODEL_OPTIONS_COUNT) {
        gps_set_dynamic_model(idx);
        EEPROM.write(config_addr(ADDR_CONF_GPS_MODEL), (uint8_t)idx);
        EEPROM.commit();
    }
}

static void gui_set_log_cb(lv_event_t *e) {
    bool on = lv_obj_has_state(gui_set_log_sw, LV_STATE_CHECKED);
    if (on) {
        if (gui_log_toggle_fn) gui_log_toggle_fn();
    } else {
        if (gui_log_toggle_fn && imu_logging) gui_log_toggle_fn();
    }
}

static void gui_create_settings_screen(lv_obj_t *parent) {
    gui_style_black_container(parent);

    // Child container for settings content — do NOT set flex on the tile itself
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, GUI_W, GUI_H);
    lv_obj_set_style_bg_color(cont, lv_color_hex(GUI_COL_BLACK), 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    gui_label_at(cont, &lv_font_montserrat_14, GUI_COL_DIM, "SETTINGS", GUI_PAD, 12);

    // --- Row 1: Display timeout (y=50) ---
    gui_label_at(cont, &lv_font_montserrat_14, GUI_COL_MID, "Display timeout", GUI_PAD, 55);
    gui_set_disp_slider = lv_slider_create(cont);
    lv_obj_set_size(gui_set_disp_slider, 180, 12);
    lv_obj_set_pos(gui_set_disp_slider, GUI_PAD, 80);
    lv_slider_set_range(gui_set_disp_slider, 5, 60);
    lv_slider_set_value(gui_set_disp_slider, (int32_t)(display_blanking_timeout / 1000), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(gui_set_disp_slider, lv_color_hex(GUI_COL_DIM), 0);
    lv_obj_set_style_bg_color(gui_set_disp_slider, lv_color_hex(GUI_COL_AMBER), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(gui_set_disp_slider, lv_color_hex(GUI_COL_WHITE), LV_PART_KNOB);
    lv_obj_set_style_pad_all(gui_set_disp_slider, 4, LV_PART_KNOB);
    lv_obj_add_event_cb(gui_set_disp_slider, gui_set_disp_cb, LV_EVENT_VALUE_CHANGED, NULL);
    char disp_buf[8]; snprintf(disp_buf, sizeof(disp_buf), "%lds", (long)(display_blanking_timeout / 1000));
    gui_set_disp_val = gui_label_at(cont, &lv_font_montserrat_14, GUI_COL_WHITE, disp_buf, GUI_PAD + 200, 75);

    gui_create_rule(cont, 110);

    // --- Row 2: Beacon enable (y=120) ---
    gui_label_at(cont, &lv_font_montserrat_14, GUI_COL_MID, "Beacon", GUI_PAD, 125);
    gui_set_bcn_sw = lv_switch_create(cont);
    lv_obj_set_pos(gui_set_bcn_sw, GUI_W - GUI_PAD - 50, 120);
    lv_obj_set_size(gui_set_bcn_sw, 50, 26);
    if (beacon_enabled) lv_obj_add_state(gui_set_bcn_sw, LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(gui_set_bcn_sw, lv_color_hex(GUI_COL_DIM), 0);
    lv_obj_set_style_bg_color(gui_set_bcn_sw, lv_color_hex(GUI_COL_AMBER), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_event_cb(gui_set_bcn_sw, gui_set_bcn_en_cb, LV_EVENT_VALUE_CHANGED, NULL);

    gui_create_rule(cont, 160);

    // --- Row 3: Beacon interval (y=170) ---
    gui_label_at(cont, &lv_font_montserrat_14, GUI_COL_MID, "Beacon interval", GUI_PAD, 175);
    gui_set_bcn_roller = lv_roller_create(cont);
    lv_roller_set_options(gui_set_bcn_roller, "10s\n30s\n1min\n5min\n10min", LV_ROLLER_MODE_NORMAL);
    lv_obj_set_pos(gui_set_bcn_roller, GUI_W - GUI_PAD - 100, 170);
    lv_obj_set_size(gui_set_bcn_roller, 100, 60);
    lv_obj_set_style_bg_color(gui_set_bcn_roller, lv_color_hex(GUI_COL_BLACK), 0);
    lv_obj_set_style_text_color(gui_set_bcn_roller, lv_color_hex(GUI_COL_WHITE), 0);
    lv_obj_set_style_text_font(gui_set_bcn_roller, &lv_font_montserrat_14, 0);
    lv_obj_set_style_bg_color(gui_set_bcn_roller, lv_color_hex(GUI_COL_AMBER), LV_PART_SELECTED);
    lv_obj_set_style_text_color(gui_set_bcn_roller, lv_color_hex(GUI_COL_BLACK), LV_PART_SELECTED);
    // Set initial selection from current beacon_interval_ms
    for (uint8_t i = 0; i < BEACON_INTERVAL_OPTIONS_COUNT; i++) {
        if (beacon_interval_options[i] == beacon_interval_ms) {
            lv_roller_set_selected(gui_set_bcn_roller, i, LV_ANIM_OFF);
            break;
        }
    }
    lv_obj_add_event_cb(gui_set_bcn_roller, gui_set_bcn_int_cb, LV_EVENT_VALUE_CHANGED, NULL);

    gui_create_rule(cont, 245);

    // --- Row 4: GPS dynamic model (y=255) ---
    gui_label_at(cont, &lv_font_montserrat_14, GUI_COL_MID, "GPS model", GUI_PAD, 260);
    gui_set_gps_roller = lv_roller_create(cont);
    lv_roller_set_options(gui_set_gps_roller, "Portable\nStationary\nPedestrian\nAutomotive", LV_ROLLER_MODE_NORMAL);
    lv_obj_set_pos(gui_set_gps_roller, GUI_W - GUI_PAD - 140, 255);
    lv_obj_set_size(gui_set_gps_roller, 140, 60);
    lv_obj_set_style_bg_color(gui_set_gps_roller, lv_color_hex(GUI_COL_BLACK), 0);
    lv_obj_set_style_text_color(gui_set_gps_roller, lv_color_hex(GUI_COL_WHITE), 0);
    lv_obj_set_style_text_font(gui_set_gps_roller, &lv_font_montserrat_14, 0);
    lv_obj_set_style_bg_color(gui_set_gps_roller, lv_color_hex(GUI_COL_AMBER), LV_PART_SELECTED);
    lv_obj_set_style_text_color(gui_set_gps_roller, lv_color_hex(GUI_COL_BLACK), LV_PART_SELECTED);
    lv_roller_set_selected(gui_set_gps_roller, gps_dynamic_model, LV_ANIM_OFF);
    lv_obj_add_event_cb(gui_set_gps_roller, gui_set_gps_model_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // --- Sensor logger ---
    gui_create_rule(cont, 320);
    gui_label_at(cont, &lv_font_montserrat_14, GUI_COL_MID, "Data logger", GUI_PAD, 335);
    gui_set_log_sw = lv_switch_create(cont);
    lv_obj_set_pos(gui_set_log_sw, GUI_W - GUI_PAD - 50, 330);
    lv_obj_set_size(gui_set_log_sw, 50, 26);
    lv_obj_set_style_bg_color(gui_set_log_sw, lv_color_hex(GUI_COL_DIM), 0);
    lv_obj_set_style_bg_color(gui_set_log_sw, lv_color_hex(GUI_COL_GREEN), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_event_cb(gui_set_log_sw, gui_set_log_cb, LV_EVENT_VALUE_CHANGED, NULL);
    gui_set_log_status = gui_label_at(cont, &lv_font_montserrat_14, GUI_COL_DIM, "", GUI_PAD, 365);
}

// ---------------------------------------------------------------------------
// Tileview change event — haptic feedback
// ---------------------------------------------------------------------------
static void gui_tile_change_cb(lv_event_t *e) {
    lv_obj_t *tv = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_t *tile = (lv_obj_t *)lv_tileview_get_tile_active(tv);
    if (!tile) return;

    // Get tile position
    lv_coord_t col = lv_obj_get_x(tile) / GUI_W;
    lv_coord_t row = lv_obj_get_y(tile) / GUI_H;

    if (col != gui_last_tile_col || row != gui_last_tile_row) {
        gui_last_tile_col = col;
        gui_last_tile_row = row;
        #if defined(DRV2605_H)
        if (drv2605_ready) drv2605_play(HAPTIC_TRANSITION);
        #endif
    }
}

// ---------------------------------------------------------------------------
// Update all screen data from firmware globals
// ---------------------------------------------------------------------------
static const char *gui_month_names[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                         "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

static bool gui_is_scrolling() {
    if (!gui_tileview) return false;
    lv_obj_t *tile = (lv_obj_t *)lv_tileview_get_tile_active(gui_tileview);
    if (!tile) return false;
    // Check if scroll position doesn't match tile alignment
    lv_coord_t sx = lv_obj_get_scroll_x(gui_tileview);
    lv_coord_t sy = lv_obj_get_scroll_y(gui_tileview);
    lv_coord_t tx = lv_obj_get_x(tile);
    lv_coord_t ty = lv_obj_get_y(tile);
    return (sx != tx || sy != ty);
}

static void gui_update_data() {
    if (!gui_time_label) return;

    // Skip data updates during scroll animation — frees CPU for rendering
    if (gui_is_scrolling()) return;

    uint32_t now = millis();
    if (now - gui_last_data_update < GUI_DATA_UPDATE_MS) return;
    gui_last_data_update = now;

    // Detect current tile — only update visible screen's labels
    lv_obj_t *cur_tile = (lv_obj_t *)lv_tileview_get_tile_active(gui_tileview);
    bool on_watch  = (cur_tile == gui_tile_watch);
    bool on_radio  = (cur_tile == gui_tile_radio);
    bool on_gps    = (cur_tile == gui_tile_gps);
    bool on_settings = (cur_tile == gui_tile_set);

    // ---- Watch face ----
    // Time always updates (cheap, changes rarely)
    lv_label_set_text_fmt(gui_time_label, "%02d:%02d", rtc_hour, rtc_minute);

    if (!on_watch && !on_radio && !on_gps && !on_settings) return;

    // ---- Watch face details (only when visible) ----
    if (on_watch) {

    #if HAS_RTC == true
    if (rtc_year > 0) {
        const char *mon = (rtc_month >= 1 && rtc_month <= 12) ? gui_month_names[rtc_month - 1] : "---";
        lv_label_set_text_fmt(gui_date_label, "%d %s %d", rtc_day, mon, rtc_year);
    }
    #endif

    // Mode indicator
    if (bt_state == BT_STATE_CONNECTED) {
        lv_label_set_text(gui_mode_label, "MODEM");
        lv_obj_set_style_text_color(gui_mode_label, lv_color_hex(GUI_COL_BLUE), 0);
    }
    #if HAS_GPS == true
    else if (beacon_mode_active) {
        lv_label_set_text(gui_mode_label, "BEACON");
        lv_obj_set_style_text_color(gui_mode_label, lv_color_hex(GUI_COL_AMBER), 0);
    }
    #endif
    else if (radio_online) {
        lv_label_set_text(gui_mode_label, "RADIO");
        lv_obj_set_style_text_color(gui_mode_label, lv_color_hex(GUI_COL_MID), 0);
    } else {
        lv_label_set_text(gui_mode_label, "IDLE");
        lv_obj_set_style_text_color(gui_mode_label, lv_color_hex(GUI_COL_DIM), 0);
    }

    // Battery
    if (battery_state == BATTERY_STATE_CHARGING) {
        lv_label_set_text_fmt(gui_batt_label, "%d%% +", (int)battery_percent);
    } else {
        lv_label_set_text_fmt(gui_batt_label, "%d%%", (int)battery_percent);
    }
    lv_obj_align(gui_batt_label, LV_ALIGN_TOP_RIGHT, -GUI_PAD, GUI_STATUS_Y);

    // LoRa complication
    if (radio_online) {
        if (last_rssi > -292) {
            lv_label_set_text_fmt(gui_lora_value, "%d", last_rssi);
        } else {
            lv_label_set_text(gui_lora_value, "---");
        }
        lv_obj_set_style_text_color(gui_lora_value, lv_color_hex(GUI_COL_AMBER), 0);
    } else {
        lv_label_set_text(gui_lora_value, "OFF");
        lv_obj_set_style_text_color(gui_lora_value, lv_color_hex(GUI_COL_DIM), 0);
    }

    // GPS complication — color by fix quality
    #if HAS_GPS == true
    if (gps_sats > 0) {
        lv_label_set_text_fmt(gui_gps_value, "%d sats", gps_sats);
        uint32_t gps_col = (gps_hdop < 5.0) ? GUI_COL_TEAL :
                           (gps_hdop < 15.0) ? GUI_COL_AMBER : GUI_COL_MID;
        lv_obj_set_style_text_color(gui_gps_value, lv_color_hex(gps_col), 0);
    } else {
        lv_label_set_text(gui_gps_value, "no fix");
        lv_obj_set_style_text_color(gui_gps_value, lv_color_hex(GUI_COL_DIM), 0);
    }
    #endif

    // Step counter
    if (gui_step_label) {
        if (imu_step_count > 0) {
            lv_label_set_text_fmt(gui_step_label, "%lu steps", imu_step_count);
            lv_obj_set_style_text_color(gui_step_label, lv_color_hex(GUI_COL_MID), 0);
        } else {
            lv_label_set_text(gui_step_label, "");
        }
    }

    // Bubble level — spring-damper physics with non-linear sensitivity
    if (gui_level_dot && imu_az_f != 0) {
        static float bub_x = 0, bub_y = 0;   // current bubble position (pixels)
        static float vel_x = 0, vel_y = 0;   // bubble velocity
        static uint32_t bub_t = 0;            // last update time

        uint32_t now_ms = millis();
        float dt = (bub_t > 0) ? (now_ms - bub_t) / 1000.0f : 0.016f;
        if (dt > 0.1f) dt = 0.1f;  // clamp for first frame / pauses
        bub_t = now_ms;

        float max_r = (GUI_LEVEL_SIZE - GUI_LEVEL_DOT) / 2.0f;

        // Tilt vector from accelerometer (radians)
        float tilt_x = atan2f(imu_ax_f, imu_az_f);
        float tilt_y = atan2f(imu_ay_f, imu_az_f);

        // Work in polar: magnitude + direction
        float tilt_r = sqrtf(tilt_x * tilt_x + tilt_y * tilt_y);
        float tilt_dir_x = (tilt_r > 0.001f) ? tilt_x / tilt_r : 0;
        float tilt_dir_y = (tilt_r > 0.001f) ? tilt_y / tilt_r : 0;

        // Non-linear radial mapping: tanh compresses large tilts,
        // amplifies small ones. k=3 → full ring at ~20° tilt
        float mapped_r = tanhf(tilt_r * 3.0f) * max_r;

        // Project back to cartesian (invert for natural bubble feel)
        float target_x = -tilt_dir_x * mapped_r;
        float target_y = -tilt_dir_y * mapped_r;

        // Spring-damper: overdamped for viscous fluid feel
        // Sub-step at 20ms to keep integration stable at any frame rate
        const float spring = 40.0f;
        const float damping = 12.0f;
        const float max_sub = 0.02f;
        int steps = (int)(dt / max_sub) + 1;
        float sdt = dt / steps;
        for (int si = 0; si < steps; si++) {
            vel_x += (spring * (target_x - bub_x) - damping * vel_x) * sdt;
            vel_y += (spring * (target_y - bub_y) - damping * vel_y) * sdt;
            bub_x += vel_x * sdt;
            bub_y += vel_y * sdt;
        }

        // Clamp to ring boundary (bubble can't escape the fluid)
        float dist = sqrtf(bub_x * bub_x + bub_y * bub_y);
        if (dist > max_r) {
            bub_x = bub_x * max_r / dist;
            bub_y = bub_y * max_r / dist;
            // Kill velocity component along the wall
            float nx = bub_x / dist, ny = bub_y / dist;
            float vdot = vel_x * nx + vel_y * ny;
            if (vdot > 0) { vel_x -= vdot * nx; vel_y -= vdot * ny; }
        }

        lv_obj_set_pos(gui_level_dot,
            (int)(GUI_LEVEL_SIZE / 2 - GUI_LEVEL_DOT / 2 + bub_x),
            (int)(GUI_LEVEL_SIZE / 2 - GUI_LEVEL_DOT / 2 + bub_y));

        // Tilt angle for display and colour
        float tx = imu_ax_f / 4096.0f, ty = imu_ay_f / 4096.0f;
        float tilt_deg = atan2f(sqrtf(tx*tx + ty*ty), fabsf(imu_az_f / 4096.0f)) * 57.2958f;
        uint32_t dot_col = (tilt_deg < 2.0f) ? GUI_COL_GREEN :
                           (tilt_deg < 10.0f) ? GUI_COL_AMBER : GUI_COL_RED;
        lv_obj_set_style_bg_color(gui_level_dot, lv_color_hex(dot_col), 0);

        lv_label_set_text_fmt(gui_level_angle, "%.1f\xC2\xB0", tilt_deg);
        lv_obj_set_style_text_color(gui_level_angle, lv_color_hex(dot_col), 0);
    }

    // Battery complication — voltage and state
    if (battery_state == BATTERY_STATE_CHARGING) {
        lv_label_set_text_fmt(gui_batt_value, "%.2fV", battery_voltage);
        lv_obj_set_style_text_color(gui_batt_value, lv_color_hex(GUI_COL_GREEN), 0);
        lv_label_set_text(gui_batt_detail, "Charging");
        lv_obj_set_style_text_color(gui_batt_detail, lv_color_hex(GUI_COL_GREEN), 0);
    } else if (battery_state == BATTERY_STATE_CHARGED) {
        lv_label_set_text_fmt(gui_batt_value, "%.2fV", battery_voltage);
        lv_obj_set_style_text_color(gui_batt_value, lv_color_hex(GUI_COL_GREEN), 0);
        lv_label_set_text(gui_batt_detail, "Full");
        lv_obj_set_style_text_color(gui_batt_detail, lv_color_hex(GUI_COL_GREEN), 0);
    } else if (battery_percent < 15) {
        lv_label_set_text_fmt(gui_batt_value, "%.2fV", battery_voltage);
        lv_obj_set_style_text_color(gui_batt_value, lv_color_hex(GUI_COL_RED), 0);
        lv_label_set_text(gui_batt_detail, "Low");
        lv_obj_set_style_text_color(gui_batt_detail, lv_color_hex(GUI_COL_RED), 0);
    } else {
        lv_label_set_text_fmt(gui_batt_value, "%.2fV", battery_voltage);
        lv_obj_set_style_text_color(gui_batt_value, lv_color_hex(GUI_COL_WHITE), 0);
        lv_label_set_text(gui_batt_detail, "Batt");
        lv_obj_set_style_text_color(gui_batt_detail, lv_color_hex(GUI_COL_DIM), 0);
    }

    } // end on_watch

    // ---- Radio status screen (only when visible) ----
    if (on_radio && gui_radio_freq) {
        if (lora_freq > 0) {
            lv_label_set_text_fmt(gui_radio_freq, "%.3f MHz", (float)lora_freq / 1000000.0);
        } else {
            lv_label_set_text(gui_radio_freq, "--- MHz");
        }

        lv_label_set_text_fmt(gui_radio_params, "SF%d  BW%lu  CR4/%d",
                              lora_sf, lora_bw / 1000, lora_cr);

        if (radio_online && last_rssi > -292) {
            lv_bar_set_value(gui_radio_rssi_bar, last_rssi, LV_ANIM_ON);
            lv_label_set_text_fmt(gui_radio_rssi_lbl, "%d dBm", last_rssi);
        } else {
            lv_bar_set_value(gui_radio_rssi_bar, -140, LV_ANIM_OFF);
            lv_label_set_text(gui_radio_rssi_lbl, "---");
        }

        lv_label_set_text_fmt(gui_radio_util, "%.1f%%",
                              (float)local_channel_util / 100.0);

        if (bt_state == BT_STATE_CONNECTED) {
            lv_label_set_text(gui_radio_ble, "Connected");
            lv_obj_set_style_text_color(gui_radio_ble, lv_color_hex(GUI_COL_BLUE), 0);
        } else if (bt_state == BT_STATE_ON) {
            lv_label_set_text(gui_radio_ble, "Advertising");
            lv_obj_set_style_text_color(gui_radio_ble, lv_color_hex(GUI_COL_MID), 0);
        } else {
            lv_label_set_text(gui_radio_ble, "Off");
            lv_obj_set_style_text_color(gui_radio_ble, lv_color_hex(GUI_COL_DIM), 0);
        }

        lv_label_set_text_fmt(gui_radio_pkts, "RX: %lu  TX: %lu", stat_rx, stat_tx);

        // Battery detail
        if (gui_radio_batt) {
            lv_label_set_text_fmt(gui_radio_batt, "%.2fV  %d%%", battery_voltage, (int)battery_percent);
            lv_obj_align(gui_radio_batt, LV_ALIGN_TOP_RIGHT, -GUI_PAD, 335);
        }

        // Temperature
        if (gui_radio_temp) {
            if (pmu_temperature > (PMU_TEMP_MIN - 1)) {
                lv_label_set_text_fmt(gui_radio_temp, "%.1f C", pmu_temperature);
            } else {
                lv_label_set_text(gui_radio_temp, "---");
            }
            lv_obj_align(gui_radio_temp, LV_ALIGN_TOP_RIGHT, -GUI_PAD, 380);
        }
    }

    // ---- GPS screen (only when visible — float formatting is expensive) ----
    #if HAS_GPS == true
    if (on_gps && gui_gps_coords) {
        bool good_fix = (gps_sats >= 4 && gps_hdop < 10.0 && gps_lat != 0.0);
        bool any_fix  = (gps_sats > 0 && gps_lat != 0.0);

        // Coordinates — show when any fix, but dim when HDOP is poor
        if (any_fix) {
            lv_label_set_text_fmt(gui_gps_coords, "%.6f\n%.6f", gps_lat, gps_lon);
            lv_obj_set_style_text_color(gui_gps_coords,
                lv_color_hex(good_fix ? GUI_COL_TEAL : GUI_COL_MID), 0);
        } else {
            lv_label_set_text(gui_gps_coords, "No fix");
            lv_obj_set_style_text_color(gui_gps_coords, lv_color_hex(GUI_COL_DIM), 0);
        }

        // Fix quality — color HDOP by quality
        uint32_t hdop_col = (gps_hdop < 2.0) ? GUI_COL_GREEN :
                            (gps_hdop < 5.0) ? GUI_COL_TEAL :
                            (gps_hdop < 10.0) ? GUI_COL_AMBER : GUI_COL_RED;
        lv_label_set_text_fmt(gui_gps_fix, "Sats: %d  HDOP: %.1f", gps_sats, gps_hdop);
        lv_obj_set_style_text_color(gui_gps_fix, lv_color_hex(hdop_col), 0);

        // Alt/Speed — suppress speed when HDOP is poor (it's just noise)
        if (good_fix) {
            lv_label_set_text_fmt(gui_gps_alt, "Alt: %.0fm  Spd: %.1fkm/h", gps_alt, gps_speed);
            lv_obj_set_style_text_color(gui_gps_alt, lv_color_hex(GUI_COL_MID), 0);
        } else if (any_fix) {
            lv_label_set_text_fmt(gui_gps_alt, "Alt: %.0fm  Spd: ---", gps_alt);
            lv_obj_set_style_text_color(gui_gps_alt, lv_color_hex(GUI_COL_DIM), 0);
        } else {
            lv_label_set_text(gui_gps_alt, "Alt: ---  Spd: ---");
            lv_obj_set_style_text_color(gui_gps_alt, lv_color_hex(GUI_COL_DIM), 0);
        }

        if (beacon_mode_active) {
            lv_label_set_text(gui_gps_beacon, "Beacon: active");
            lv_obj_set_style_text_color(gui_gps_beacon, lv_color_hex(GUI_COL_AMBER), 0);
        } else {
            lv_label_set_text(gui_gps_beacon, "Beacon: off");
            lv_obj_set_style_text_color(gui_gps_beacon, lv_color_hex(GUI_COL_DIM), 0);
        }
    }
    #endif

    // ---- Settings screen (logger status) ----
    if (on_settings && gui_set_log_status) {
        #if HAS_SD
        if (imu_logging) {
            uint32_t dur = (millis() - imu_log_start_ms) / 1000;
            lv_label_set_text_fmt(gui_set_log_status, "%lu samples  %lus", imu_log_samples, dur);
            lv_obj_set_style_text_color(gui_set_log_status, lv_color_hex(GUI_COL_GREEN), 0);
            if (!lv_obj_has_state(gui_set_log_sw, LV_STATE_CHECKED))
                lv_obj_add_state(gui_set_log_sw, LV_STATE_CHECKED);
        } else {
            lv_label_set_text(gui_set_log_status, "SD card ready");
            lv_obj_set_style_text_color(gui_set_log_status, lv_color_hex(GUI_COL_DIM), 0);
            if (lv_obj_has_state(gui_set_log_sw, LV_STATE_CHECKED))
                lv_obj_clear_state(gui_set_log_sw, LV_STATE_CHECKED);
        }
        #else
        lv_label_set_text(gui_set_log_status, "No SD card");
        #endif
    }
}

// ---------------------------------------------------------------------------
// Initialize LVGL and create all screens
// ---------------------------------------------------------------------------
bool gui_init() {
    lv_init();

    // --- Display driver ---
    gui_display = lv_display_create(GUI_W, GUI_H);
    if (!gui_display) return false;
    lv_display_set_color_format(gui_display, LV_COLOR_FORMAT_RGB565_SWAPPED);
    lv_display_set_flush_cb(gui_display, gui_flush_cb);

    uint32_t buf_size = GUI_W * GUI_BUF_LINES * sizeof(uint16_t);
    gui_buf1 = (uint8_t *)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    gui_buf2 = (uint8_t *)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!gui_buf1 || !gui_buf2) {
        if (gui_buf1) free(gui_buf1);
        if (gui_buf2) free(gui_buf2);
        gui_buf2 = NULL;
        gui_buf1 = (uint8_t *)malloc(buf_size);
        if (!gui_buf1) return false;
    }
    lv_display_set_buffers(gui_display, gui_buf1, gui_buf2, buf_size,
                            LV_DISPLAY_RENDER_MODE_FULL);

    // Shadow framebuffer for screenshots (410*502*2 = 411,640 bytes)
    gui_screenshot_buf = (uint16_t *)heap_caps_malloc(GUI_W * GUI_H * sizeof(uint16_t),
                                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (gui_screenshot_buf) {
        memset(gui_screenshot_buf, 0, GUI_W * GUI_H * sizeof(uint16_t));
        Serial.printf("[gui] screenshot buf @ %p (%u bytes)\n",
                      gui_screenshot_buf, GUI_W * GUI_H * 2);
    }

    // --- Input driver ---
    gui_indev = lv_indev_create();
    if (gui_indev) {
        lv_indev_set_type(gui_indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(gui_indev, gui_touch_read_cb);
        lv_indev_set_scroll_throw(gui_indev, 20);  // Moderate friction: decelerates into snap within ~1s at 10fps
    }

    // --- Screen setup ---
    gui_screen = lv_screen_active();
    gui_style_black_container(gui_screen);

    // --- Tileview: 3x3 grid, 5 populated tiles ---
    //        (1,0) Radio
    // (0,1) GPS  (1,1) Watch  (2,1) Messages
    //        (1,2) Settings
    gui_tileview = lv_tileview_create(gui_screen);
    lv_obj_set_style_bg_color(gui_tileview, lv_color_hex(GUI_COL_BLACK), 0);
    lv_obj_set_style_bg_opa(gui_tileview, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(gui_tileview, 0, 0);
    lv_obj_set_style_pad_all(gui_tileview, 0, 0);
    lv_obj_set_scrollbar_mode(gui_tileview, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_anim_duration(gui_tileview, 150, 0);  // Snappy 150ms scroll snap
    lv_obj_clear_flag(gui_tileview, LV_OBJ_FLAG_SCROLL_ELASTIC);  // No bounce at tile edges
    lv_obj_set_size(gui_tileview, GUI_W, GUI_H);

    gui_tile_watch = lv_tileview_add_tile(gui_tileview, 1, 1, LV_DIR_ALL);
    gui_tile_radio = lv_tileview_add_tile(gui_tileview, 1, 0, LV_DIR_BOTTOM);
    gui_tile_gps   = lv_tileview_add_tile(gui_tileview, 0, 1, LV_DIR_RIGHT);
    gui_tile_msg   = lv_tileview_add_tile(gui_tileview, 2, 1, LV_DIR_LEFT);
    gui_tile_set   = lv_tileview_add_tile(gui_tileview, 1, 2, LV_DIR_TOP);

    // Start on watch face
    lv_tileview_set_tile(gui_tileview, gui_tile_watch, LV_ANIM_OFF);

    // Haptic feedback on tile change
    lv_obj_add_event_cb(gui_tileview, gui_tile_change_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // --- Create screen content ---
    gui_create_watchface(gui_tile_watch);
    gui_create_radio_screen(gui_tile_radio);
    gui_create_gps_screen(gui_tile_gps);
    gui_create_msg_screen(gui_tile_msg);
    gui_create_settings_screen(gui_tile_set);

    return true;
}

// ---------------------------------------------------------------------------
// Screenshot: dump framebuffer as raw RGB565 to a file on SPIFFS,
// or output dimensions to serial for external tools.
// Call gui_screenshot() to write /screenshot.raw to SPIFFS (if mounted),
// or read gui_screenshot_buf directly via debugger.
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Remote debug protocol over serial
// ---------------------------------------------------------------------------
// Trigger: 3-byte prefix [0x52, 0x57, 0x53] ("RWS") + command byte + optional payload
//
// Commands:
//   'S' (0x53) — Screenshot: captures next frame, responds RWSS + u16 w + u16 h + pixels
//   'T' (0x54) — Touch inject: reads 5 bytes (u16 x, u16 y, u8 duration_100ms)
//   'N' (0x4E) — Navigate: reads 2 bytes (u8 col, u8 row) — jump to tile
//   'M' (0x4D) — Metrics: responds RWSM + JSON stats
//   'I' (0x49) — Invalidate: force full screen redraw
//   'L' (0x4C) — Log toggle: start/stop IMU logging to SD card
//   'F' (0x46) — File list: lists files on SD card
//   'P' (0x50) — Profile: runs standardized performance test, reports JSON results
//   'B' (0x42) — Beacon dump: dumps last beacon packet pre/post IFAC
//   'C' (0x43) — Crypto test: runs IFAC test vectors, reports pass/fail

#define GUI_CMD_PREFIX_LEN 3
static const uint8_t gui_cmd_prefix[] = {0x52, 0x57, 0x53};  // "RWS"
static uint8_t gui_cmd_state = 0;
static uint8_t gui_cmd_id = 0;
static uint8_t gui_cmd_payload[8];
static uint8_t gui_cmd_payload_pos = 0;
static uint8_t gui_cmd_payload_len = 0;

static void gui_cmd_execute();

void gui_process_serial_byte(uint8_t b) {
    // Match prefix
    if (gui_cmd_state < GUI_CMD_PREFIX_LEN) {
        if (b == gui_cmd_prefix[gui_cmd_state]) {
            gui_cmd_state++;
        } else {
            gui_cmd_state = (b == gui_cmd_prefix[0]) ? 1 : 0;
        }
        return;
    }

    // Prefix matched — next byte is command
    if (gui_cmd_state == GUI_CMD_PREFIX_LEN) {
        gui_cmd_id = b;
        gui_cmd_payload_pos = 0;
        switch (b) {
            case 'T': gui_cmd_payload_len = 5; break;  // x(2) + y(2) + duration(1)
            case 'N': gui_cmd_payload_len = 2; break;  // col(1) + row(1)
            case 'D': gui_cmd_payload_len = 1; break;  // file index(1)
            default:  gui_cmd_payload_len = 0; break;  // S, M, I, F, L, X, Z — no payload
        }
        gui_cmd_state++;
        if (gui_cmd_payload_len == 0) {
            gui_cmd_execute();
            gui_cmd_state = 0;
        }
        return;
    }

    // Collecting payload
    if (gui_cmd_payload_pos < gui_cmd_payload_len) {
        gui_cmd_payload[gui_cmd_payload_pos++] = b;
        if (gui_cmd_payload_pos >= gui_cmd_payload_len) {
            gui_cmd_execute();
            gui_cmd_state = 0;
        }
    }
}

static void gui_cmd_execute() {
    const uint8_t hdr[] = {'R', 'W', 'S', gui_cmd_id};

    switch (gui_cmd_id) {
        case 'S': {  // Screenshot
            if (gui_screenshot_buf) {
                // Unblank and force full redraw so screenshot captures entire screen
                if (display_blanked) display_unblank();
                lv_obj_invalidate(lv_screen_active());
                gui_screenshot_pending = true;
                // Force full-screen render into screenshot buffer
                // Advance tick past the refresh period to ensure render fires
                lv_tick_inc(LV_DEF_REFR_PERIOD + 1);
                gui_update_data();
                lv_timer_handler();
                gui_screenshot_pending = false;
                Serial.write(hdr, 4);
                uint16_t w = GUI_W, h = GUI_H;
                Serial.write((uint8_t *)&w, 2);
                Serial.write((uint8_t *)&h, 2);
                Serial.write((uint8_t *)gui_screenshot_buf, GUI_W * GUI_H * 2);
                Serial.flush();
            } else {
                Serial.write(hdr, 4);
                uint16_t z = 0;
                Serial.write((uint8_t *)&z, 2);
                Serial.write((uint8_t *)&z, 2);
                Serial.flush();
            }
            break;
        }

        case 'T': {  // Touch inject
            gui_inject_x = gui_cmd_payload[0] | (gui_cmd_payload[1] << 8);
            gui_inject_y = gui_cmd_payload[2] | (gui_cmd_payload[3] << 8);
            uint32_t dur = gui_cmd_payload[4] * 100;  // duration in 100ms units
            if (dur == 0) dur = 200;
            gui_inject_pressed = true;
            gui_inject_until = millis() + dur;
            // Unblank display on injected touch
            if (display_blanked) display_unblank();
            break;
        }

        case 'N': {  // Navigate to tile
            uint8_t col = gui_cmd_payload[0];
            uint8_t row = gui_cmd_payload[1];
            if (gui_tileview) {
                // Find tile at position
                lv_obj_t *target = NULL;
                if (col == 1 && row == 1) target = gui_tile_watch;
                else if (col == 1 && row == 0) target = gui_tile_radio;
                else if (col == 0 && row == 1) target = gui_tile_gps;
                else if (col == 2 && row == 1) target = gui_tile_msg;
                else if (col == 1 && row == 2) target = gui_tile_set;
                if (target) {
                    lv_tileview_set_tile(gui_tileview, target, LV_ANIM_ON);
                }
            }
            if (display_blanked) display_unblank();
            break;
        }

        case 'M': {  // Metrics
            Serial.write(hdr, 4);
            char buf[192];
            uint32_t avg_flush = gui_frame_count > 0 ? gui_flush_us_total / gui_frame_count : 0;
            snprintf(buf, sizeof(buf),
                "{\"build\":\"%s %s\",\"loop\":%lu,\"radio\":%lu,"
                "\"serial\":%lu,\"disp\":%lu,\"pmu\":%lu,"
                "\"gps\":%lu,\"bt\":%lu,\"imu\":%lu,"
                "\"bcn_gate\":%d,\"hw_ready\":%d,"
                "\"lxmf_id\":%d,\"bcn_crypto\":%d}\n",
                __DATE__, __TIME__,
                gui_loop_us_last, prof_radio_us, prof_serial_us,
                prof_display_us, prof_pmu_us, prof_gps_us,
                prof_bt_us, prof_imu_us,
                beacon_gate, hw_ready ? 1 : 0,
                lxmf_identity_configured ? 1 : 0,
                beacon_crypto_configured ? 1 : 0);
            gui_loop_us_max = 0;
            Serial.write((uint8_t *)buf, strlen(buf));
            Serial.flush();
            break;
        }

        case 'P': {  // Standardized performance profile test
            Serial.write(hdr, 4);
            if (display_blanked) display_unblank();

            uint32_t p_t0, p_t1;
            uint32_t p_idle_render = 0, p_idle_flush = 0;
            uint32_t p_full_render = 0, p_full_flush = 0;
            uint32_t p_nav_total = 0;
            uint32_t p_data_update = 0;
            uint32_t p_frames = 0;
            uint32_t p_multi_total = 0;

            // Test 1: Idle frame (nothing dirty)
            lv_tick_inc(LV_DEF_REFR_PERIOD + 1);
            lv_timer_handler();  // clear any pending
            gui_flush_us_last = 0;
            gui_frame_count = 0;
            p_t0 = micros();
            lv_tick_inc(LV_DEF_REFR_PERIOD + 1);
            lv_timer_handler();
            p_t1 = micros();
            p_idle_render = p_t1 - p_t0;
            p_idle_flush = gui_flush_us_last;

            // Test 2: Full invalidation + render
            lv_obj_invalidate(lv_screen_active());
            gui_flush_us_last = 0;
            p_t0 = micros();
            lv_tick_inc(LV_DEF_REFR_PERIOD + 1);
            lv_timer_handler();
            p_t1 = micros();
            p_full_render = p_t1 - p_t0;
            p_full_flush = gui_flush_us_last;

            // Test 3: Data update cycle
            gui_last_data_update = 0;  // force update
            p_t0 = micros();
            gui_update_data();
            p_t1 = micros();
            p_data_update = p_t1 - p_t0;

            // Test 4: Navigate to each tile and back (5 transitions)
            p_t0 = micros();
            lv_tileview_set_tile(gui_tileview, gui_tile_radio, LV_ANIM_OFF);
            lv_tick_inc(LV_DEF_REFR_PERIOD + 1);
            lv_timer_handler();
            lv_tileview_set_tile(gui_tileview, gui_tile_gps, LV_ANIM_OFF);
            lv_tick_inc(LV_DEF_REFR_PERIOD + 1);
            lv_timer_handler();
            lv_tileview_set_tile(gui_tileview, gui_tile_msg, LV_ANIM_OFF);
            lv_tick_inc(LV_DEF_REFR_PERIOD + 1);
            lv_timer_handler();
            lv_tileview_set_tile(gui_tileview, gui_tile_set, LV_ANIM_OFF);
            lv_tick_inc(LV_DEF_REFR_PERIOD + 1);
            lv_timer_handler();
            lv_tileview_set_tile(gui_tileview, gui_tile_watch, LV_ANIM_OFF);
            lv_tick_inc(LV_DEF_REFR_PERIOD + 1);
            lv_timer_handler();
            p_t1 = micros();
            p_nav_total = p_t1 - p_t0;

            // Test 5: Rapid frame burst (10 full frames)
            p_t0 = micros();
            for (int i = 0; i < 10; i++) {
                lv_obj_invalidate(lv_screen_active());
                lv_tick_inc(LV_DEF_REFR_PERIOD + 1);
                lv_timer_handler();
            }
            p_t1 = micros();
            p_multi_total = p_t1 - p_t0;

            Serial.printf("{\"test\":\"profile\",\"build\":\"%s %s\","
                "\"idle_us\":%lu,\"idle_flush_us\":%lu,"
                "\"full_us\":%lu,\"full_flush_us\":%lu,"
                "\"data_update_us\":%lu,"
                "\"nav_5tile_us\":%lu,"
                "\"burst_10frame_us\":%lu,"
                "\"avg_frame_us\":%lu,"
                "\"loop_us\":%lu,"
                "\"heap\":%lu,\"psram\":%lu}\n",
                __DATE__, __TIME__,
                p_idle_render, p_idle_flush,
                p_full_render, p_full_flush,
                p_data_update,
                p_nav_total,
                p_multi_total, p_multi_total / 10,
                gui_loop_us_last,
                (uint32_t)esp_get_free_heap_size(),
                (uint32_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
            Serial.flush();
            break;
        }

        case 'B': {  // Beacon packet dump
            extern uint8_t diag_beacon_pre[];
            extern uint16_t diag_beacon_pre_len;
            extern uint8_t diag_beacon_post[];
            extern uint16_t diag_beacon_post_len;

            Serial.write(hdr, 4);
            Serial.printf("{\"pre_len\":%d,\"post_len\":%d", diag_beacon_pre_len, diag_beacon_post_len);
            if (diag_beacon_pre_len > 0) {
                Serial.printf(",\"pre\":\"");
                for (int i = 0; i < diag_beacon_pre_len; i++) Serial.printf("%02x", diag_beacon_pre[i]);
                Serial.printf("\"");
            }
            if (diag_beacon_post_len > 0) {
                Serial.printf(",\"post\":\"");
                for (int i = 0; i < diag_beacon_post_len; i++) Serial.printf("%02x", diag_beacon_post[i]);
                Serial.printf("\"");
            }
            Serial.println("}");
            Serial.flush();
            break;
        }

        case 'C': {  // Crypto test — IFAC test vectors
            #if HAS_GPS == true
            Serial.write(hdr, 4);

            // Test vectors from scripts/test_ifac.py
            const uint8_t tv_key[64] = {0x3a, 0xc2, 0xe0, 0x12, 0xa0, 0x86, 0x04, 0x3c, 0x67, 0xcc, 0xef, 0x40, 0x6a, 0x0b, 0xdb, 0x38, 0xc0, 0x66, 0xb2, 0xee, 0x0a, 0x7f, 0x18, 0x27, 0xfa, 0x1c, 0xb9, 0xdc, 0xcf, 0xbb, 0x8e, 0x9d, 0x53, 0x48, 0xc5, 0x56, 0xf0, 0x8e, 0xed, 0xf3, 0x0b, 0xce, 0x46, 0x2b, 0xb2, 0x09, 0x6b, 0x99, 0x26, 0x08, 0xf4, 0xfc, 0xfd, 0x12, 0x32, 0x4b, 0xb2, 0x45, 0x86, 0x2b, 0x59, 0xd6, 0x11, 0xc7};
            const uint8_t tv_pk[32] = {0x1a, 0x54, 0x5d, 0x78, 0x34, 0xc3, 0xe1, 0x6c, 0x53, 0x9d, 0xd5, 0xf5, 0x3a, 0xd1, 0x5b, 0x67, 0xae, 0x57, 0x5e, 0x97, 0x06, 0x05, 0x38, 0x5b, 0xeb, 0x76, 0xe9, 0x85, 0x2e, 0xf9, 0xe1, 0xdf};
            const uint8_t tv_msg[19] = {0x00, 0x00, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
            const uint8_t tv_sig[64] = {0x99, 0x80, 0x27, 0x05, 0xaf, 0xde, 0xb0, 0xe6, 0xfe, 0xe5, 0x2b, 0xbc, 0x35, 0x4a, 0x87, 0x93, 0xd8, 0xc2, 0x9c, 0x77, 0x41, 0x6c, 0x5c, 0x54, 0x62, 0x7e, 0x66, 0xc6, 0x50, 0x05, 0xe5, 0x0a, 0x02, 0x48, 0x94, 0x4b, 0xb1, 0x02, 0x5b, 0x3a, 0xaa, 0xa2, 0x9b, 0x26, 0xc4, 0x7f, 0x49, 0x4b, 0xa2, 0x1a, 0xf0, 0xb5, 0xd0, 0x08, 0x8f, 0x9b, 0x49, 0x5b, 0xf2, 0xc7, 0xe1, 0x83, 0x99, 0x01};
            const uint8_t tv_mask[27] = {0x3b, 0x8f, 0x16, 0xab, 0xe6, 0x0b, 0x8e, 0x35, 0xcb, 0x47, 0x5a, 0x3d, 0x13, 0x00, 0x05, 0xe6, 0x79, 0x79, 0x99, 0x23, 0x35, 0x24, 0x64, 0xd8, 0x4b, 0xf5, 0x3c};
            const uint8_t tv_result[27] = {0xbb, 0x8f, 0x49, 0x5b, 0xf2, 0xc7, 0xe1, 0x83, 0x99, 0x01, 0x48, 0x09, 0x45, 0x78, 0x9f, 0x5a, 0xa7, 0x89, 0x88, 0x01, 0x06, 0x60, 0x31, 0xbe, 0x3c, 0x7d, 0xa5};

            // Test 1: Keypair derivation
            uint8_t pk[32], sk[64];
            crypto_sign_ed25519_seed_keypair(pk, sk, tv_key + 32);
            bool pk_match = (memcmp(pk, tv_pk, 32) == 0);

            // Test 2: Signature
            uint8_t sig[64];
            unsigned long long sig_len;
            crypto_sign_ed25519_detached(sig, &sig_len, tv_msg, 19, sk);
            bool sig_match = (memcmp(sig, tv_sig, 64) == 0);

            // Test 3: HKDF
            uint8_t mask[27];
            rns_hkdf_var(sig + 56, 8, tv_key, 64, mask, 27);
            bool mask_match = (memcmp(mask, tv_mask, 27) == 0);

            // Test 4: Full IFAC apply
            uint8_t pkt[64];
            memcpy(pkt, tv_msg, 19);
            // Save original ifac state and substitute test key
            uint8_t saved_key[64]; bool saved_configured;
            memcpy(saved_key, ifac_key, 64);
            saved_configured = ifac_configured;
            memcpy(ifac_key, tv_key, 64);
            ifac_derive_keypair();
            ifac_configured = true;
            uint16_t result_len = ifac_apply(pkt, 19);
            bool result_match = (result_len == 27) && (memcmp(pkt, tv_result, 27) == 0);
            // Restore
            memcpy(ifac_key, saved_key, 64);
            if (saved_configured) ifac_derive_keypair();
            ifac_configured = saved_configured;

            // Report
            // Dump the live IFAC key for verification
            Serial.printf("{\"pk\":%s,\"sig\":%s,\"hkdf\":%s,\"ifac\":%s,\"configured\":%s",
                pk_match ? "true" : "false",
                sig_match ? "true" : "false",
                mask_match ? "true" : "false",
                result_match ? "true" : "false",
                ifac_configured ? "true" : "false");
            Serial.printf(",\"live_key\":\"");
            for (int i = 0; i < 64; i++) Serial.printf("%02x", ifac_key[i]);
            Serial.printf("\"");

            // Dump actual values on failure for debugging
            if (!sig_match) {
                Serial.printf(",\"actual_sig\":\"");
                for (int i = 0; i < 64; i++) Serial.printf("%02x", sig[i]);
                Serial.printf("\"");
            }
            if (!mask_match) {
                Serial.printf(",\"actual_mask\":\"");
                for (int i = 0; i < 27; i++) Serial.printf("%02x", mask[i]);
                Serial.printf("\"");
            }
            if (!result_match) {
                Serial.printf(",\"actual_result\":\"");
                for (int i = 0; i < (int)result_len; i++) Serial.printf("%02x", pkt[i]);
                Serial.printf("\",\"result_len\":%d", result_len);
            }
            Serial.println("}");
            Serial.flush();
            #else
            Serial.write(hdr, 4);
            Serial.println("{\"error\":\"no_gps\"}");
            Serial.flush();
            #endif
            break;
        }

        case 'I': {  // Invalidate — force full redraw
            if (gui_screen) lv_obj_invalidate(gui_screen);
            if (display_blanked) display_unblank();
            break;
        }

        case 'L': {  // Toggle IMU logging
            Serial.write(hdr, 4);
            if (gui_log_toggle_fn) {
                gui_imu_logging = gui_log_toggle_fn();
                Serial.printf("{\"logging\":%s}\n", gui_imu_logging ? "true" : "false");
            } else {
                Serial.println("{\"logging\":false,\"error\":\"not_available\"}");
            }
            Serial.flush();
            break;
        }

        case 'F': {  // List files on SD card
            Serial.write(hdr, 4);
            if (gui_list_files_fn) {
                gui_list_files_fn();
            } else {
                Serial.println("{\"error\":\"no_sd\"}");
            }
            Serial.flush();
            break;
        }
        case 'D': {  // Download file by index
            Serial.write(hdr, 4);
            if (gui_download_file_fn) {
                gui_download_file_fn(gui_cmd_payload[0]);
            } else {
                Serial.println("{\"error\":\"no_sd\"}");
            }
            Serial.flush();
            break;
        }
        case 'X': {  // Hard reset
            Serial.write(hdr, 4);
            Serial.println("{\"reset\":true}");
            Serial.flush();
            delay(100);
            ESP.restart();
            break;
        }
        case 'Z': {  // Reboot into download mode (no BOOT+RST needed)
            Serial.write(hdr, 4);
            Serial.println("{\"bootloader\":true}");
            Serial.flush();
            delay(100);
            #if MCU_VARIANT == MCU_ESP32
              REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
              ESP.restart();
            #endif
            break;
        }
    }
}

void gui_screenshot_info() {
    if (gui_screenshot_buf) {
        Serial.printf("[screenshot] addr=%p size=%u w=%d h=%d\n",
                      gui_screenshot_buf, GUI_W * GUI_H * 2, GUI_W, GUI_H);
    } else {
        Serial.println("[screenshot] buffer not allocated");
    }
}

// Write screenshot to SD card as raw RGB565 + BMP header
#if HAS_SD
#include <SD.h>
#include "SharedSPI.h"
bool gui_screenshot_sd(const char *path = "/screenshot.bmp") {
    if (!gui_screenshot_buf) return false;

    // Acquire shared SPI mutex for SD access
    if (shared_spi_mutex) xSemaphoreTake(shared_spi_mutex, portMAX_DELAY);
    SPI.begin(SD_CLK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS, SPI, 4000000, "/sd", 5)) {
        if (shared_spi_mutex) xSemaphoreGive(shared_spi_mutex);
        Serial.println("[screenshot] SD init failed");
        return false;
    }

    File f = SD.open(path, FILE_WRITE);
    if (!f) {
        if (shared_spi_mutex) xSemaphoreGive(shared_spi_mutex);
        Serial.println("[screenshot] file open failed");
        return false;
    }

    // Write BMP header (RGB565 LE, top-down)
    uint32_t img_size = GUI_W * GUI_H * 2;
    uint32_t file_size = 14 + 40 + 12 + img_size;  // file + info + masks + pixels
    uint32_t data_offset = 14 + 40 + 12;

    // BMP file header (14 bytes)
    uint8_t bmp_hdr[14] = {'B', 'M'};
    memcpy(&bmp_hdr[2], &file_size, 4);
    memset(&bmp_hdr[6], 0, 4);  // reserved
    memcpy(&bmp_hdr[10], &data_offset, 4);
    f.write(bmp_hdr, 14);

    // DIB header (BITMAPINFOHEADER, 40 bytes)
    uint8_t dib[40] = {};
    uint32_t dib_size = 40;
    int32_t  bmp_w = GUI_W;
    int32_t  bmp_h = -GUI_H;  // negative = top-down
    uint16_t planes = 1;
    uint16_t bpp = 16;
    uint32_t compression = 3;  // BI_BITFIELDS
    memcpy(&dib[0], &dib_size, 4);
    memcpy(&dib[4], &bmp_w, 4);
    memcpy(&dib[8], &bmp_h, 4);
    memcpy(&dib[12], &planes, 2);
    memcpy(&dib[14], &bpp, 2);
    memcpy(&dib[16], &compression, 4);
    memcpy(&dib[20], &img_size, 4);
    f.write(dib, 40);

    // RGB565 bitmasks (R, G, B)
    uint32_t mask_r = 0xF800, mask_g = 0x07E0, mask_b = 0x001F;
    f.write((uint8_t *)&mask_r, 4);
    f.write((uint8_t *)&mask_g, 4);
    f.write((uint8_t *)&mask_b, 4);

    // Pixel data (RGB565 LE, row by row)
    // BMP rows must be 4-byte aligned; 410*2=820 bytes per row, 820%4=0, no padding needed
    f.write((uint8_t *)gui_screenshot_buf, img_size);

    f.close();
    if (shared_spi_mutex) xSemaphoreGive(shared_spi_mutex);

    Serial.printf("[screenshot] saved %s (%u bytes)\n", path, file_size);
    return true;
}
#endif

// ---------------------------------------------------------------------------
// Main GUI update — called from update_display()
// ---------------------------------------------------------------------------
void gui_update() {
    static uint32_t last_tick = 0;
    uint32_t now = millis();
    lv_tick_inc(now - last_tick);
    last_tick = now;

    // Measure loop interval
    uint32_t now_us = micros();
    if (gui_last_update_us > 0) {
        gui_loop_us_last = now_us - gui_last_update_us;
        if (gui_loop_us_last > gui_loop_us_max) gui_loop_us_max = gui_loop_us_last;
    }
    gui_last_update_us = now_us;

    gui_update_data();

    gui_render_start = micros();
    lv_timer_handler();
    gui_render_us_last = micros() - gui_render_start;
    // After lv_timer_handler, a new DMA may be queued via gui_flush_cb.
    // It runs in background on SPI3 until the next gui_update() call.
}

#endif // BOARD_MODEL == BOARD_TWATCH_ULT
#endif // GUI_H
