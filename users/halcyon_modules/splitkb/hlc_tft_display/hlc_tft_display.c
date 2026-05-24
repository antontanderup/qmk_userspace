// Copyright 2024 splitkb.com (support@splitkb.com)
// SPDX-License-Identifier: GPL-2.0-or-later

#include "halcyon.h"
#include "hlc_tft_display.h"
#include "modifiers.h"

#include "graphics/icons/splitkb.qgf.h"
#include "graphics/icons/open_with.qgf.h"
#include "graphics/icons/mouse.qgf.h"
#include "graphics/icons/music_note.qgf.h"
#include "graphics/icons/calculate.qgf.h"
#include "graphics/icons/data_object.qgf.h"
#include "graphics/icons/functions.qgf.h"
#include "graphics/icons/settings.qgf.h"

#include "graphics/icons/keyboard_control_key.qgf.h"
#include "graphics/icons/keyboard_option_key.qgf.h"
#include "graphics/icons/keyboard_command_key.qgf.h"
#include "graphics/icons/shift.qgf.h"

#include "graphics/icons/keyboard_capslock.qgf.h"
#include "graphics/icons/pin.qgf.h"
#include "graphics/icons/swap_vert.qgf.h"

// Layout
#define ICON_SIZE       120
#define ICON_X          ((LCD_WIDTH - ICON_SIZE) / 2)
// Vertically center the icon between the mod row (ends at y=33) and the lock row (starts at y=210).
#define ICON_Y          62

#define LOCK_SLOT_Y     4
#define LOCK_SLOT_H     30
#define LOCK_ICON_SIZE  26

#define MOD_SLOT_Y      (LCD_HEIGHT - MOD_SLOT_H)    // 210
#define MOD_SLOT_H      30
#define MOD_ICON_SIZE   26

#define HSV_ICON_DIM    0, 0, 130    // toned-down icon color for inactive state

// Draw a filled rectangle with 2px chamfered (slightly rounded) corners.
static void filled_rounded_rect(painter_device_t dev, int x1, int y1, int x2, int y2,
                                 uint8_t h, uint8_t s, uint8_t v) {
    qp_rect(dev, x1 + 2, y1,     x2 - 2, y2,     h, s, v, true);
    qp_rect(dev, x1,     y1 + 2, x2,     y2 - 2, h, s, v, true);
    qp_rect(dev, x1 + 1, y1 + 1, x2 - 1, y2 - 1, h, s, v, true);
}

painter_device_t lcd;
painter_device_t lcd_surface;

static uint8_t lcd_surface_fb[SURFACE_REQUIRED_BUFFER_BYTE_SIZE(135, 240, 16)];

static painter_image_handle_t layer_icons[8] = {0};
static painter_image_handle_t mod_icons[4]   = {0};  // Ctrl, Alt, GUI, Shift
static painter_image_handle_t lock_icons[3]  = {0};  // Caps, Num, Scroll

// Sentinel initial values force first draw
static uint8_t last_layer    = 0xFF;
static uint8_t last_mods     = 0xFF;
static led_t   last_led_state = {.raw = 0xFF};

static void draw_layer_icon(uint8_t layer) {
    // Clear top half (icon zone)
    qp_rect(lcd_surface, ICON_X, ICON_Y, ICON_X + ICON_SIZE - 1, ICON_Y + ICON_SIZE - 1, HSV_BLACK, true);

    // Auto-mouse layer reuses the manual _MOUSE icon (layer_icons[2]).
    uint8_t icon_idx = (layer == 8) ? 2 : layer;
    if (icon_idx >= 8) {
        return;
    }

    qp_drawimage_recolor(lcd_surface, ICON_X, ICON_Y, layer_icons[icon_idx], HSV_ICON_DIM, HSV_BLACK);
}

static void draw_mods_row(uint8_t mods) {
    // Clear mod band
    qp_rect(lcd_surface, 0, MOD_SLOT_Y, LCD_WIDTH - 1, MOD_SLOT_Y + MOD_SLOT_H - 1, HSV_BLACK, true);

    // 4 slots, 33 px wide, with 1 px gaps at x=33,67,101 (total = 4*33 + 3 = 135).
    static const int    slot_left[4]  = {0, 34, 68, 102};
    static const int    slot_width[4] = {33, 33, 33, 33};
    static const uint8_t masks[4]     = {MOD_MASK_CTRL, MOD_MASK_ALT, MOD_MASK_GUI, MOD_MASK_SHIFT};

    const int icon_y = MOD_SLOT_Y + (MOD_SLOT_H - MOD_ICON_SIZE) / 2;

    for (int i = 0; i < 4; i++) {
        bool active = (mods & masks[i]) != 0;
        int  icon_x = slot_left[i] + (slot_width[i] - MOD_ICON_SIZE) / 2;
        int  x_left  = slot_left[i];
        int  x_right = slot_left[i] + slot_width[i] - 1;
        int  y_top   = MOD_SLOT_Y;
        int  y_bot   = MOD_SLOT_Y + MOD_SLOT_H - 1;

        if (active) {
            filled_rounded_rect(lcd_surface, x_left, y_top, x_right, y_bot, HSV_WHITE);
            qp_drawimage_recolor(lcd_surface, icon_x, icon_y, mod_icons[i],
                                 HSV_BLACK, HSV_WHITE);
        } else {
            qp_drawimage_recolor(lcd_surface, icon_x, icon_y, mod_icons[i],
                                 HSV_ICON_DIM, HSV_BLACK);
        }
    }
}

static void draw_locks_row(led_t leds) {
    // Clear lock band
    qp_rect(lcd_surface, 0, LOCK_SLOT_Y, LCD_WIDTH - 1, LOCK_SLOT_Y + LOCK_SLOT_H - 1, HSV_BLACK, true);

    // 3 slots, 44/44/45 px wide, with 1 px gaps at x=44 and x=89 (total = 135).
    static const int slot_left[3]  = {0, 45, 90};
    static const int slot_width[3] = {44, 44, 45};
    const bool active[3] = { leds.caps_lock, leds.num_lock, leds.scroll_lock };

    const int icon_y = LOCK_SLOT_Y + (LOCK_SLOT_H - LOCK_ICON_SIZE) / 2;

    for (int i = 0; i < 3; i++) {
        int icon_x  = slot_left[i] + (slot_width[i] - LOCK_ICON_SIZE) / 2;
        int x_left  = slot_left[i];
        int x_right = slot_left[i] + slot_width[i] - 1;
        int y_top   = LOCK_SLOT_Y;
        int y_bot   = LOCK_SLOT_Y + LOCK_SLOT_H - 1;

        if (active[i]) {
            filled_rounded_rect(lcd_surface, x_left, y_top, x_right, y_bot, HSV_WHITE);
            qp_drawimage_recolor(lcd_surface, icon_x, icon_y, lock_icons[i],
                                 HSV_BLACK, HSV_WHITE);
        } else {
            qp_drawimage_recolor(lcd_surface, icon_x, icon_y, lock_icons[i],
                                 HSV_ICON_DIM, HSV_BLACK);
        }
    }
}

void update_display(void) {
    uint8_t cur_layer = get_highest_layer(layer_state | default_layer_state);
    uint8_t cur_mods  = get_mods() | get_oneshot_mods() | get_weak_mods();
    led_t   cur_leds  = host_keyboard_led_state();

    if (cur_layer != last_layer) {
        draw_layer_icon(cur_layer);
        last_layer = cur_layer;
    }
    if (cur_mods != last_mods) {
        draw_mods_row(cur_mods);
        last_mods = cur_mods;
    }
    if (cur_leds.raw != last_led_state.raw) {
        draw_locks_row(cur_leds);
        last_led_state = cur_leds;
    }
}

// Called from halcyon.c
void module_suspend_power_down_kb(void) {
    qp_power(lcd, false);
}

// Called from halcyon.c
void module_suspend_wakeup_init_kb(void) {
    qp_power(lcd, true);
}

// Called from halcyon.c
bool module_post_init_kb(void) {
    backlight_enable();

    lcd         = qp_st7789_make_spi_device(LCD_WIDTH, LCD_HEIGHT, LCD_CS_PIN, LCD_DC_PIN, LCD_RST_PIN, LCD_SPI_DIVISOR, LCD_SPI_MODE);
    lcd_surface = qp_make_rgb565_surface(LCD_WIDTH, LCD_HEIGHT, lcd_surface_fb);

    qp_init(lcd, LCD_ROTATION);
    qp_set_viewport_offsets(lcd, LCD_OFFSET_X, LCD_OFFSET_Y);
    qp_clear(lcd);
    qp_rect(lcd, 0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, HSV_BLACK, true);
    qp_power(lcd, true);
    qp_flush(lcd);

    qp_init(lcd_surface, LCD_ROTATION);
    qp_rect(lcd_surface, 0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, HSV_BLACK, true);

    // Load icons (held for firmware lifetime)
    layer_icons[0] = qp_load_image_mem(gfx_splitkb);
    layer_icons[1] = qp_load_image_mem(gfx_open_with);
    layer_icons[2] = qp_load_image_mem(gfx_mouse);
    layer_icons[3] = qp_load_image_mem(gfx_music_note);
    layer_icons[4] = qp_load_image_mem(gfx_calculate);
    layer_icons[5] = qp_load_image_mem(gfx_data_object);
    layer_icons[6] = qp_load_image_mem(gfx_functions);
    layer_icons[7] = qp_load_image_mem(gfx_settings);

    mod_icons[0]   = qp_load_image_mem(gfx_keyboard_control_key);
    mod_icons[1]   = qp_load_image_mem(gfx_keyboard_option_key);
    mod_icons[2]   = qp_load_image_mem(gfx_keyboard_command_key);
    mod_icons[3]   = qp_load_image_mem(gfx_shift);

    lock_icons[0]  = qp_load_image_mem(gfx_keyboard_capslock);
    lock_icons[1]  = qp_load_image_mem(gfx_pin);
    lock_icons[2]  = qp_load_image_mem(gfx_swap_vert);

    qp_surface_draw(lcd_surface, lcd, 0, 0, 0);
    qp_flush(lcd);

    if (!module_post_init_user()) {
        return false;
    }
    return true;
}

// Called from halcyon.c
bool display_module_housekeeping_task_kb(bool second_display) {
    if (!display_module_housekeeping_task_user(second_display)) {
        return false;
    }

    update_display();

    qp_surface_draw(lcd_surface, lcd, 0, 0, 0);
    qp_flush(lcd);

    return true;
}
