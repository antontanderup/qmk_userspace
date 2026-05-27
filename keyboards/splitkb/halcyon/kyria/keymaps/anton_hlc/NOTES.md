# anton_hlc — internal reference

Quick-orient notes for future-me. The user-facing description lives in
`readme.md`; this file captures the non-obvious wiring and gotchas.

## Hardware

- Halcyon Kyria rev4, plain QMK (not Vial), branch `halcyon-qmk` of the splitkb userspace.
- Left half: TFT display module + 1 soldered rotary encoder.
- Right half: Cirque trackpad module + 1 soldered rotary encoder.
- Host: macOS (Apple Silicon).
- User typically runs with `CG_TOGG` swap **on** ("mac mode") so the labeled-LCTL home-row key sends Cmd.

## Layers (9 total)

| Idx | Name | How reached |
|-----|------|-------------|
| 0 | `_QWERTY` | base |
| 1 | `_NAV` | `LT(_NAV, Space)` left thumb |
| 2 | `_MOUSE` | `LT(_MOUSE, Tab)` left thumb |
| 3 | `_MEDIA` | `LT(_MEDIA, Esc)` left thumb |
| 4 | `_NUM` | `LT(_NUM, Bspc)` right thumb |
| 5 | `_SYM` | `LT(_SYM, Enter)` right thumb |
| 6 | `_FUN` | `LT(_FUN, Del)` right thumb |
| 7 | `_ADJUST` | `LT(_ADJUST, RM_TOGG)` left thumb. Holds `QK_BOOT` for flashing. |
| 8 | `_AUTO_MOUSE` | activated automatically by trackpad motion. `AUTO_MOUSE_DEFAULT_LAYER = 8`. |

## Home-row mods (`_QWERTY`)

Left  outer→inner: `A=LGUI` `S=LALT` `D=LCTL` `F=LSFT`
Right outer→inner: `;=RGUI` `L=LALT` `K=RCTL` `J=RSFT`

`Z` is `MT(LCTL, Z)`. Outer thumb columns hold `EMOJI` (`Ctrl+Cmd+Space`)
and `AP_GLOBE` (macOS Globe key — see below).

## Custom keycodes

```c
enum custom_keycodes { AP_GLOBE = SAFE_RANGE };
```

- **`AP_GLOBE`** — macOS Globe (🌐) key. Handled in `process_record_user`
  by `host_consumer_send(AC_NEXT_KEYBOARD_LAYOUT_SELECT)` (USB HID Consumer
  page usage `0x29D`). Requires `KEYBOARD_SHARED_EP = yes` in rules.mk so the
  consumer report shares the keyboard endpoint (needed for Globe+E, Globe+F,
  etc. chords).

## Tap dances

```c
enum tap_dance_codes { TD_CAPS_WORD_LOCK };
```

- **`TD(TD_CAPS_WORD_LOCK)`** on `_NAV` (right hand row 2, position above
  Insert) — 1 tap = `CAPS_WORD` (via `caps_word_on()` directly, not via
  `tap_code16(CW_TOGG)` which doesn't work — 16-bit quantum keycodes get
  truncated by `register_code`). 2 taps = real `KC_CAPS`.

## OS detection + CG_TOGG state — split sync

This is the trickiest part of the keymap. **The slave has no USB**, so:
1. `detected_host_os()` on slave always returns `OS_UNSURE`.
2. `keymap_config.swap_lctl_lgui` on slave never gets updated (CG_TOGG presses
   are only processed on master).

To make the LED indicator (master/slave-agnostic) and TFT display (which lives
on the left half regardless of which half is master) reflect reality, we sync
both pieces of state from master to slave via a custom split RPC:

```c
#define SPLIT_TRANSACTION_IDS_USER USER_OS_SYNC   // config.h

typedef struct { uint8_t os; uint8_t cg_swap; } user_sync_t;
static volatile os_variant_t synced_host_os = OS_UNSURE;
static volatile bool         synced_cg_swap = false;
```

- `keyboard_post_init_user` registers `user_os_sync_slave_handler`.
- `housekeeping_task_user` (master only): always updates master's local
  `synced_*` (independent of transport state — keeps the master responsive
  even if transport blips). When transport is connected, pushes the packet
  on change + a 1 s keep-alive.
- Indicator and TFT both read the `synced_*` variables.
- Keymap-level conditionals should call `macos_mode()` instead of reading
  `synced_cg_swap` directly. The TFT module stays in raw "CG swap" terms
  (it's generic); only this keymap interprets CG-on as "Mac mode".

**TFT override hook**: `hlc_tft_display.c` declares `__attribute__((weak)) bool
hlc_macos_mode(void)` returning `keymap_config.swap_lctl_lgui` (the module
treats CG-swap-on as Mac mode since it ships Apple/Cmd icons). This keymap
overrides it to return `macos_mode()` so the TFT mod row shows correct icons
even when running on the slave (USB on right side).

## LED indicators

In `rgb_matrix_indicators_advanced_user`, two independent per-LED checks:

- **`LED_CG_TOGG = 40`** — bright red when `synced_host_os == OS_MACOS && !macos_mode()`.
  Derived from g_led_config row 8 col 2 (matrix `[8][2]` = `k8C`), the
  right-hand thumb cluster outer slot where `CG_TOGG` is bound on base.
- **`LED_CAPS_WORD_LOCK = 54`** — red when caps lock is on, blue when caps word
  is on. Derived from g_led_config row 6 col 5 (matrix `[6][5]` = `R11`), the
  fifth key from the left on the right-hand second row, where the
  `TD(TD_CAPS_WORD_LOCK)` tap dance lives on `_NAV`.

Each LED is range-gated independently against `[led_min, led_max)` so either
half only paints LEDs it owns.

OS detection needs:
- `OS_DETECTION_ENABLE = yes` in rules.mk
- `#define OS_DETECTION_SINGLE_REPORT` in config.h — ARM Macs cause repeated
  re-detection that never settles inside the debounce window without this.
- `#define OS_DETECTION_DEBOUNCE 500`

## Trackpad: drag scroll on `_MOUSE`

`pointing_device_task_combined_user` (in keymap.c, gated on
`POINTING_DEVICE_COMBINED`):

- On `_MOUSE` layer: trackpad x/y converted to scroll wheel via float
  accumulator with `SCROLL_DIVISOR_H / V = 50.0f`. Higher divisor = slower.
- x/y zeroed so auto-mouse can't shadow `_MOUSE` with `_AUTO_MOUSE`.
- `POINTING_DEVICE_HIRES_SCROLL_ENABLE` is on (macOS often ignores it, so the
  divisor still does the actual throttling). `WHEEL_EXTENDED_REPORT` on so
  `h/v` are int16 instead of int8 — no clipping at ±127.
- `layer_state_set_user` resets the accumulator when `_MOUSE` leaves the
  stack (prevents fractional value leaking into next session).

Cirque gestures (cursor glide, tap-to-click, edge scroll) are **disabled**
upstream in `users/halcyon_modules/splitkb/hlc_cirque_trackpad/config.h` —
shared module config edit, applies to any keymap using this fork.

## Auto mouse layer

`POINTING_DEVICE_AUTO_MOUSE_ENABLE` + `AUTO_MOUSE_DEFAULT_LAYER = 8` in
config.h. **Must** call `set_auto_mouse_enable(true)` in
`pointing_device_init_user` — the runtime flag defaults to off even with
the define. `_AUTO_MOUSE` keymap is mostly transparent; right-thumb gets
`MS_BTN1 / MS_BTN3 / MS_BTN2` for clicks while cursor is on trackpad.

## Encoders

`ENCODER_MAP_ENABLE = no` — everything in `encoder_update_user` for
flexibility (`tap_code16` for modifier combos, multi-tap per detent).

| Side | Layer | Action |
|------|-------|--------|
| Left  (idx 0) | `_MOUSE`, `_AUTO_MOUSE` | `MS_WHLU/D` |
| Left  (idx 0) | other | `Cmd+Z` / `Cmd+Shift+Z` (undo/redo) |
| Right (idx 2) | `_MEDIA` | volume |
| Right (idx 2) | other | scroll 5 lines (`Up×5` / `Down×5`) |

Slots 1 and 3 are the inactive Halcyon-module encoder positions (we only have
the 2 soldered).

## Sparkle on QWERTY icon

`process_record_user` calls `splitkb_logo_sparkle()` on every key press
(gated on `HLC_TFT_DISPLAY` so right-half build skips). The TFT module:
- Recolors one random non-black pixel of the splitkb disc to a random vivid HSV.
- Persists sparkles in a 512-slot ring buffer; replayed every time the
  layer-0 icon redraws (which happens on every layer change).
- Reset only on firmware reboot.

## TFT display roles

| Row | y range | Content |
|-----|---------|---------|
| Lock row | 4–33 | Caps Word · Caps Lock · CG_TOGG-state (Apple ↔ ◆ filled) |
| Layer icon | 62–181 | 120×120, dim-gray, single splitkb logo on QWERTY |
| Mod row | 210–239 | 4 slots: positions track LEFT-HAND mod keys; icons swap with CG_TOGG state |

Mod-row icon at slot 2 swaps ⌃ ↔ ⌘ depending on swap state; slot 0 swaps
between ⌘ (or ◆ outlined when not swapped) and ⌃. All driven by
`synced_cg_swap`.

## Files at a glance

- `keymap.c` — layers, custom keycodes, tap dance, encoder, drag scroll, OS sync, LED indicator.
- `config.h` — `POINTING_DEVICE_AUTO_MOUSE_ENABLE`, `AUTO_MOUSE_DEFAULT_LAYER 8`, hi-res scroll + extended wheel, OS detection tunables, `SPLIT_TRANSACTION_IDS_USER USER_OS_SYNC`.
- `rules.mk` — toggles for `TAP_DANCE`, `CAPS_WORD`, `OS_DETECTION`, `KEYBOARD_SHARED_EP`, `USER_NAME := halcyon_modules`.
- `readme.md` — user-facing description.
- `NOTES.md` — this file.

## Build

```sh
cd ~/Development/keyboardstuff/qmk_userspace
qmk userspace-compile
```

Produces `kyria_left_tft.uf2` and `kyria_right_trackpad.uf2`. Both **must**
be re-flashed after any change touching `config.h`, sync logic, or shared
module code.

## Gotchas I keep tripping on

- `tap_code16(CW_TOGG)` doesn't work — call `caps_word_on()` directly.
- `tap_code16(LCTL(KC_X))` skips the CG_TOGG modifier swap. If you need a
  specific host-side combo, emit the post-swap keycode directly.
- Custom keycode handlers in `process_record_user` must `return false` to
  consume the event; otherwise QMK processes the keycode further.
- Image pool: `QUANTUM_PAINTER_NUM_IMAGES = 20` in the TFT module config.
  We use 18 currently; adding more icons may require bumping it again.
- `is_keyboard_master()` only flips after USB enumeration completes. Don't
  rely on it during boot/init.
- Split-pointing reports: when only one half has the trackpad, the trackpad
  data lands in `right_report` (or `left_report`) regardless of which side
  is master. We modify `right_report` since the Cirque is on the right.
- `keymap_config` doesn't auto-sync across the split. CG state has to be
  manually broadcast (see split sync section above).
