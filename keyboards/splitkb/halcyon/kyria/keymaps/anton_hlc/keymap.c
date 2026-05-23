// Copyright 2024 splitkb.com (support@splitkb.com)
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

enum layers {
    _QWERTY = 0,
    _NAV,
    _MOUSE,
    _MEDIA,
    _NUM,
    _SYM,
    _FUN,
    _ADJUST,
};

// Aliases for readability
#define QWERTY   DF(_QWERTY)

#define CTL_ESC  MT(MOD_LCTL, KC_ESC)
#define CTL_QUOT MT(MOD_RCTL, KC_QUOTE)
#define CTL_MINS MT(MOD_RCTL, KC_MINUS)
#define ALT_ENT  MT(MOD_LALT, KC_ENT)

#define KC_COPY  LCTL(KC_C)
#define KC_PASTE LCTL(KC_V)
#define KC_CUT   LCTL(KC_X)
#define KC_UNDO  LCTL(KC_Z)
#define KC_REDO  LCTL(LSFT(KC_Z))

#define EMOJI           LCTL(LGUI(KC_SPACE))
#define CHANGE_LANGUAGE LGUI(KC_SPACE)

// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
/*
 * Base Layer: QWERTY
 */
    [_QWERTY] = LAYOUT_split_3x6_5_hlc(
     _______, KC_Q,               KC_W,               KC_E,               KC_R,               KC_T,                                                                    KC_Y, KC_U,               KC_I,               KC_O,               KC_P,                  KC_LEFT_BRACKET,
     _______, MT(MOD_LGUI, KC_A), MT(MOD_LALT, KC_S), MT(MOD_LCTL, KC_D), MT(MOD_LSFT, KC_F), KC_G,                                                                    KC_H, MT(MOD_RSFT, KC_J), MT(MOD_RCTL, KC_K), MT(MOD_LALT, KC_L), MT(MOD_RGUI, KC_SCLN), KC_QUOTE,
     MS_BTN2, MT(MOD_LCTL, KC_Z), KC_X,            KC_C,               KC_V,               KC_B, EMOJI, KC_F13,        MS_BTN1, MS_BTN2,                            KC_N, KC_M,               KC_COMM,            KC_DOT,             KC_SLSH,               CHANGE_LANGUAGE,
                                                     _______, LT(_ADJUST, RM_TOGG), LT(_MEDIA, KC_ESC), LT(_NAV, KC_SPACE), LT(_MOUSE, KC_TAB),     LT(_SYM, KC_ENTER), LT(_NUM, KC_BACKSPACE), LT(_FUN, KC_DELETE), CG_TOGG, _______,
     KC_MUTE, KC_NO, KC_NO, KC_NO, KC_NO,                                                                                                                              KC_MUTE, KC_NO, KC_NO, KC_NO, KC_NO
    ),

/*
 * Nav Layer: Navigation
 */
    [_NAV] = LAYOUT_split_3x6_5_hlc(
     _______, _______, _______, _______, _______, _______,                                     KC_REDO, KC_PASTE, KC_COPY, KC_CUT,  KC_UNDO,   _______,
     _______, _______, _______, _______, _______, _______,                                     KC_LEFT, KC_DOWN,  KC_UP,   KC_RGHT, KC_CAPS,   _______,
     _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, KC_HOME, KC_PGDN,  KC_PGUP, KC_END,  KC_INSERT, _______,
                                _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
     _______, _______, _______, _______, _______,                                                                _______, _______, _______, _______, _______
    ),

/*
 * Mouse Layer: Mousing
 */
    [_MOUSE] = LAYOUT_split_3x6_5_hlc(
     _______, _______, _______, _______, _______, _______,                                       KC_AGAIN,    KC_PASTE,   KC_COPY,  KC_CUT,      KC_UNDO, _______,
     _______, _______, _______, _______, _______, _______,                                       MS_LEFT,     MS_DOWN,    MS_UP,    MS_RGHT,     _______, _______,
     _______, _______, _______, MS_BTN2, MS_BTN1, _______, _______, _______, _______, _______,   _______,     _______,    _______,  _______,     _______, _______,
                                _______, _______, _______, _______, _______, MS_BTN1, MS_BTN3, MS_BTN2, _______, _______,
     _______, _______, _______, _______, _______,                                                                _______, _______, _______, _______, _______
    ),

/*
 * Media Layer: Media controls
 */
    [_MEDIA] = LAYOUT_split_3x6_5_hlc(
     _______, _______, _______, _______, _______, _______,                                       _______,             _______,           _______,         _______,             _______, _______,
     _______, _______, _______, _______, _______, _______,                                       KC_MEDIA_PREV_TRACK, KC_AUDIO_VOL_DOWN, KC_AUDIO_VOL_UP, KC_MEDIA_NEXT_TRACK, _______, _______,
     _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,   _______,             _______,           _______,         _______,             _______, _______,
                                _______, _______, _______, _______, _______, KC_MEDIA_STOP, KC_MEDIA_PLAY_PAUSE, KC_AUDIO_MUTE, _______, _______,
     _______, _______, _______, _______, _______,                                                                _______, _______, _______, _______, _______
    ),

/*
 * Num layer: Numbers and some symbols
 */
    [_NUM] = LAYOUT_split_3x6_5_hlc(
     _______, KC_LEFT_BRACKET, KC_7, KC_8, KC_9, KC_RIGHT_BRACKET,                                    _______, _______, _______, _______, _______, _______,
     _______, KC_SEMICOLON,    KC_4, KC_5, KC_6, KC_EQUAL,                                            _______, _______, _______, _______, _______, _______,
     _______, KC_GRAVE,        KC_1, KC_2, KC_3, KC_BACKSLASH, _______, _______, _______, _______,    _______, _______, _______, _______, _______, _______,
                                _______, _______, KC_DOT, KC_0, KC_MINUS,    _______, _______, _______, _______, _______,
     _______, _______, _______, _______, _______,                                                                _______, _______, _______, _______, _______
    ),

/*
 * Symbols layer: Symbols
 */
    [_SYM] = LAYOUT_split_3x6_5_hlc(
     _______, KC_LEFT_CURLY_BRACE, KC_AMPERSAND, KC_ASTERISK, KC_LEFT_PAREN,  KC_RIGHT_CURLY_BRACE,                                    _______, _______, _______, _______, _______, _______,
     _______, KC_COLON,            KC_DOLLAR,    KC_PERCENT,  KC_CIRCUMFLEX,  KC_PLUS,                                                 _______, _______, _______, _______, _______, _______,
     _______, KC_TILDE,            KC_EXCLAIM,   KC_AT,       KC_HASH,        KC_PIPE, _______, _______, _______, _______,             _______, _______, _______, _______, _______, _______,
                                _______, _______, KC_LEFT_PAREN, KC_RIGHT_PAREN, KC_UNDERSCORE,    _______, _______, _______, _______, _______,
     _______, _______, _______, _______, _______,                                                                _______, _______, _______, _______, _______
    ),

/*
 * Function Layer: Function keys
 */
    [_FUN] = LAYOUT_split_3x6_5_hlc(
     _______, KC_F12, KC_F7, KC_F8, KC_F9, KC_PRINT_SCREEN,                                       _______, _______, _______, _______, _______, _______,
     _______, KC_F11, KC_F4, KC_F5, KC_F6, KC_SCROLL_LOCK,                                        _______, _______, _______, _______, _______, _______,
     _______, KC_F10, KC_F1, KC_F2, KC_F3, KC_PAUSE, _______, _______, _______, _______,          _______, _______, _______, _______, _______, _______,
                                _______, _______, KC_APPLICATION, _______, _______,    _______, _______, _______, _______, _______,
     _______, _______, _______, _______, _______,                                                                _______, _______, _______, _______, _______
    ),

/*
 * Adjust Layer: Default layer settings, RGB
 */
    [_ADJUST] = LAYOUT_split_3x6_5_hlc(
     _______, RGB_M_P, RGB_M_K,  RGB_M_R, RGB_M_SN, RGB_M_X,                                       _______, _______, _______, _______, _______, _______,
     _______, RGB_M_G, RGB_M_TW, _______, _______,  _______,                                       RM_TOGG, RM_SATU, RM_HUEU, RM_VALU, RM_NEXT, _______,
     _______, _______, _______,  _______, _______,  _______, _______, _______, _______, _______,   _______, RM_SATD, RM_HUED, RM_VALD, RM_PREV, _______,
                                _______, _______, _______, _______, _______,    _______, _______, _______, _______, _______,
     _______, _______, _______, _______, _______,                                                                _______, _______, _______, _______, _______
    ),
};
// clang-format on

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
void pointing_device_init_user(void) {
    set_auto_mouse_enable(true);
}
#endif

#ifdef ENCODER_ENABLE
bool encoder_update_user(uint8_t index, bool clockwise) {
    if (index == 0) {
        // LEFT soldered encoder
        switch (get_highest_layer(layer_state | default_layer_state)) {
            case _MEDIA:
            case _MOUSE:
            case _QWERTY:
            case _NUM:
            case _SYM:
            case _FUN:
            case _ADJUST:
            case _NAV:
            default:
                // Undo / Redo
                if (clockwise) {
                    tap_code16(LGUI(LSFT(KC_Z)));
                } else {
                    tap_code16(LGUI(KC_Z));
                }
        }
    } else if (index == 2) {
        // RIGHT soldered encoder
        switch (get_highest_layer(layer_state | default_layer_state)) {
            case _MEDIA:
                // Volume control
                if (clockwise) {
                    tap_code(KC_VOLU);
                } else {
                    tap_code(KC_VOLD);
                }
                break;
            case _MOUSE:
            case _QWERTY:
            case _NUM:
            case _SYM:
            case _FUN:
            case _ADJUST:
            case _NAV:
            default:
                // Scroll 5 lines
                if (clockwise) {
                    tap_code(KC_DOWN);
                    tap_code(KC_DOWN);
                    tap_code(KC_DOWN);
                    tap_code(KC_DOWN);
                    tap_code(KC_DOWN);
                } else {
                    tap_code(KC_UP);
                    tap_code(KC_UP);
                    tap_code(KC_UP);
                    tap_code(KC_UP);
                    tap_code(KC_UP);
                }
        }
    }
    return false;
}
#endif
