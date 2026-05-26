// Copyright 2024 splitkb.com (support@splitkb.com)
// SPDX-License-Identifier: GPL-2.0-or-later

#include "halcyon.h"
#include "hlc_tft_display.h"
#include "modifiers.h"
#include "timer.h"
#include <stdlib.h>

#include "graphics/icons/splitkb.qgf.h"
#include "graphics/icons/open_with.qgf.h"
#include "graphics/icons/mouse.qgf.h"
#include "graphics/icons/music_note.qgf.h"
#include "graphics/icons/calculate.qgf.h"
#include "graphics/icons/data_object.qgf.h"
#include "graphics/icons/functions.qgf.h"
#include "graphics/icons/settings.qgf.h"
#include "graphics/icons/trackpad_input.qgf.h"

#include "graphics/icons/keyboard_control_key.qgf.h"
#include "graphics/icons/keyboard_option_key.qgf.h"
#include "graphics/icons/keyboard_command_key.qgf.h"
#include "graphics/icons/shift.qgf.h"
#include "graphics/icons/diamond_mod.qgf.h"

#include "graphics/icons/uppercase.qgf.h"
#include "graphics/icons/keyboard_capslock.qgf.h"
#include "graphics/icons/apple_logo.qgf.h"
#include "graphics/icons/diamond.qgf.h"

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

static painter_image_handle_t layer_icons[9] = {0};
static painter_image_handle_t mod_icons[4]   = {0};  // Ctrl, Alt, GUI, Shift
static painter_image_handle_t diamond_mod_icon = NULL; // Diamond (30x30) for the mod row's GUI position when CG_TOGG is OFF
static painter_image_handle_t lock_icons[2]  = {0};  // Caps Word, Caps Lock
static painter_image_handle_t cg_icon_swap   = NULL; // Apple   (CG swap on)
static painter_image_handle_t cg_icon_nrm    = NULL; // Diamond (CG swap off)

// Sentinel initial values force first draw
static uint8_t last_layer    = 0xFF;
static uint8_t last_mods     = 0xFF;
static led_t   last_led_state    = {.raw = 0xFF};
static bool    last_caps_word    = true;  // not 0 → forces first draw
static bool    last_cg_swap      = true;  // not equal to initial false → forces first draw

// Ring buffer of accumulated sparkles on the splitkb (QWERTY) icon. Replayed
// every time draw_layer_icon redraws layer 0 so sparkles survive layer switches.
#define SPARKLE_BUFFER_SIZE 512
typedef struct {
    uint16_t x;
    uint16_t y;
    uint8_t  h;
    uint8_t  s;
    uint8_t  v;
} sparkle_t;
static sparkle_t sparkles[SPARKLE_BUFFER_SIZE];
static uint16_t  sparkle_count = 0;
static uint16_t  sparkle_next  = 0;

static void draw_layer_icon(uint8_t layer) {
    // Clear top half (icon zone)
    qp_rect(lcd_surface, ICON_X, ICON_Y, ICON_X + ICON_SIZE - 1, ICON_Y + ICON_SIZE - 1, HSV_BLACK, true);

    if (layer >= 9) {
        return;
    }

    qp_drawimage_recolor(lcd_surface, ICON_X, ICON_Y, layer_icons[layer], HSV_ICON_DIM, HSV_BLACK);

    // Replay accumulated sparkles on top of the freshly drawn splitkb logo.
    if (layer == 0) {
        for (uint16_t i = 0; i < sparkle_count; i++) {
            qp_setpixel(lcd_surface, sparkles[i].x, sparkles[i].y,
                        sparkles[i].h, sparkles[i].s, sparkles[i].v);
        }
    }
}

void splitkb_logo_sparkle(void) {
    // Only sparkle while the QWERTY layer (splitkb logo) is on screen.
    if (get_highest_layer(layer_state | default_layer_state) != 0) {
        return;
    }

    // Pick a random pixel inside the icon's bounding box.
    int x = ICON_X + (rand() % ICON_SIZE);
    int y = ICON_Y + (rand() % ICON_SIZE);

    // The RGB565 framebuffer: 2 bytes/pixel, row-major. Skip if currently black
    // (outside the disc, or the kb cutout) so sparkle only lands on the icon body.
    size_t idx = ((size_t)y * LCD_WIDTH + (size_t)x) * 2u;
    if (lcd_surface_fb[idx] == 0 && lcd_surface_fb[idx + 1] == 0) {
        return;
    }

    // Random vivid color: any hue, biased saturated + bright.
    uint8_t h = rand() & 0xFF;
    uint8_t s = 160 + (rand() & 0x5F);
    uint8_t v = 200 + (rand() & 0x3F);

    // Store in ring buffer so it survives layer switches.
    sparkles[sparkle_next] = (sparkle_t){ (uint16_t)x, (uint16_t)y, h, s, v };
    sparkle_next = (sparkle_next + 1) % SPARKLE_BUFFER_SIZE;
    if (sparkle_count < SPARKLE_BUFFER_SIZE) sparkle_count++;

    qp_setpixel(lcd_surface, x, y, h, s, v);
}

static void draw_mods_row(uint8_t mods, bool cg_swapped) {
    // Clear mod band
    qp_rect(lcd_surface, 0, MOD_SLOT_Y, LCD_WIDTH - 1, MOD_SLOT_Y + MOD_SLOT_H - 1, HSV_BLACK, true);

    // 4 slots, 33 px wide, with 1 px gaps at x=33,67,101 (total = 4*33 + 3 = 135).
    static const int    slot_left[4]  = {0, 34, 68, 102};
    static const int    slot_width[4] = {33, 33, 33, 33};
    // Masks flip Ctrl/GUI when CG_TOGG is on so the lit slot matches what the host sees.
    const uint8_t masks[4] = {
        cg_swapped ? MOD_MASK_CTRL : MOD_MASK_GUI,
        MOD_MASK_ALT,
        cg_swapped ? MOD_MASK_GUI  : MOD_MASK_CTRL,
        MOD_MASK_SHIFT,
    };

    // Icons show what each finger does on the host. In mac mode (swap on) the
    // Cmd-producing slot shows ⌘; in default mode the GUI-producing slot shows
    // ◆ (a neutral "GUI key" glyph since it's not Apple-Cmd anymore).
    // Swap OFF: A=GUI(◆), S=Alt, D=Ctrl,    F=Shift -> ◆ ⌥ ⌃ ⇧
    // Swap ON : A=Ctrl,   S=Alt, D=Cmd(⌘), F=Shift -> ⌃ ⌥ ⌘ ⇧
    painter_image_handle_t icons[4] = {
        cg_swapped ? mod_icons[0]    /*Ctrl*/ : diamond_mod_icon /*◆*/,
        mod_icons[1] /*Alt*/,
        cg_swapped ? mod_icons[2]    /*Cmd*/  : mod_icons[0]     /*Ctrl*/,
        mod_icons[3] /*Shift*/,
    };

    const int icon_y = MOD_SLOT_Y + (MOD_SLOT_H - MOD_ICON_SIZE) / 2;

    for (int i = 0; i < 4; i++) {
        bool active = (mods & masks[i]) != 0;
        int  icon_x = slot_left[i] + (slot_width[i] - MOD_ICON_SIZE) / 2;
        int  x_left  = slot_left[i];
        int  x_right = slot_left[i] + slot_width[i] - 1;
        int  y_top   = MOD_SLOT_Y;
        int  y_bot   = MOD_SLOT_Y + MOD_SLOT_H - 1;

        painter_image_handle_t icon = icons[i];

        if (active) {
            filled_rounded_rect(lcd_surface, x_left, y_top, x_right, y_bot, HSV_WHITE);
            qp_drawimage_recolor(lcd_surface, icon_x, icon_y, icon,
                                 HSV_BLACK, HSV_WHITE);
        } else {
            qp_drawimage_recolor(lcd_surface, icon_x, icon_y, icon,
                                 HSV_ICON_DIM, HSV_BLACK);
        }
    }
}

static void draw_locks_row(led_t leds, bool caps_word, bool cg_swapped) {
    // Clear lock band
    qp_rect(lcd_surface, 0, LOCK_SLOT_Y, LCD_WIDTH - 1, LOCK_SLOT_Y + LOCK_SLOT_H - 1, HSV_BLACK, true);

    // 3 slots: 44/44/45 px wide, with 1 px gaps at x=44 and x=89 (total = 135).
    static const int slot_left[3]  = {0, 45, 90};
    static const int slot_width[3] = {44, 44, 45};

    // Slot 2 (CG_TOGG): apple icon when swapped, diamond when not. The icon
    // itself conveys the state; the slot never lights up (no white background).
    const bool active[3] = { caps_word, leds.caps_lock, false };
    painter_image_handle_t icons[3] = {
        lock_icons[0], lock_icons[1],
        cg_swapped ? cg_icon_swap : cg_icon_nrm,
    };

    const int icon_y = LOCK_SLOT_Y + (LOCK_SLOT_H - LOCK_ICON_SIZE) / 2;

    for (int i = 0; i < 3; i++) {
        int icon_x  = slot_left[i] + (slot_width[i] - LOCK_ICON_SIZE) / 2;
        int x_left  = slot_left[i];
        int x_right = slot_left[i] + slot_width[i] - 1;
        int y_top   = LOCK_SLOT_Y;
        int y_bot   = LOCK_SLOT_Y + LOCK_SLOT_H - 1;

        if (active[i]) {
            filled_rounded_rect(lcd_surface, x_left, y_top, x_right, y_bot, HSV_WHITE);
            qp_drawimage_recolor(lcd_surface, icon_x, icon_y, icons[i],
                                 HSV_BLACK, HSV_WHITE);
        } else {
            qp_drawimage_recolor(lcd_surface, icon_x, icon_y, icons[i],
                                 HSV_ICON_DIM, HSV_BLACK);
        }
    }
}

void update_display(void) {
    uint8_t cur_layer     = get_highest_layer(layer_state | default_layer_state);
    uint8_t cur_mods      = get_mods() | get_oneshot_mods() | get_weak_mods();
    led_t   cur_leds      = host_keyboard_led_state();
    bool    cur_caps_word = is_caps_word_on();
    bool    cur_cg_swap   = keymap_config.swap_lctl_lgui;

    bool cg_changed = (cur_cg_swap != last_cg_swap);

    if (cur_layer != last_layer) {
        draw_layer_icon(cur_layer);
        last_layer = cur_layer;
    }
    if (cur_mods != last_mods || cg_changed) {
        draw_mods_row(cur_mods, cur_cg_swap);
        last_mods = cur_mods;
    }
    if (cur_leds.raw != last_led_state.raw || cur_caps_word != last_caps_word || cg_changed) {
        draw_locks_row(cur_leds, cur_caps_word, cur_cg_swap);
        last_led_state = cur_leds;
        last_caps_word = cur_caps_word;
    }
    last_cg_swap = cur_cg_swap;
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
    layer_icons[8] = qp_load_image_mem(gfx_trackpad_input);

    mod_icons[0]   = qp_load_image_mem(gfx_keyboard_control_key);
    mod_icons[1]   = qp_load_image_mem(gfx_keyboard_option_key);
    mod_icons[2]   = qp_load_image_mem(gfx_keyboard_command_key);
    mod_icons[3]   = qp_load_image_mem(gfx_shift);
    diamond_mod_icon = qp_load_image_mem(gfx_diamond_mod);

    lock_icons[0]  = qp_load_image_mem(gfx_uppercase);
    lock_icons[1]  = qp_load_image_mem(gfx_keyboard_capslock);
    cg_icon_swap   = qp_load_image_mem(gfx_apple_logo);
    cg_icon_nrm    = qp_load_image_mem(gfx_diamond);

    qp_surface_draw(lcd_surface, lcd, 0, 0, 0);
    qp_flush(lcd);

    // Seed for splitkb_logo_sparkle (cosmetic; quality of seed doesn't matter much).
    srand((unsigned int)timer_read32());

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
