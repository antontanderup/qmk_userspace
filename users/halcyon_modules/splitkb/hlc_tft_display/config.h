// Copyright 2024 splitkb.com (support@splitkb.com)
// SPDX-License-Identifier: GPL-2.0-or-later

// Any QMK options should go here

#pragma once

#define HLC_TFT_DISPLAY

// LCD Configuration
#define LCD_RST_PIN GP26
#define LCD_CS_PIN GP13
#define LCD_DC_PIN GP16
// Divisor 0 gets clamped to 2 by the SPI driver → ~62.5 MHz, 4x the ST7789's
// rated write clock. That overclock intermittently corrupts window-set
// commands, splattering flushed pixels (usually the big layer icon) at the
// wrong panel location. 4 → ~31.25 MHz; bump to 8 (in-spec) if artifacts persist.
#define LCD_SPI_DIVISOR 4
#define LCD_SPI_MODE 3
#define LCD_WIDTH 135
#define LCD_HEIGHT 240
#define LCD_ROTATION QP_ROTATION_0
#define LCD_OFFSET_X 52
#define LCD_OFFSET_Y 40

// QP Configuration
#define QUANTUM_PAINTER_SUPPORTS_NATIVE_COLORS TRUE
#define ST7789_NO_AUTOMATIC_VIEWPORT_OFFSETS
#define ST7789_NUM_DEVICES 1

#define SURFACE_NUM_DEVICES 1

// Need room for layer + modifier + lock + caps_word icons; default is 8.
#define QUANTUM_PAINTER_NUM_IMAGES 20

// Backlight configuration
#undef BACKLIGHT_PIN
#define BACKLIGHT_PIN GP27

// Timeout configuration
#define QUANTUM_PAINTER_DISPLAY_TIMEOUT HLC_BACKLIGHT_TIMEOUT

// How often the full framebuffer is re-pushed to the panel, repainting any
// pixels a glitched SPI transfer left at the wrong location (the surface's
// dirty tracking can't see panel-side corruption).
#define HLC_TFT_RESYNC_INTERVAL_MS 30000
