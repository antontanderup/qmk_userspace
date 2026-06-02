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

// Cycle windows within the current app on macOS (Cmd+`). Written as
// LCTL(KC_GRAVE) so the CG_TOGG swap turns it into Cmd+grave at the host;
// outside mac mode it sends Ctrl+grave (VS Code terminal toggle on Win).
#define MAC_CYCLE LCTL(KC_GRAVE)

// EMOJI is a custom keycode below — branches on macos_mode: Globe+E on Mac
// (Sonoma+ picker), Win+. otherwise (Windows 10+ system picker).

// AP_GLOBE: macOS Globe (🌐) key — sent as USB HID Consumer page usage 0x29D
// (AC Keyboard Layout Select). macOS routes this to the Globe key handler.
//
// M_*: one-shot RGB matrix mode pickers. The legacy RGB_M_* keycodes don't
// work on rgb_matrix-only builds (they belong to the older rgblight code
// path), so we mint our own and call rgb_matrix_mode() directly.
enum custom_keycodes {
    AP_GLOBE = SAFE_RANGE,
    EMOJI,      // macos_mode → Globe+E; otherwise → Win+.
    M_PLAIN,    // SOLID_COLOR
    M_BREATH,   // BREATHING
    M_BAND,     // BAND_VAL
    M_CYCLE,    // CYCLE_LEFT_RIGHT (classic rainbow)
    M_PIN,      // CYCLE_PINWHEEL
    M_BEACON,   // RAINBOW_BEACON
    M_DROPS,    // RAINDROPS
    M_JELLY,    // JELLYBEAN_RAINDROPS
    M_HUEBR,    // HUE_BREATHING
    M_FLOW,     // PIXEL_FLOW
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
                                                     _______, LT(_ADJUST, RM_TOGG), LT(_MEDIA, KC_ESC), LT(_NAV, KC_SPACE), LT(_MOUSE, KC_TAB),     LT(_SYM, KC_ENTER), LT(_NUM, KC_BACKSPACE), LT(_FUN, KC_DELETE), LT(_ADJUST, CG_TOGG), _______,
     KC_MUTE, KC_NO, KC_NO, KC_NO, KC_NO,                                                                                                                              KC_MUTE, KC_NO, KC_NO, KC_NO, KC_NO
    ),

/*
 * Nav Layer: Navigation
 */
    [_NAV] = LAYOUT_split_3x6_5_hlc(
     _______, _______, _______, _______, _______, _______,                                     KC_REDO, KC_PASTE, KC_COPY, KC_CUT,  KC_UNDO,   KC_MCTL,
     _______, _______, _______, _______, _______, _______,                                     KC_LEFT, KC_DOWN,  KC_UP,   KC_RGHT, TD(TD_CAPS_WORD_LOCK), _______,
     _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, KC_HOME, KC_PGDN,  KC_PGUP, KC_END,  KC_INSERT, _______,
                                _______, _______, _______, _______, _______, _______, _______, _______, MAC_CYCLE, _______,
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
     _______, M_PLAIN,  M_BREATH, M_BAND,  M_CYCLE, M_PIN,                                         _______, _______, _______, _______, _______, _______,
     _______, M_BEACON, M_DROPS,  M_JELLY, M_HUEBR, M_FLOW,                                        RM_TOGG, RM_SATU, RM_HUEU, RM_SPDU, RM_NEXT, _______,
     QK_BOOT, _______, _______,  _______, _______,  _______, _______, _______, _______, _______,   _______, RM_SATD, RM_HUED, RM_SPDD, RM_PREV, QK_BOOT,
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
        case EMOJI: {
            // Cache mac state at press time so a release after a CG_TOGG flip
            // still tears down whatever the press registered.
            static bool emoji_used_mac = false;
            if (record->event.pressed) {
                emoji_used_mac = macos_mode();
                if (emoji_used_mac) {
                    // Globe + E — the Sonoma+ emoji picker. Needs
                    // KEYBOARD_SHARED_EP=yes so the consumer + keyboard
                    // reports can both be live at once.
                    host_consumer_send(AC_NEXT_KEYBOARD_LAYOUT_SELECT);
                    register_code(KC_E);
                } else {
                    // Win+. — Windows 10+ system emoji picker.
                    register_code16(LGUI(KC_DOT));
                }
            } else {
                if (emoji_used_mac) {
                    unregister_code(KC_E);
                    host_consumer_send(0);
                } else {
                    unregister_code16(LGUI(KC_DOT));
                }
            }
            return false;
        }
        // CG_TOGG and RM_TOGG are 16-bit Quantum keycodes, so they can't ride
        // in an LT() tap slot — only the low byte survives the LT() packing, so
        // the tap is silently truncated to a stray basic keycode (see NOTES.md).
        // We let real LT() handle the hold (momentary _ADJUST) and do the tap
        // action by hand here. The case labels truncate identically to the
        // stored keys, so they still match; we just ignore the broken tap
        // keycode and call the action directly.
        case LT(_ADJUST, CG_TOGG):
            if (record->tap.count && record->event.pressed) {
                // Mirror QK_MAGIC_TOGGLE_CTL_GUI exactly: toggle both Ctrl/GUI
                // swap pairs (right follows left) and persist. Toggling only
                // the left pair would desync the RCTL/RGUI home-row mods.
                keymap_config.swap_lctl_lgui = !keymap_config.swap_lctl_lgui;
                keymap_config.swap_rctl_rgui = keymap_config.swap_lctl_lgui;
                eeconfig_update_keymap(&keymap_config);
                return false;
            }
            return true;  // held → let QMK switch to _ADJUST
#ifdef RGB_MATRIX_ENABLE
        case LT(_ADJUST, RM_TOGG):
            if (record->tap.count && record->event.pressed) {
                rgb_matrix_toggle();  // persists to EEPROM, like RM_TOGG
                return false;
            }
            return true;  // held → let QMK switch to _ADJUST
        // One-shot mode pickers on _ADJUST. Only act on press; rgb_matrix_mode
        // persists to EEPROM (QMK handles wear-leveling internally).
        case M_PLAIN:  if (record->event.pressed) rgb_matrix_mode(RGB_MATRIX_SOLID_COLOR);         return false;
        case M_BREATH: if (record->event.pressed) rgb_matrix_mode(RGB_MATRIX_BREATHING);           return false;
        case M_BAND:   if (record->event.pressed) rgb_matrix_mode(RGB_MATRIX_BAND_VAL);            return false;
        case M_CYCLE:  if (record->event.pressed) rgb_matrix_mode(RGB_MATRIX_CYCLE_LEFT_RIGHT);    return false;
        case M_PIN:    if (record->event.pressed) rgb_matrix_mode(RGB_MATRIX_CYCLE_PINWHEEL);      return false;
        case M_BEACON: if (record->event.pressed) rgb_matrix_mode(RGB_MATRIX_RAINBOW_BEACON);      return false;
        case M_DROPS:  if (record->event.pressed) rgb_matrix_mode(RGB_MATRIX_RAINDROPS);           return false;
        case M_JELLY:  if (record->event.pressed) rgb_matrix_mode(RGB_MATRIX_JELLYBEAN_RAINDROPS); return false;
        case M_HUEBR:  if (record->event.pressed) rgb_matrix_mode(RGB_MATRIX_HUE_BREATHING);       return false;
        case M_FLOW:   if (record->event.pressed) rgb_matrix_mode(RGB_MATRIX_PIXEL_FLOW);          return false;
#endif
    }
    return true;
}

#ifdef RGB_MATRIX_ENABLE
static inline void paint_led(uint8_t led_min, uint8_t led_max, uint8_t led, uint8_t r, uint8_t g, uint8_t b) {
    if (led >= led_min && led < led_max) {
        rgb_matrix_set_color(led, r, g, b);
    }
}

#    ifdef OS_DETECTION_ENABLE
// CG-swap toggle flash: blink every LED 3x when the Control/GUI swap flips —
// white entering swapped (Mac) mode, green leaving it. Edge-detected on
// synced_cg_swap so both halves flash together (master updates it locally,
// slave via the USER_OS_SYNC RPC). Replaces the old OS-detection red warning,
// which broke when KEYBOARD_SHARED_EP changed the USB descriptors and made
// detected_host_os() mis-fingerprint macOS — see NOTES.md.
#        define CG_FLASH_BLINKS    3
#        define CG_FLASH_ON_MS     120
#        define CG_FLASH_OFF_MS    120
#        define CG_FLASH_PERIOD_MS (CG_FLASH_ON_MS + CG_FLASH_OFF_MS)
#        define CG_FLASH_TOTAL_MS  (CG_FLASH_BLINKS * CG_FLASH_PERIOD_MS)
// Suppress flashes for this long after boot: an all-LEDs-white blink coinciding
// with TFT power-on can brown out the display's init sequence (peak RGB draw).
#        define CG_FLASH_BOOT_GRACE_MS 3000
// White lights all 3 channels, so at full (0xFF) it draws ~3x the green flash
// and browns out the TFT. Cap it so total per-LED draw stays under the
// (proven-safe) green flash: 3 * 0x50 = 0xF0 < 0xFF.
#        define CG_FLASH_WHITE_LEVEL 0x50
static uint32_t cg_flash_start  = 0;
static bool     cg_flash_active = false;
static bool     cg_flash_white  = false;  // true = white (swapped/Mac), false = green
#    endif

// Each LED is gated independently so the slice [led_min, led_max) on either
// half only paints the LEDs it actually owns.
bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
#    ifdef OS_DETECTION_ENABLE
    // Arm the flash on a swap change. During the boot grace window we keep
    // prev_cg in lockstep with the current state so neither the persisted
    // initial value nor the slave's first sync from the master can trigger a
    // flash while the TFT is still powering on.
    {
        static bool prev_cg = false;
        const bool  cur_cg  = synced_cg_swap;
        if (timer_read32() < CG_FLASH_BOOT_GRACE_MS) {
            prev_cg = cur_cg;
        } else if (cur_cg != prev_cg) {
            prev_cg         = cur_cg;
            cg_flash_white  = cur_cg;        // white = entering swap/Mac, green = leaving
            cg_flash_start  = timer_read32();
            cg_flash_active = true;
        }
    }
    if (cg_flash_active) {
        const uint32_t elapsed = timer_elapsed32(cg_flash_start);
        if (elapsed >= CG_FLASH_TOTAL_MS) {
            cg_flash_active = false;
        } else {
            // Only the top row blinks. Flashing the whole board — even
            // current-capped — still browned out the TFT; 12 LEDs (6 per half,
            // from g_led_config: left 25-30, right 56-61) stays within budget.
            static const uint8_t cg_flash_leds[] = {
                25, 26, 27, 28, 29, 30,  // left-half top row
                56, 57, 58, 59, 60, 61,  // right-half top row
            };
            const bool on = (elapsed % CG_FLASH_PERIOD_MS) < CG_FLASH_ON_MS;
            uint8_t r = 0, g = 0, b = 0;
            if (on) {
                if (cg_flash_white) {
                    r = g = b = CG_FLASH_WHITE_LEVEL;  // dim white (current-capped)
                } else {
                    g = 0xFF;                          // green
                }
            }
            // Off-phase paints the row black against the live effect, so the row
            // blinks; the rest of the board keeps animating.
            for (size_t i = 0; i < ARRAY_SIZE(cg_flash_leds); i++) {
                paint_led(led_min, led_max, cg_flash_leds[i], r, g, b);
            }
            return false;  // flash owns the frame; non-row LEDs show the base effect
        }
    }
#    endif
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

    // _ADJUST: the whole layer is the RGB control panel, visualizing its own
    // state. Adjust keys preview their effects, "above column" LEDs show the
    // current color, mode pickers form a memorable rainbow, NEXT/PREV in amber,
    // QK_BOOT in red as a "don't fat-finger me" warning, RM_TOGG green.
    if (IS_LAYER_ON(_ADJUST)) {
        paint_led(led_min, led_max, 18, 0xFF, 0x00, 0x00);  // QK_BOOT left
        paint_led(led_min, led_max, 49, 0xFF, 0x00, 0x00);  // QK_BOOT right
        if (rgb_matrix_is_enabled()) {
            paint_led(led_min, led_max, 50, 0x00, 0xC0, 0x00);  // RM_TOGG — green
        } else {
            paint_led(led_min, led_max, 50, 0xFF, 0x00, 0x00);  // RM_TOGG — red (unreachable)
        }

        // Live HSV state from the matrix. All previews scale brightness with
        // the current val so they harmonize when the user dims the matrix.
        const uint8_t h = rgb_matrix_get_hue();
        const uint8_t s = rgb_matrix_get_sat();
        const uint8_t v = rgb_matrix_get_val();
        const uint8_t hue_delta = 16;

        const RGB rgb_current  = hsv_to_rgb((HSV){ h,                        s,    v });
        const RGB rgb_max_sat  = hsv_to_rgb((HSV){ h,                        0xFF, v });
        const RGB rgb_min_sat  = hsv_to_rgb((HSV){ h,                        0x30, v });
        const RGB rgb_hue_up   = hsv_to_rgb((HSV){ (uint8_t)(h + hue_delta), s,    v });
        const RGB rgb_hue_down = hsv_to_rgb((HSV){ (uint8_t)(h - hue_delta), s,    v });

        // SAT triplet
        paint_led(led_min, led_max, 57, rgb_current.r,  rgb_current.g,  rgb_current.b);   // above sat col
        paint_led(led_min, led_max, 51, rgb_max_sat.r,  rgb_max_sat.g,  rgb_max_sat.b);   // RM_SATU
        paint_led(led_min, led_max, 45, rgb_min_sat.r,  rgb_min_sat.g,  rgb_min_sat.b);   // RM_SATD
        // HUE triplet
        paint_led(led_min, led_max, 58, rgb_current.r,  rgb_current.g,  rgb_current.b);   // above hue col
        paint_led(led_min, led_max, 52, rgb_hue_up.r,   rgb_hue_up.g,   rgb_hue_up.b);    // RM_HUEU
        paint_led(led_min, led_max, 46, rgb_hue_down.r, rgb_hue_down.g, rgb_hue_down.b);  // RM_HUED
        // SPEED triplet (former VAL column). Above-column LED brightness
        // tracks the actual current speed (0-255) — turning the encoder for
        // brightness leaves it alone; pressing SPDU/SPDD makes it dim/brighten.
        const uint8_t speed = rgb_matrix_get_speed();
        const RGB rgb_spd_now  = hsv_to_rgb((HSV){ 130, 0xFF, speed             });
        const RGB rgb_spd_up   = hsv_to_rgb((HSV){ 130, 0xFF, v                 });
        const RGB rgb_spd_down = hsv_to_rgb((HSV){ 130, 0xFF, (uint8_t)(v >> 1) });
        paint_led(led_min, led_max, 59, rgb_spd_now.r,  rgb_spd_now.g,  rgb_spd_now.b);   // above speed col — live speed
        paint_led(led_min, led_max, 53, rgb_spd_up.r,   rgb_spd_up.g,   rgb_spd_up.b);    // RM_SPDU
        paint_led(led_min, led_max, 47, rgb_spd_down.r, rgb_spd_down.g, rgb_spd_down.b);  // RM_SPDD

        // Mode picker grid (left hand, excluding outer column + bottom row).
        // 10 keys across two rows, 10 hues distributed around the wheel
        // (~26 apart). Full saturation so each reads as visually distinct;
        // brightness tracks current val so it harmonizes with the right hand.
        // Builds muscle memory: "the green one is cycle", etc.
        static const struct { uint8_t led; uint8_t hue; } mode_pickers[] = {
            // Row 1 (calmer): solid → waves
            { 29,   0 },  // M_PLAIN  (red)
            { 28,  26 },  // M_BREATH (orange)
            { 27,  52 },  // M_BAND   (yellow)
            { 26,  78 },  // M_CYCLE  (chartreuse)
            { 25, 104 },  // M_PIN    (green)
            // Row 2 (busier): radial → particle
            { 23, 130 },  // M_BEACON (teal)
            { 22, 156 },  // M_DROPS  (cyan)
            { 21, 182 },  // M_JELLY  (blue)
            { 20, 208 },  // M_HUEBR  (purple)
            { 19, 234 },  // M_FLOW   (magenta)
        };
        for (size_t i = 0; i < ARRAY_SIZE(mode_pickers); i++) {
            const RGB rgb = hsv_to_rgb((HSV){ mode_pickers[i].hue, 0xFF, v });
            paint_led(led_min, led_max, mode_pickers[i].led, rgb.r, rgb.g, rgb.b);
        }

        // RM_NEXT / RM_PREV — amber pair, signals "cycle through modes".
        // Same color on both since the function is symmetric.
        const RGB rgb_nav = hsv_to_rgb((HSV){ 25, 0xFF, v });
        paint_led(led_min, led_max, 54, rgb_nav.r, rgb_nav.g, rgb_nav.b);  // RM_NEXT
        paint_led(led_min, led_max, 48, rgb_nav.r, rgb_nav.g, rgb_nav.b);  // RM_PREV
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
        // KC_MCTL and MAC_CYCLE — cool blue, outside the purple ramp. Both
        // are "switch/cycle on macOS" system actions, so same color groups
        // them visually.
        paint_led(led_min, led_max, 61, 0x60, 0xB0, 0xFF);  // KC_MCTL
        paint_led(led_min, led_max, 40, 0x60, 0xB0, 0xFF);  // MAC_CYCLE (CG_TOGG slot)
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

// _NAV flick gestures: a quick directional swipe on the trackpad fires a macOS
// Mission Control / Spaces action.
//   up    → Mission Control   (Ctrl+Up)
//   down  → dismiss it        (Esc)
//   left  → previous Space    (Ctrl+Left)
//   right → next Space        (Ctrl+Right)
//
// We accumulate trackpad deltas while the finger is moving; once the
// dominant-axis displacement crosses NAV_FLICK_THRESHOLD we fire once and lock
// until the finger lifts (motion idles for NAV_FLICK_IDLE_MS), so one swipe =
// one action. Ctrl combos go through tap_code16 so they bypass the CG_TOGG swap
// and reach the host as a *real* Ctrl — Mission Control / Spaces are Ctrl
// shortcuts, not Cmd (see "Gotchas" in NOTES.md). Trackpad y is positive
// downward, matching the drag-scroll convention above.
#define NAV_FLICK_THRESHOLD 80   // accumulated counts before a flick fires (tune to taste)
#define NAV_FLICK_IDLE_MS   150  // no-motion gap that ends a stroke

static int16_t  nav_flick_ax = 0;
static int16_t  nav_flick_ay = 0;
static uint16_t nav_flick_last_motion = 0;
static bool     nav_flick_fired = false;

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
    } else if (IS_LAYER_ON(_NAV)) {
        const int16_t dx = right_report.x;
        const int16_t dy = right_report.y;

        if (dx != 0 || dy != 0) {
            // A gap longer than the idle window means the previous swipe ended
            // (finger lifted) — start a fresh stroke.
            if (timer_elapsed(nav_flick_last_motion) > NAV_FLICK_IDLE_MS) {
                nav_flick_ax    = 0;
                nav_flick_ay    = 0;
                nav_flick_fired = false;
            }
            nav_flick_last_motion = timer_read();
            nav_flick_ax += dx;
            nav_flick_ay += dy;

            if (!nav_flick_fired) {
                const int16_t mag_x = nav_flick_ax < 0 ? -nav_flick_ax : nav_flick_ax;
                const int16_t mag_y = nav_flick_ay < 0 ? -nav_flick_ay : nav_flick_ay;
                if (mag_x >= NAV_FLICK_THRESHOLD || mag_y >= NAV_FLICK_THRESHOLD) {
                    if (mag_y >= mag_x) {
                        if (nav_flick_ay < 0) {
                            tap_code16(LCTL(KC_UP));     // flick up → Mission Control
                        } else {
                            tap_code(KC_ESC);            // flick down → dismiss
                        }
                    } else {
                        if (nav_flick_ax < 0) {
                            tap_code16(LCTL(KC_RIGHT));  // flick left → next Space
                        } else {
                            tap_code16(LCTL(KC_LEFT));   // flick right → previous Space
                        }
                    }
                    nav_flick_fired = true;
                }
            }
        }

        // Never let _NAV trackpad motion move the cursor or wake _AUTO_MOUSE.
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
#ifdef RGB_MATRIX_ENABLE
            case _ADJUST:
                // Matrix hue (right encoder does brightness on this layer)
                if (clockwise) {
                    rgb_matrix_increase_hue();
                } else {
                    rgb_matrix_decrease_hue();
                }
                break;
#endif
            case _MEDIA:
            case _QWERTY:
            case _NUM:
            case _SYM:
            case _FUN:
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
#ifdef RGB_MATRIX_ENABLE
            case _ADJUST:
                // Matrix brightness (so VAL keys are freed for speed)
                if (clockwise) {
                    rgb_matrix_increase_val();
                } else {
                    rgb_matrix_decrease_val();
                }
                break;
#endif
            case _MOUSE:
            case _QWERTY:
            case _NUM:
            case _SYM:
            case _FUN:
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
