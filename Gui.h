// R-Watch GUI — LVGL integration for T-Watch Ultimate
// Tileview navigation with watch face, radio status, GPS, messages, settings
// Requires: CO5300.h (display), XL9555.h (power control), DRV2605.h (haptic)

#ifndef GUI_H
#define GUI_H

#if BOARD_MODEL == BOARD_TWATCH_ULT

#include <lvgl.h>

// Custom fonts: generated with lv_font_conv --no-compress from Montserrat Bold
// IMPORTANT: must use --no-compress or set LV_USE_FONT_COMPRESSED=1 in lv_conf.h
#include "Fonts/montserrat_bold_96.c"

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

// Full-frame buffer: renders entire screen at once (eliminates tearing during scroll)
// 410*502*2 = 411,640 bytes per buffer — fits in PSRAM
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
static lv_obj_t *gui_steps_value = NULL;
static lv_obj_t *gui_steps_label = NULL;

// Radio status widgets
static lv_obj_t *gui_radio_freq  = NULL;
static lv_obj_t *gui_radio_params = NULL;
static lv_obj_t *gui_radio_rssi_bar = NULL;
static lv_obj_t *gui_radio_rssi_lbl = NULL;
static lv_obj_t *gui_radio_util  = NULL;
static lv_obj_t *gui_radio_ble   = NULL;
static lv_obj_t *gui_radio_pkts  = NULL;

// GPS screen widgets
static lv_obj_t *gui_gps_coords  = NULL;
static lv_obj_t *gui_gps_fix     = NULL;
static lv_obj_t *gui_gps_alt     = NULL;
static lv_obj_t *gui_gps_beacon  = NULL;

// Touch input via function pointer (set by .ino after touch init)
typedef bool (*gui_touch_fn_t)(int16_t *x, int16_t *y);
static gui_touch_fn_t gui_touch_fn = NULL;

// Data update throttle
static uint32_t gui_last_data_update = 0;
#define GUI_DATA_UPDATE_MS 500

// Track current tile for haptic feedback
static uint8_t gui_last_tile_col = 1;
static uint8_t gui_last_tile_row = 1;

// ---------------------------------------------------------------------------
// LVGL display flush callback
// ---------------------------------------------------------------------------
// Shadow framebuffer for screenshots (RGB565 swapped / big-endian — same as display)
uint16_t *gui_screenshot_buf = NULL;

static void gui_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint16_t x1 = area->x1;
    uint16_t y1 = area->y1;
    uint16_t w  = area->x2 - area->x1 + 1;
    uint16_t h  = area->y2 - area->y1 + 1;
    uint16_t *pixels = (uint16_t *)px_map;

    // Copy to shadow framebuffer for screenshots
    if (gui_screenshot_buf) {
        for (uint16_t row = 0; row < h; row++) {
            memcpy(&gui_screenshot_buf[(y1 + row) * GUI_W + x1],
                   &pixels[row * w], w * sizeof(uint16_t));
        }
    }

    co5300_push_pixels(x1, y1, w, h, pixels);
    lv_display_flush_ready(disp);
}

// ---------------------------------------------------------------------------
// LVGL touch input read callback
// ---------------------------------------------------------------------------
static void gui_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    int16_t tx, ty;
    if (gui_touch_fn && gui_touch_fn(&tx, &ty)) {
        data->point.x = tx;
        data->point.y = ty;
        data->state = LV_INDEV_STATE_PRESSED;
        last_unblank_event = millis();
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

    lv_obj_t *val = gui_label(cell, &lv_font_montserrat_20, color, "--");
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
    gui_time_label = gui_label(parent, &montserrat_bold_96, GUI_COL_WHITE, "00:00");
    lv_obj_set_style_text_letter_space(gui_time_label, 2, 0);
    lv_obj_set_width(gui_time_label, GUI_W);
    lv_obj_set_style_text_align(gui_time_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(gui_time_label, 0, GUI_TIME_Y);

    // Date
    gui_date_label = gui_label(parent, &lv_font_montserrat_20, GUI_COL_MID, "--- -- ---");
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
    gui_create_complication(comp, GUI_PAD + cw * 2, cw, GUI_COL_WHITE, "Steps", &gui_steps_value, &gui_steps_label);

    // Rule 2
    gui_create_rule(parent, GUI_RULE2_Y);
}

// ---------------------------------------------------------------------------
// Screen: Radio Status (top tile — swipe down from watch face)
// ---------------------------------------------------------------------------
static void gui_create_radio_screen(lv_obj_t *parent) {
    gui_style_black_container(parent);

    gui_label_at(parent, &lv_font_montserrat_14, GUI_COL_DIM, "RADIO STATUS", GUI_PAD, 12);

    // Frequency
    gui_radio_freq = gui_label_at(parent, &lv_font_montserrat_20, GUI_COL_AMBER, "--- MHz", GUI_PAD, 40);

    // LoRa parameters
    gui_radio_params = gui_label_at(parent, &lv_font_montserrat_20, GUI_COL_MID, "SF- BW- CR-", GUI_PAD, 80);

    // RSSI
    gui_create_rule(parent, 115);
    gui_label_at(parent, &lv_font_montserrat_14, GUI_COL_DIM, "RSSI", GUI_PAD, 125);
    gui_radio_rssi_lbl = gui_label(parent, &lv_font_montserrat_20, GUI_COL_AMBER, "---");
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
    gui_radio_util = gui_label(parent, &lv_font_montserrat_20, GUI_COL_MID, "-- %");
    lv_obj_align(gui_radio_util, LV_ALIGN_TOP_RIGHT, -GUI_PAD, 195);

    // BLE status
    gui_create_rule(parent, 230);
    gui_label_at(parent, &lv_font_montserrat_14, GUI_COL_DIM, "BLE", GUI_PAD, 243);
    gui_radio_ble = gui_label(parent, &lv_font_montserrat_20, GUI_COL_BLUE, "---");
    lv_obj_align(gui_radio_ble, LV_ALIGN_TOP_RIGHT, -GUI_PAD, 240);

    // Packet counts
    gui_create_rule(parent, 275);
    gui_radio_pkts = gui_label_at(parent, &lv_font_montserrat_20, GUI_COL_MID,
                                   "RX: 0  TX: 0", GUI_PAD, 290);
}

// ---------------------------------------------------------------------------
// Screen: GPS (right tile — swipe right from watch face)
// ---------------------------------------------------------------------------
static void gui_create_gps_screen(lv_obj_t *parent) {
    gui_style_black_container(parent);

    gui_label_at(parent, &lv_font_montserrat_14, GUI_COL_DIM, "GPS", GUI_PAD, 12);

    // Coordinates
    gui_gps_coords = gui_label_at(parent, &lv_font_montserrat_20, GUI_COL_TEAL,
                                   "-- --", GUI_PAD, 40);

    // Fix quality
    gui_create_rule(parent, 110);
    gui_gps_fix = gui_label_at(parent, &lv_font_montserrat_20, GUI_COL_MID,
                                "Sats: --  HDOP: --", GUI_PAD, 125);

    // Altitude and speed
    gui_create_rule(parent, 160);
    gui_gps_alt = gui_label_at(parent, &lv_font_montserrat_20, GUI_COL_MID,
                                "Alt: --  Spd: --", GUI_PAD, 175);

    // Beacon status
    gui_create_rule(parent, 215);
    gui_gps_beacon = gui_label_at(parent, &lv_font_montserrat_20, GUI_COL_AMBER,
                                   "Beacon: --", GUI_PAD, 230);
}

// ---------------------------------------------------------------------------
// Screen: Messages (left tile — swipe left from watch face)
// ---------------------------------------------------------------------------
static void gui_create_msg_screen(lv_obj_t *parent) {
    gui_style_black_container(parent);
    gui_label_at(parent, &lv_font_montserrat_14, GUI_COL_DIM, "MESSAGES", GUI_PAD, 12);
    lv_obj_t *lbl = gui_label(parent, &lv_font_montserrat_20, GUI_COL_DIM, "No messages");
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
}

// ---------------------------------------------------------------------------
// Screen: Settings (bottom tile — swipe up from watch face)
// ---------------------------------------------------------------------------
static void gui_create_settings_screen(lv_obj_t *parent) {
    gui_style_black_container(parent);
    gui_label_at(parent, &lv_font_montserrat_14, GUI_COL_DIM, "SETTINGS", GUI_PAD, 12);
    lv_obj_t *lbl = gui_label(parent, &lv_font_montserrat_20, GUI_COL_MID, "Coming soon");
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
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

static void gui_update_data() {
    if (!gui_time_label) return;

    uint32_t now = millis();
    if (now - gui_last_data_update < GUI_DATA_UPDATE_MS) return;
    gui_last_data_update = now;

    // ---- Watch face ----
    lv_label_set_text_fmt(gui_time_label, "%02d:%02d", rtc_hour, rtc_minute);

    #if HAS_RTC == true
    if (rtc_year > 0) {
        const char *mon = (rtc_month >= 1 && rtc_month <= 12) ? gui_month_names[rtc_month - 1] : "---";
        lv_label_set_text_fmt(gui_date_label, "%d %s %d", rtc_day, mon, rtc_year);
    }
    #endif

    // Mode
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

    // GPS complication
    #if HAS_GPS == true
    if (gps_sats > 0) {
        lv_label_set_text_fmt(gui_gps_value, "%d sats", gps_sats);
        lv_obj_set_style_text_color(gui_gps_value, lv_color_hex(GUI_COL_TEAL), 0);
    } else {
        lv_label_set_text(gui_gps_value, "no fix");
        lv_obj_set_style_text_color(gui_gps_value, lv_color_hex(GUI_COL_DIM), 0);
    }
    #endif

    // Steps placeholder
    lv_label_set_text(gui_steps_value, "--");

    // ---- Radio status screen ----
    if (gui_radio_freq) {
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
    }

    // ---- GPS screen ----
    #if HAS_GPS == true
    if (gui_gps_coords) {
        if (gps_sats > 0 && gps_lat != 0.0) {
            lv_label_set_text_fmt(gui_gps_coords, "%.6f\n%.6f", gps_lat, gps_lon);
            lv_obj_set_style_text_color(gui_gps_coords, lv_color_hex(GUI_COL_TEAL), 0);
        } else {
            lv_label_set_text(gui_gps_coords, "No fix");
            lv_obj_set_style_text_color(gui_gps_coords, lv_color_hex(GUI_COL_DIM), 0);
        }

        lv_label_set_text_fmt(gui_gps_fix, "Sats: %d  HDOP: %.1f", gps_sats, gps_hdop);

        lv_label_set_text_fmt(gui_gps_alt, "Alt: %.0fm  Spd: %.1fkm/h", gps_alt, gps_speed);

        if (beacon_mode_active) {
            lv_label_set_text(gui_gps_beacon, "Beacon: active");
            lv_obj_set_style_text_color(gui_gps_beacon, lv_color_hex(GUI_COL_AMBER), 0);
        } else {
            lv_label_set_text(gui_gps_beacon, "Beacon: off");
            lv_obj_set_style_text_color(gui_gps_beacon, lv_color_hex(GUI_COL_DIM), 0);
        }
    }
    #endif
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
// Serial screenshot protocol:
// Trigger: host sends 4 bytes [0x52, 0x57, 0x53, 0x53] ("RWSS" = R-Watch Screen Shot)
// Response: "RWSS" + uint16_t(width) + uint16_t(height) + raw RGB565 LE pixels
#define GUI_SS_MAGIC_0 0x52
#define GUI_SS_MAGIC_1 0x57
#define GUI_SS_MAGIC_2 0x53
#define GUI_SS_MAGIC_3 0x53
static uint8_t gui_ss_state = 0;

void gui_check_screenshot_trigger(uint8_t byte_in) {
    const uint8_t magic[] = {GUI_SS_MAGIC_0, GUI_SS_MAGIC_1, GUI_SS_MAGIC_2, GUI_SS_MAGIC_3};
    if (byte_in == magic[gui_ss_state]) {
        gui_ss_state++;
        if (gui_ss_state == 4) {
            gui_ss_state = 0;
            if (gui_screenshot_buf) {
                Serial.write(magic, 4);
                uint16_t w = GUI_W, h = GUI_H;
                Serial.write((uint8_t *)&w, 2);
                Serial.write((uint8_t *)&h, 2);
                Serial.write((uint8_t *)gui_screenshot_buf, GUI_W * GUI_H * 2);
                Serial.flush();
            } else {
                // Buffer not allocated — send error marker
                Serial.write(magic, 4);
                uint16_t w = 0, h = 0;
                Serial.write((uint8_t *)&w, 2);
                Serial.write((uint8_t *)&h, 2);
                Serial.flush();
            }
        }
    } else {
        gui_ss_state = (byte_in == magic[0]) ? 1 : 0;
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
bool gui_screenshot_sd(const char *path = "/screenshot.bmp") {
    if (!gui_screenshot_buf) return false;

    // Init SD on shared SPI bus
    SPI.begin(SD_CLK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS, SPI)) {
        Serial.println("[screenshot] SD init failed");
        return false;
    }

    File f = SD.open(path, FILE_WRITE);
    if (!f) {
        Serial.println("[screenshot] file open failed");
        SD.end();
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
    SD.end();

    // Restart LoRa SPI after SD use (shared bus)
    SPI.end();

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

    gui_update_data();
    lv_timer_handler();
}

#endif // BOARD_MODEL == BOARD_TWATCH_ULT
#endif // GUI_H
