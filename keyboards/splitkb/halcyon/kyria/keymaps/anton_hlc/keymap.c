// Copyright 2024 splitkb.com (support@splitkb.com)
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

#ifdef OS_DETECTION_ENABLE
#    include "os_detection.h"
#    include "transactions.h"
#    include "split_util.h"
#endif

#ifdef HLC_TFT_DISPLAY
// Defined in users/halcyon_modules/splitkb/hlc_tft_display/hlc_tft_display.c
void splitkb_logo_sparkle(void);
#endif

// LED index for the CG_TOGG key (matrix [8][2], k8C — right-half thumb cluster).
#define LED_CG_TOGG 40

#ifdef OS_DETECTION_ENABLE
// Master detects OS and tracks the CG_TOGG swap state. Slave has no USB (so no
// OS detection) and never processes CG_TOGG keypresses (so its keymap_config
// stays at boot default). We sync both to the slave via a custom split
// transaction so the indicator can run identically on either half.
typedef struct {
    uint8_t os;        // os_variant_t
    uint8_t cg_swap;   // 0/1
} user_sync_t;

static volatile os_variant_t synced_host_os = OS_UNSURE;
static volatile bool         synced_cg_swap = false;

static void user_os_sync_slave_handler(uint8_t in_size, const void *in_data,
                                       uint8_t out_size, void *out_data) {
    if (in_size == sizeof(user_sync_t)) {
        const user_sync_t *p = in_data;
        synced_host_os = (os_variant_t)p->os;
        synced_cg_swap = p->cg_swap != 0;
    }
}

// Override the TFT module's weak getter so the display reads the split-synced
// value (slave's keymap_config never sees CG_TOGG presses).
bool hlc_cg_swap_state(void) {
    return synced_cg_swap;
}
#endif

enum layers {
    _QWERTY = 0,
    _NAV,
    _MOUSE,
    _MEDIA,
    _NUM,
    _SYM,
    _FUN,
    _ADJUST,
    _AUTO_MOUSE,
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

// macOS Globe (🌐) key — sent as USB HID Consumer page usage 0x29D
// (AC Keyboard Layout Select). macOS routes this to the Globe key handler,
// no Apple VID spoofing required.
enum custom_keycodes {
    AP_GLOBE = SAFE_RANGE,
};

// Tap dance: 1 tap = CAPS_WORD, 2 taps = CAPS_LOCK
enum tap_dance_codes {
    TD_CAPS_WORD_LOCK,
};

static void td_caps_finished(tap_dance_state_t *state, void *user_data) {
    if (state->count == 1) {
        // CW_TOGG can't go through tap_code16 (16-bit quantum keycode gets
        // truncated by register_code), so call the caps_word API directly.
        caps_word_on();
    } else {
        tap_code(KC_CAPS);
    }
}

tap_dance_action_t tap_dance_actions[] = {
    [TD_CAPS_WORD_LOCK] = ACTION_TAP_DANCE_FN(td_caps_finished),
};

// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
/*
 * Base Layer: QWERTY
 */
    [_QWERTY] = LAYOUT_split_3x6_5_hlc(
     _______, KC_Q,               KC_W,               KC_E,               KC_R,               KC_T,                                                                    KC_Y, KC_U,               KC_I,               KC_O,               KC_P,                  KC_LEFT_BRACKET,
     _______, MT(MOD_LGUI, KC_A), MT(MOD_LALT, KC_S), MT(MOD_LCTL, KC_D), MT(MOD_LSFT, KC_F), KC_G,                                                                    KC_H, MT(MOD_RSFT, KC_J), MT(MOD_RCTL, KC_K), MT(MOD_LALT, KC_L), MT(MOD_RGUI, KC_SCLN), KC_QUOTE,
     MS_BTN2, MT(MOD_LCTL, KC_Z), KC_X,            KC_C,               KC_V,               KC_B, EMOJI, KC_F13,        MS_BTN1, MS_BTN2,                            KC_N, KC_M,               KC_COMM,            KC_DOT,             KC_SLSH,               AP_GLOBE,
                                                     _______, LT(_ADJUST, RM_TOGG), LT(_MEDIA, KC_ESC), LT(_NAV, KC_SPACE), LT(_MOUSE, KC_TAB),     LT(_SYM, KC_ENTER), LT(_NUM, KC_BACKSPACE), LT(_FUN, KC_DELETE), CG_TOGG, _______,
     KC_MUTE, KC_NO, KC_NO, KC_NO, KC_NO,                                                                                                                              KC_MUTE, KC_NO, KC_NO, KC_NO, KC_NO
    ),

/*
 * Nav Layer: Navigation
 */
    [_NAV] = LAYOUT_split_3x6_5_hlc(
     _______, _______, _______, _______, _______, _______,                                     KC_REDO, KC_PASTE, KC_COPY, KC_CUT,  KC_UNDO,   KC_MCTL,
     _______, _______, _______, _______, _______, _______,                                     KC_LEFT, KC_DOWN,  KC_UP,   KC_RGHT, TD(TD_CAPS_WORD_LOCK), _______,
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
     _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,   _______,     _______,    _______,  _______,     _______, _______,
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
     QK_BOOT, _______, _______,  _______, _______,  _______, _______, _______, _______, _______,   _______, RM_SATD, RM_HUED, RM_VALD, RM_PREV, QK_BOOT,
                                _______, _______, _______, _______, _______,    _______, _______, _______, _______, _______,
     _______, _______, _______, _______, _______,                                                                _______, _______, _______, _______, _______
    ),

/*
 * Auto Mouse Layer: only activated automatically by trackpad motion.
 * Mostly transparent — the trackpad moves the cursor, the thumb cluster gives clicks,
 * the left encoder scrolls (handled in encoder_update_user).
 */
    [_AUTO_MOUSE] = LAYOUT_split_3x6_5_hlc(
     _______, _______, _______, _______, _______, _______,                                       _______, _______, _______, _______, _______, _______,
     _______, _______, _______, _______, _______, _______,                                       _______, _______, _______, _______, _______, _______,
     _______, _______, _______, MS_BTN2, MS_BTN1, _______, _______, _______, _______, _______,  _______, _______, _______, _______, _______, _______,
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

#ifdef OS_DETECTION_ENABLE
void keyboard_post_init_user(void) {
    transaction_register_rpc(USER_OS_SYNC, user_os_sync_slave_handler);
}

void housekeeping_task_user(void) {
    if (!is_keyboard_master()) {
        return;
    }
    user_sync_t cur = {
        .os      = (uint8_t)detected_host_os(),
        .cg_swap = keymap_config.swap_lctl_lgui ? 1 : 0,
    };
    // Always reflect locally — independent of split transport connectivity —
    // so master's indicator and TFT see the real state immediately.
    synced_host_os = (os_variant_t)cur.os;
    synced_cg_swap = cur.cg_swap != 0;

    if (!is_transport_connected()) {
        return;
    }
    static uint16_t last_sent_at = 0;
    static user_sync_t last_sent = { .os = OS_UNSURE, .cg_swap = 0 };
    if (cur.os != last_sent.os || cur.cg_swap != last_sent.cg_swap || timer_elapsed(last_sent_at) > 1000) {
        if (transaction_rpc_send(USER_OS_SYNC, sizeof(cur), &cur)) {
            last_sent = cur;
            last_sent_at = timer_read();
        }
    }
}
#endif

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
#ifdef HLC_TFT_DISPLAY
        splitkb_logo_sparkle();
#endif
    }
    switch (keycode) {
        case AP_GLOBE:
            host_consumer_send(record->event.pressed ? AC_NEXT_KEYBOARD_LAYOUT_SELECT : 0);
            return false;
    }
    return true;
}

#if defined(RGB_MATRIX_ENABLE) && defined(OS_DETECTION_ENABLE)
// Light CG_TOGG bright red when the host is macOS but the swap is off.
// synced_host_os is populated on master via detected_host_os() and pushed to
// slave via the USER_OS_SYNC split RPC (see housekeeping_task_user above) so
// both halves render the same indicator.
bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    if (LED_CG_TOGG < led_min || LED_CG_TOGG >= led_max) {
        return false;
    }
    if (synced_host_os == OS_MACOS && !synced_cg_swap) {
        rgb_matrix_set_color(LED_CG_TOGG, RGB_RED);
    }
    return false;
}
#endif

#ifdef POINTING_DEVICE_COMBINED
// Drag scroll: on the manual _MOUSE layer, convert trackpad cursor movement into
// scroll-wheel events (no cursor motion). Zeroing x/y also prevents auto-mouse
// from firing and shadowing _MOUSE with _AUTO_MOUSE.
//
// Even with POINTING_DEVICE_HIRES_SCROLL_ENABLE, macOS often ignores the Resolution
// Multiplier and treats each tick as a full wheel click. The accumulator+divisor
// throttles us to a sane rate regardless. Higher divisor = slower scroll.
#define SCROLL_DIVISOR_H 50.0f
#define SCROLL_DIVISOR_V 50.0f

static float scroll_accumulated_h = 0;
static float scroll_accumulated_v = 0;

report_mouse_t pointing_device_task_combined_user(report_mouse_t left_report, report_mouse_t right_report) {
    if (IS_LAYER_ON(_MOUSE)) {
        scroll_accumulated_h +=  (float)right_report.x / SCROLL_DIVISOR_H;
        scroll_accumulated_v += -(float)right_report.y / SCROLL_DIVISOR_V;

        right_report.h = (mouse_hv_report_t)scroll_accumulated_h;
        right_report.v = (mouse_hv_report_t)scroll_accumulated_v;

        scroll_accumulated_h -= (mouse_hv_report_t)scroll_accumulated_h;
        scroll_accumulated_v -= (mouse_hv_report_t)scroll_accumulated_v;

        right_report.x = 0;
        right_report.y = 0;
    }
    return pointing_device_combine_reports(left_report, right_report);
}

// Clear partial scroll fractions when leaving _MOUSE so they don't leak across sessions.
layer_state_t layer_state_set_user(layer_state_t state) {
    if (!IS_LAYER_ON_STATE(state, _MOUSE)) {
        scroll_accumulated_h = 0;
        scroll_accumulated_v = 0;
    }
    return state;
}
#endif

#ifdef ENCODER_ENABLE
bool encoder_update_user(uint8_t index, bool clockwise) {
    if (index == 0) {
        // LEFT soldered encoder
        switch (get_highest_layer(layer_state | default_layer_state)) {
            case _MOUSE:
            case _AUTO_MOUSE:
                // Scroll wheel
                if (clockwise) {
                    tap_code(MS_WHLD);
                } else {
                    tap_code(MS_WHLU);
                }
                break;
            case _MEDIA:
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
