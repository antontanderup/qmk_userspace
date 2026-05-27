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

// LED indices, derived from g_led_config in users/halcyon_modules/splitkb/halcyon.c.
// CG_TOGG key (matrix [8][2], k8C — right-half thumb cluster).
#define LED_CG_TOGG        40
// Caps Word / Caps Lock TD key (matrix [6][5], R11 — right-half row 2, fifth from left).
#define LED_CAPS_WORD_LOCK 54

// Home-row mod LEDs. Lit by rgb_matrix_indicators_advanced_user to show which
// modifier is currently held. We light both physical bindings of each mod
// regardless of which side triggered it. Z is also MT(LCTL,...) but is left
// uncolored — it's just a convenience bind and shouldn't visually compete with
// the home-row Ctrl key.
//   Left  row 2: A=23, S=22, D=21, F=20   (matrix row 1, cols 5/4/3/2)
//   Right row 2: J=51, K=52, L=53, ;=54   (matrix row 6, cols 2/3/4/5)
#define LED_HRM_GUI_L 23  // A — MT(LGUI, A)
#define LED_HRM_ALT_L 22  // S — MT(LALT, S)
#define LED_HRM_CTL_L 21  // D — MT(LCTL, D)
#define LED_HRM_SFT_L 20  // F — MT(LSFT, F)
#define LED_HRM_SFT_R 51  // J — MT(RSFT, J)
#define LED_HRM_CTL_R 52  // K — MT(RCTL, K)
#define LED_HRM_ALT_R 53  // L — MT(LALT, L)
#define LED_HRM_GUI_R 54  // ; — MT(RGUI, ;)

// _AUTO_MOUSE click keys, overlaid on base C, V (row 2) and R (row 1).
#define LED_AM_BTN2 15  // C — MS_BTN2 (right click)
#define LED_AM_BTN1 14  // V — MS_BTN1 (left click)
#define LED_AM_BTN3 26  // R — MS_BTN3 (middle click)

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
// value (slave's keymap_config never sees CG_TOGG presses). Uses
// synced_cg_swap directly because the keymap-level macos_mode() helper is
// defined further down (and resolves to the same value in this build).
bool hlc_macos_mode(void) {
    return synced_cg_swap;
}
#endif

// Keymap convention: CG_TOGG-on means the user is driving a Mac (so the
// labeled-LCTL home-row key sends Cmd). With OS_DETECTION we use the
// split-synced value so both halves agree; without it we fall back to
// keymap_config (only correct on master). Defined outside the OS_DETECTION
// block so indicators that don't depend on OS detection (e.g. the mod LED
// painter) can still ask "are we in mac mode?".
static inline bool macos_mode(void) {
#ifdef OS_DETECTION_ENABLE
    return synced_cg_swap;
#else
    return keymap_config.swap_lctl_lgui;
#endif
}

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
     AP_GLOBE, MT(MOD_LCTL, KC_Z), KC_X,            KC_C,               KC_V,               KC_B, EMOJI, KC_F13,        MS_BTN1, MS_BTN2,                            KC_N, KC_M,               KC_COMM,            KC_DOT,             KC_SLSH,               AP_GLOBE,
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
     _______, _______, _______, _______, MS_BTN3, _______,                                       _______, _______, _______, _______, _______, _______,
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

#ifdef RGB_MATRIX_ENABLE
static inline void paint_led(uint8_t led_min, uint8_t led_max, uint8_t led, uint8_t r, uint8_t g, uint8_t b) {
    if (led >= led_min && led < led_max) {
        rgb_matrix_set_color(led, r, g, b);
    }
}

// Each LED is gated independently so the slice [led_min, led_max) on either
// half only paints the LEDs it actually owns.
bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    // QMK's `mod_config()` applies the CG swap BEFORE the mod is registered
    // (see quantum/keycode_config.c), so get_mods() returns post-swap bits.
    // That means in mac mode, MOD_MASK_CTRL is set when host sees Ctrl held
    // (= physical LGUI keys A,; were pressed), and MOD_MASK_GUI when host
    // sees Cmd (= physical LCTL keys D,K). The table flips accordingly so we
    // always light the keys the user actually pressed.
    const bool mac = macos_mode();
    const struct { uint8_t mask; uint8_t led_l; uint8_t led_r; } hrm_leds[] = {
        { MOD_MASK_SHIFT, LED_HRM_SFT_L,                          LED_HRM_SFT_R                          },
        { MOD_MASK_ALT,   LED_HRM_ALT_L,                          LED_HRM_ALT_R                          },
        { MOD_MASK_CTRL,  mac ? LED_HRM_GUI_L : LED_HRM_CTL_L,    mac ? LED_HRM_GUI_R : LED_HRM_CTL_R    },
        { MOD_MASK_GUI,   mac ? LED_HRM_CTL_L : LED_HRM_GUI_L,    mac ? LED_HRM_CTL_R : LED_HRM_GUI_R    },
    };

    const uint8_t mods = get_mods() | get_weak_mods() | get_oneshot_mods();
    for (size_t i = 0; i < ARRAY_SIZE(hrm_leds); i++) {
        if (mods & hrm_leds[i].mask) {
            paint_led(led_min, led_max, hrm_leds[i].led_l, RGB_WHITE);
            paint_led(led_min, led_max, hrm_leds[i].led_r, RGB_WHITE);
        }
    }

    // _AUTO_MOUSE: highlight the click keys so they're findable when the
    // trackpad has just yanked you onto the layer.
    if (IS_LAYER_ON(_AUTO_MOUSE)) {
        paint_led(led_min, led_max, LED_AM_BTN1, RGB_PURPLE);
        paint_led(led_min, led_max, LED_AM_BTN2, RGB_PURPLE);
        paint_led(led_min, led_max, LED_AM_BTN3, RGB_PURPLE);
    }

    // _NUM: paint the left-hand numpad. Digits in a cool, light blue;
    // surrounding symbols/operators in the same hue but more saturated so
    // they read as a distinct group at a glance.
    if (IS_LAYER_ON(_NUM)) {
        static const uint8_t num_leds[] = {
            28, 27, 26,  // 7 8 9
            22, 21, 20,  // 4 5 6
            16, 15, 14,  // 1 2 3
            7,           // 0
        };
        static const uint8_t sym_leds[] = {
            29, 25,      // [ ]
            23, 19,      // ; =
            17, 13,      // grave, backslash
            8,  6,       // . -
        };
        for (size_t i = 0; i < ARRAY_SIZE(num_leds); i++) {
            paint_led(led_min, led_max, num_leds[i], 0x60, 0xB0, 0xFF);
        }
        for (size_t i = 0; i < ARRAY_SIZE(sym_leds); i++) {
            paint_led(led_min, led_max, sym_leds[i], 0x05, 0x12, 0x28);
        }
    }

    if (IS_LAYER_ON(_SYM)) {
        static const uint8_t num_leds[] = {
            28, 27, 26,  // & * (
            22, 21, 20,  // $ % ^
            16, 15, 14,  // ! @ #
            7,           // )
        };
        static const uint8_t sym_leds[] = {
            29, 25,      // { }
            23, 19,      // : +
            17, 13,      // ~ |
            8,  6,       // ( _
        };
        for (size_t i = 0; i < ARRAY_SIZE(num_leds); i++) {
            paint_led(led_min, led_max, num_leds[i], 0xFF, 0xE0, 0x10);
        }
        for (size_t i = 0; i < ARRAY_SIZE(sym_leds); i++) {
            paint_led(led_min, led_max, sym_leds[i], 0x1A, 0x16, 0x02);
        }
    }

    // _MEDIA: full chaos. Each bound key cycles through every hue at full
    // saturation, with a prime-offset phase so all 7 LEDs show different
    // colors at any given moment. ~3.8s full cycle (256 hues * 15ms tick).
    if (IS_LAYER_ON(_MEDIA)) {
        static const uint8_t media_leds[] = {
            50, 51, 52, 53,  // PREV VOL- VOL+ NEXT
            37, 38, 39,      // STOP PLAY/PAUSE MUTE (right thumbs)
        };
        const uint8_t base_hue = (uint8_t)(timer_read32() / 15);
        for (size_t i = 0; i < ARRAY_SIZE(media_leds); i++) {
            HSV hsv = { .h = (uint8_t)(base_hue + i * 37), .s = 0xFF, .v = 0xE0 };
            RGB rgb = hsv_to_rgb(hsv);
            paint_led(led_min, led_max, media_leds[i], rgb.r, rgb.g, rgb.b);
        }
    }

    // _MOUSE: ocean green on the cursor + click keys (the only parts of the
    // layer that aren't transparent). Single tier — no need to differentiate.
    if (IS_LAYER_ON(_MOUSE)) {
        static const uint8_t mouse_leds[] = {
            50, 51, 52, 53,  // MS_LEFT MS_DOWN MS_UP MS_RGHT
            37, 38, 39,      // MS_BTN1 MS_BTN3 MS_BTN2 (right thumbs R21/R22/R23)
        };
        for (size_t i = 0; i < ARRAY_SIZE(mouse_leds); i++) {
            paint_led(led_min, led_max, mouse_leds[i], 0x10, 0xC0, 0x80);
        }
    }

    // _NAV: purple gradient on the right hand. Arrows primary (brightest,
    // slightly desaturated/lavender), edit + page-nav row secondary (mid,
    // more saturated), INSERT tertiary (barely-on, fully saturated).
    // Mission Control (KC_MCTL) painted blue separately so it reads as a
    // distinct system key rather than a fourth gradient step.
    if (IS_LAYER_ON(_NAV)) {
        static const uint8_t nav_primary_leds[] = {
            50, 51, 52, 53,      // LEFT DOWN UP RIGHT
            44, 45, 46, 47,      // HOME PGDN PGUP END
        };
        static const uint8_t nav_secondary_leds[] = {
            56, 57, 58, 59, 60,  // REDO PASTE COPY CUT UNDO
            54,                  // TD(CAPS_WORD_LOCK) — overridden red/blue when active
        };
        static const uint8_t nav_tertiary_leds[] = {
            48,                  // INSERT
        };
        for (size_t i = 0; i < ARRAY_SIZE(nav_primary_leds); i++) {
            paint_led(led_min, led_max, nav_primary_leds[i], 0xA0, 0x60, 0xFF);
        }
        for (size_t i = 0; i < ARRAY_SIZE(nav_secondary_leds); i++) {
            paint_led(led_min, led_max, nav_secondary_leds[i], 0x20, 0x00, 0x40);
        }
        for (size_t i = 0; i < ARRAY_SIZE(nav_tertiary_leds); i++) {
            paint_led(led_min, led_max, nav_tertiary_leds[i], 0x06, 0x00, 0x10);
        }
        // KC_MCTL — cool blue, outside the purple ramp.
        paint_led(led_min, led_max, 61, 0x60, 0xB0, 0xFF);
    }

    // _FUN: three tiers of red on the left hand. F1-F9 primary (brightest,
    // slightly warm), F10-F12 secondary (mid), and the system keys
    // (PrtSc/ScrLk/Pause/App) tertiary (dimmest). Brightness drops ~3x per
    // tier; saturation climbs as brightness falls.
    if (IS_LAYER_ON(_FUN)) {
        static const uint8_t fun_primary_leds[] = {
            28, 27, 26,  // F7 F8 F9
            22, 21, 20,  // F4 F5 F6
            16, 15, 14,  // F1 F2 F3
        };
        static const uint8_t fun_secondary_leds[] = {
            29,          // F12
            23,          // F11
            17,          // F10
        };
        static const uint8_t fun_tertiary_leds[] = {
            25,          // KC_PRINT_SCREEN
            19,          // KC_SCROLL_LOCK
            13,          // KC_PAUSE
            8,           // KC_APPLICATION (thumb)
        };
        // All tiers are pure red (G=B=0) — only the R channel varies. Any G or
        // B value at all drifts the hue toward pink/orange on this hardware.
        for (size_t i = 0; i < ARRAY_SIZE(fun_primary_leds); i++) {
            paint_led(led_min, led_max, fun_primary_leds[i], 0xFF, 0x00, 0x00);
        }
        for (size_t i = 0; i < ARRAY_SIZE(fun_secondary_leds); i++) {
            paint_led(led_min, led_max, fun_secondary_leds[i], 0x18, 0x00, 0x00);
        }
        for (size_t i = 0; i < ARRAY_SIZE(fun_tertiary_leds); i++) {
            paint_led(led_min, led_max, fun_tertiary_leds[i], 0x06, 0x00, 0x00);
        }
    }

#    ifdef OS_DETECTION_ENABLE
    // CG_TOGG: bright red when host is macOS but swap is off (i.e. user forgot
    // to enable mac mode). synced_* are populated on master and pushed to slave
    // via the USER_OS_SYNC split RPC, so both halves render identically.
    if (LED_CG_TOGG >= led_min && LED_CG_TOGG < led_max) {
        if (synced_host_os == OS_MACOS && !macos_mode()) {
            rgb_matrix_set_color(LED_CG_TOGG, RGB_RED);
        }
    }
#    endif
    // Caps Lock / Caps Word on the TD key. Painted last so it overrides any
    // held-mod tint at the same LED (matters because ; is RGUI and shares
    // LED 54). Caps Lock wins over Caps Word if both somehow on.
    if (LED_CAPS_WORD_LOCK >= led_min && LED_CAPS_WORD_LOCK < led_max) {
        if (host_keyboard_led_state().caps_lock) {
            rgb_matrix_set_color(LED_CAPS_WORD_LOCK, RGB_RED);
        } else if (is_caps_word_on()) {
            rgb_matrix_set_color(LED_CAPS_WORD_LOCK, RGB_BLUE);
        }
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
