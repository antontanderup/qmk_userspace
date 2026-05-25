# anton_hlc — personal Halcyon Kyria keymap

Built for a Halcyon Kyria rev4 with:

- **Left half**: TFT display module + soldered rotary encoder.
- **Right half**: Cirque trackpad module + soldered rotary encoder.
- Plain QMK (not Vial), macOS host.

## Layers

| Idx | Name | How it's reached |
|-----|------|------------------|
| 0 | `_QWERTY` | base |
| 1 | `_NAV` | hold `LT(_NAV, Space)` (left thumb) |
| 2 | `_MOUSE` | hold `LT(_MOUSE, Tab)` (left thumb) |
| 3 | `_MEDIA` | hold `LT(_MEDIA, Esc)` (left thumb) |
| 4 | `_NUM` | hold `LT(_NUM, Bspc)` (right thumb) |
| 5 | `_SYM` | hold `LT(_SYM, Enter)` (right thumb) |
| 6 | `_FUN` | hold `LT(_FUN, Del)` (right thumb) |
| 7 | `_ADJUST` | hold `LT(_ADJUST, RM_TOGG)` (left thumb) — has `QK_BOOT` for flashing |
| 8 | `_AUTO_MOUSE` | activated automatically by trackpad motion |

### Base (`_QWERTY`)
- Home-row mods, GACS order: `A=LGUI`, `S=LALT`, `D=LCTL`, `F=LSFT` on left; mirrored `J=RSFT`, `K=RCTL`, `L=LALT`, `;=RGUI` on right.
- `Z` is `MT(LCTL, Z)`.
- Left inner-bottom and right inner-bottom carry `MS_BTN2` / `MS_BTN1` / `MS_BTN2` for quick mouse clicks without leaving base.
- Outer-bottom thumb cluster: `EMOJI` (`Ctrl+Cmd+Space`) on left, `CHANGE_LANGUAGE` (`Cmd+Space`) on right.
- `KC_F13` is wired to be picked up by an OS-side hotkey app.

### `_MOUSE`
- Manual mousing layer.
- Right hand: `MS_LEFT/DOWN/UP/RGHT` for keyboard-driven cursor + clipboard ops on the top row.
- Left inner thumb-area: `MS_BTN2`, `MS_BTN1`.
- Right thumb cluster: `MS_BTN1 / MS_BTN3 / MS_BTN2`.
- **Trackpad is converted to drag-scroll while this layer is held** — see "Pointing device" below.

### `_AUTO_MOUSE`
- Mostly transparent. Right thumb cluster gets `MS_BTN1 / MS_BTN3 / MS_BTN2` so you can click while the trackpad is in use.
- No cursor-movement keys here — the trackpad is doing that.

### Other layers
Standard Halcyon-default arrangement (nav cluster, numpad-ish 1-9-0, symbols, F-keys, RGB matrix controls). `_ADJUST` carries `QK_BOOT` in the leftmost slot of row 3 — tap to drop into the RP2040 UF2 bootloader.

## Pointing device (Cirque)

Hardware quirks disabled (in `users/halcyon_modules/splitkb/hlc_cirque_trackpad/config.h` — personal preferences applied to the shared module config):
- Cursor glide off.
- Tap-to-click off (dedicated mouse-button keys instead).
- Circular / edge scrolling off.

Behavior in the keymap:
- `POINTING_DEVICE_AUTO_MOUSE_ENABLE` + `AUTO_MOUSE_DEFAULT_LAYER = 8` — trackpad motion activates `_AUTO_MOUSE` (default timeout 650 ms).
- `set_auto_mouse_enable(true)` called from `pointing_device_init_user`, because the runtime flag defaults to off.
- `pointing_device_task_combined_user` implements **drag-scroll on `_MOUSE`**: trackpad `x/y` is rewritten to `h/v` (with `y` inverted for natural-scroll feel). `x/y` is zeroed so auto-mouse can't fire and shadow `_MOUSE` with `_AUTO_MOUSE`.
- Scroll speed is throttled with a `SCROLL_DIVISOR_H/V = 50` float accumulator. Higher divisor → slower. macOS often ignores the HID Resolution Multiplier feature for generic mice, so this stays even with `POINTING_DEVICE_HIRES_SCROLL_ENABLE` on.
- `WHEEL_EXTENDED_REPORT` is on so `h/v` are 16-bit and don't clip at ±127.

## Encoders

The Kyria rev4 exposes 4 encoder slots (2 soldered + 2 Halcyon-module). This build only has the 2 soldered ones (`index 0` left, `index 2` right). Indices 1 and 3 are no-ops.

Defined in `encoder_update_user`:

| Side | Layer | Action |
|------|-------|--------|
| Left (idx 0) | `_MOUSE`, `_AUTO_MOUSE` | scroll wheel (`MS_WHLU/MS_WHLD`) |
| Left (idx 0) | everything else | undo / redo (`Cmd+Z` / `Cmd+Shift+Z`) |
| Right (idx 2) | `_MEDIA` | volume up / down |
| Right (idx 2) | everything else | scroll 5 lines (`Up×5` / `Down×5`) |

`ENCODER_MAP_ENABLE = no` — we route everything through `encoder_update_user` so we can use `tap_code16` for modifier combos and emit multiple taps per detent.

Encoder *press* is wired in the `LAYOUT_split_3x6_5_hlc` module row as `KC_MUTE` on both sides (only on `_QWERTY`).

## TFT display (left half)

Heavily customized — see `users/halcyon_modules/splitkb/hlc_tft_display/README.md` for the module internals. The current draw is:

- **Top row (y=4..33)**: 3 lock-state slots (Caps, Num, Scroll Lock) with rounded white background + black icon when active; dim gray icon on black when inactive.
- **Middle (y=62..181)**: 120 × 120 layer icon (Material Symbols), recolored to a single dim gray. Icons:
  - `_QWERTY` → splitkb logo (kb in a disc)
  - `_NAV` → 4-way arrows
  - `_MOUSE` → mouse
  - `_MEDIA` → music note
  - `_NUM` → calculator
  - `_SYM` → `{ }`
  - `_FUN` → Σ
  - `_ADJUST` → gear
  - `_AUTO_MOUSE` → trackpad-with-finger
- **Bottom row (y=210..239)**: 4 modifier slots (Ctrl `^`, Alt `⌥`, Cmd `⌘`, Shift `⇧`), same active/inactive treatment as locks.

All diffed redraws — only the band whose state changed gets re-rendered.

## Build

From `~/Development/keyboardstuff/qmk_userspace`:

```sh
qmk userspace-compile
```

Produces `kyria_left_tft.uf2` (left half) and `kyria_right_trackpad.uf2` (right half) in the userspace root.

Flash via UF2: double-tap the soldered reset on the half (or `QK_BOOT` on `_ADJUST`), the RP2040 mounts as `RPI-RP2`, drag the matching `.uf2` onto it.
