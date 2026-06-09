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
| 3 | `_WINDOW` | `LT(_WINDOW, Esc)` left thumb. macOS window tiling (Rectangle). |
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
- **`EMOJI`** — mode-aware emoji picker. In `macos_mode()` it fires Globe+E
  (modern Sonoma+ picker) by sending the AC_NEXT_KEYBOARD_LAYOUT_SELECT
  consumer + register_code(KC_E) simultaneously. Outside mac mode it fires
  `LGUI(KC_DOT)` = Win+. (Windows 10+ system emoji picker; with CG_TOGG off
  the LGUI bit reaches the host as the Windows key). The "was mac at press
  time" decision is cached in a static so the release path tears down
  whatever the press registered, even if CG_TOGG flips mid-hold.
- **`WM_*`** — macOS window-management keycodes (Rectangle) on the `_WINDOW`
  layer. Emitted via `tap_code16` to bypass the CG_TOGG swap. See the Window
  management section below.

## App launchers (`APP_*` on `_NAV`)

Spotlight-driven, host-setup-free. Each `APP_*` keycode runs `app_launch()`:
`tap_code16(LGUI(KC_SPACE))` → `wait_ms` → `send_string("name")` → `wait_ms` →
`tap_code(KC_ENTER)`. So it opens Spotlight, types the app name, and launches the
top hit. Bound on the `_NAV` left hand by letter mnemonic (hold left-thumb Space,
tap the letter):

| Key | Keycode | Types | LED (brand) |
|-----|---------|-------|-------------|
| C | `APP_CODE` | `code`      | blue (LED 15) |
| F | `APP_FF`   | `firefox`   | orange (LED 20) |
| T | `APP_TERM` | `terminal`  | green (LED 25) |
| S | `APP_SIM`  | `simulator` | grey (LED 22) |

- `⌘Space` goes through `tap_code16` to dodge the CG_TOGG swap (a keymap `⌘`
  becomes `⌃` in mac mode — same footgun as the window layer). Assumes Spotlight
  is on `⌘Space` (not remapped to Alfred/Raycast).
- Timing lives in `SPOTLIGHT_OPEN_MS` (250) and `SPOTLIGHT_RESOLVE_MS` (250). If a
  launch occasionally fires Enter before Spotlight resolves the hit, raise the
  resolve delay. The `wait_ms` calls block the matrix for ~450 ms per launch —
  fine for a deliberate app-jump.
- To add an app: new `APP_*` keycode, a `case` calling `app_launch("query")`, a
  free `_NAV` key, an LED, and a label in the LayerPeek generator.

## LT() taps with Quantum keycodes (CG_TOGG / RM_TOGG)

The two `_ADJUST` thumb keys are `LT(_ADJUST, CG_TOGG)` (right) and
`LT(_ADJUST, RM_TOGG)` (left). `LT(layer, kc)` only keeps the **low byte** of
`kc` (8 bits for the tap, 4 for the layer), so a 16-bit Quantum keycode like
`CG_TOGG` (Magic range, `0x70xx`) or `RM_TOGG` (RGB range) gets truncated — the
tap fires a stray basic keycode and the real action never runs.

Fix lives in `process_record_user`: real `LT()` handles the **hold** (momentary
`_ADJUST`); the **tap** is intercepted and the action done by hand. The `case
LT(_ADJUST, CG_TOGG):` label truncates identically to the stored key, so it
still matches — we just ignore the broken tap keycode.

- **CG_TOGG tap** mirrors `QK_MAGIC_TOGGLE_CTL_GUI`: flips **both**
  `swap_lctl_lgui` and `swap_rctl_rgui` (right follows left) then
  `eeconfig_update_keymap(&keymap_config)`. Toggling only the left pair would
  desync the RCTL/RGUI home-row mods. Verified against
  `qmk_firmware/quantum/process_keycode/process_magic.c`.
- **RM_TOGG tap** calls `rgb_matrix_toggle()` (the EEPROM-persisting variant).
- Pattern: `if (record->tap.count && record->event.pressed) { …; return false; }
  return true;` — consume only the tap press, let the hold fall through to QMK.
  Same family as the `tap_code16(CW_TOGG)` gotcha at the bottom of this file.

## Tap dances

```c
enum tap_dance_codes { TD_CAPS_WORD_LOCK };
```

- **`TD(TD_CAPS_WORD_LOCK)`** on `_NAV` (right hand row 2, position above
  Insert) — 1 tap = `CAPS_WORD` (via `caps_word_on()` directly, not via
  `tap_code16(CW_TOGG)` which doesn't work — 16-bit quantum keycodes get
  truncated by `register_code`). 2 taps = real `KC_CAPS`.

## OS detection + CG_TOGG + caps word — split sync

This is the trickiest part of the keymap. **The slave has no USB**, so:
1. `detected_host_os()` on slave always returns `OS_UNSURE`.
2. `keymap_config.swap_lctl_lgui` on slave never gets updated (CG_TOGG presses
   are only processed on master).
3. `is_caps_word_on()` is master-only internal state — QMK has **no built-in
   split sync** for caps word (unlike caps/num/scroll lock, which ride
   `SPLIT_LED_STATE_ENABLE`). Without syncing it, the right-hand Shift LED (J)
   wouldn't light for caps word even though the left (F) does.

To make the LED indicator (master/slave-agnostic) and TFT display (which lives
on the left half regardless of which half is master) reflect reality, we sync
all three pieces of state from master to slave via one custom split RPC:

```c
#define SPLIT_TRANSACTION_IDS_USER USER_OS_SYNC   // config.h

typedef struct { uint8_t os; uint8_t cg_swap; uint8_t caps_word; } user_sync_t;
static volatile os_variant_t synced_host_os   = OS_UNSURE;
static volatile bool         synced_cg_swap   = false;
static volatile bool         synced_caps_word = false;
```

`caps_word` piggybacks on this existing transaction rather than adding a second
one. The caps indicator reads `synced_caps_word` on both halves (one-frame
latency on the master is invisible).

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

`rgb_matrix_indicators_advanced_user` paints three groups, each LED gated
against `[led_min, led_max)` so either half only paints LEDs it owns.

**Held home-row mods** — all lit white. Painted first so caps overrides on
conflict at LED 54.

Driven by a small `hrm_leds[]` table mapping each `MOD_MASK_*` to its two
physical LED bindings; the indicator loops and paints both LEDs of any held
mod. The CTRL and GUI rows are mac-mode-aware: QMK applies the CG swap inside
`mod_config()` *before* the mod is registered (see
`quantum/keycode_config.c`), so `get_mods()` returns post-swap bits. In mac
mode, pressing D registers MOD_BIT(LGUI) and pressing A registers
MOD_BIT(LCTL); the table flips the LED targets so the lit keys are the ones
the user actually pressed, not the opposite pair.

Z is also `MT(LCTL, Z)` but deliberately uncolored — it's a convenience bind
for one-handed copy/paste and shouldn't visually compete with the home-row.

LED indices derived from g_led_config (Kyria rev4 block in
`users/halcyon_modules/splitkb/halcyon.c`):

```
A=23 S=22 D=21 F=20   (matrix row 1)        ; J=51 K=52 L=53 ;=54   (matrix row 6)
```

**_AUTO_MOUSE click keys** — all lit whenever `_AUTO_MOUSE` is active so the
click positions are findable when the trackpad has just yanked you onto the
layer. `LED_AM_BTN1 = 14` (V, MS_BTN1) and `LED_AM_BTN2 = 15` (C, MS_BTN2)
purple; `LED_AM_BTN3 = 26` (R, MS_BTN3) blue to distinguish it as the
less-frequent middle click.

**_NUM / _SYM layer tints** — when either layer is on, the left-hand cluster
is painted via two inline `static const` LED arrays per layer. Same two-tier
brightness relationship (~10% luminance ratio) on both, different base hue:

- `_NUM`: primary digits cool light blue `(0x60, 0xB0, 0xFF)`, border
  symbols/operators dim `(0x05, 0x12, 0x28)`.
- `_SYM`: primary shifted glyphs saturated yellow `(0xFF, 0xE0, 0x10)`, border
  symbols dim `(0x1A, 0x16, 0x02)`.
- `_ADJUST`: both QK_BOOT keys (LEDs 18 and 49 — outer corners of row 3)
  painted bright red as a "don't hit by accident" warning. RM_TOGG (LED 50)
  green when `rgb_matrix_is_enabled()` is true; the "red when off" branch is
  unreachable because indicator callbacks don't run while the matrix is off
  (kept in code for clarity, but expect to only see green). The whole layer is the RGB control panel, visualizing its own state. Live
HSV state is read each call via `rgb_matrix_get_hue/sat/val` and converted
through `hsv_to_rgb()`, so every cell updates as soon as you press an
adjust key.

  - **SAT triplet**: SATU (LED 51) = current hue at max sat; SATD (LED 45) =
    current hue at min visible sat (S=0x30 — fully desaturated would just be
    gray); above-column LED 57 (R02) = current color at actual current sat.
  - **HUE triplet**: HUEU (LED 52) = hue +16/256 (~22°); HUED (LED 46) = hue
    -16/256; above-column LED 58 (R03) = current color.
  - **SPEED triplet** (former VAL column): SPDU (LED 53) full-brightness
    cyan, SPDD (LED 47) half-brightness cyan, above-column LED 59 (R04)
    cyan brightness = current `rgb_matrix_get_speed()`. RM_VALU/RM_VALD are
    no longer on the keymap — brightness lives on the right encoder
    (`_ADJUST` layer), freeing the val column for speed.
  - **Mode pickers (left hand)**: 10 custom keycodes `M_PLAIN/BREATH/BAND/
    CYCLE/PIN` (row 1) and `M_BEACON/DROPS/JELLY/HUEBR/FLOW` (row 2). Each
    maps to a specific `RGB_MATRIX_*` enum via `process_record_user` calling
    `rgb_matrix_mode()`. The legacy `RGB_M_*` keycodes don't work on
    rgb_matrix-only builds — they belong to the older rgblight code path and
    silently no-op here, which is why we mint our own. Indicator: 10 hues
    distributed ~26 apart around the wheel at full saturation, brightness
    tracking current val. Builds color → mode muscle memory.
  - **NEXT/PREV (LEDs 54, 48)**: matched amber pair (hue 25 at current val)
    signaling "cycle through modes". Same color since the action is symmetric.

  Note: `MAC_CYCLE` (= `LCTL(KC_GRAVE)`, cycles windows within current app on
  Mac) is bound on the `_NAV` thumb row at the CG_TOGG slot (matrix `[8,2]`,
  LED 40). On `_NAV` it's painted the same cool blue as KC_MCTL — both are
  macOS system-switch actions, so grouping them visually makes sense. When
  not in mac mode the CG_TOGG warning red on LED 40 overrides the blue,
  which is the correct precedence.

  Mode-keycode-to-enum lookup wasn't trivially available (RGB_M_* routes
  through QMK's compatibility layer), so the mode pickers don't highlight
  the active mode — possible follow-up if exact enum constants get pinned
  down. LED 54 (RM_NEXT) is also LED_CAPS_WORD_LOCK; the caps lock/word
  indicator paints later, so it correctly overrides the amber when active.
- `_WINDOW`: Rectangle tiling map (see the Window management section below).
  Left hand is a 2×3 grid of **sixths** in orange `(0xFF, 0x55, 0x00)` — top
  sixths on LEDs 28/27/26 (W E R), bottom on 16/15/14 (X C V) — laid out like
  the screen, with **Center** on D (LED 21) green. Right hand: **halves** on the
  home-row arrows (LEDs 50–53) cyan; **thirds** across the top row (LEDs 57–60)
  blue, with the two-thirds keys (58/59) brighter since they're the most-used
  split; **Maximize** on Y (LED 56) white. Static colors — no animation.
- `_MOUSE`: single-tier ocean green `(0x10, 0xC0, 0x80)` on the cursor keys
  (MS_LEFT/DOWN/UP/RGHT at LEDs 50–53) and the right-thumb click cluster
  (MS_BTN1/BTN3/BTN2 at LEDs 37–39). No gradient — the layer is mostly
  transparent and these are the only bound keys.
- `_NAV`: three tiers of purple on the right hand. Primary arrows + page-nav
  (LEFT/DOWN/UP/RGHT, HOME/PGDN/PGUP/END) `(0xA0, 0x60, 0xFF)` light/lavender
  (62% sat); secondary edit row + TD caps key (REDO/PASTE/COPY/CUT/UNDO,
  TD_CAPS_WORD_LOCK) `(0x20, 0x00, 0x40)` mid pure purple (100% sat);
  tertiary INSERT `(0x06, 0x00, 0x10)` barely-on pure purple. KC_MCTL painted
  blue `(0x60, 0xB0, 0xFF)` separately so Mission Control reads as a distinct
  system key, not a fourth gradient step. The TD caps key's secondary purple
  is overridden red/blue when caps lock or caps word is on, since that
  indicator paints later. The **left** hand carries the `APP_*` launcher keys,
  each painted in its app's brand color (see the App launchers section): VS Code
  blue (15), Firefox orange (20), Terminal green (25), Simulator grey (22).
- `_FUN`: three tiers of red, all pure (G=B=0). Only R varies — primary F1-F9
  `(0xFF, 0, 0)` full, secondary F10-F12 `(0x18, 0, 0)` ~9%, tertiary system
  keys (PrtSc/ScrLk/Pause/App) `(0x06, 0, 0)` ~2% (barely-on glow). Any non-zero G or B drifts the hue
  toward pink or orange on this hardware, even when G==B — so we keep the
  whole layer monochromatic and use brightness alone to tier the keys.

Border keys are barely-on by design — they read as a quiet frame rather than
competing with the primary cluster for attention. Keeps numbers
visually distinct from the operator border so muscle memory builds faster.

**CG-swap toggle flash** — when the Control/GUI swap flips, every LED blinks
**3×**: **white** when entering swapped (Mac) mode, **green** when leaving it.
Implemented at the top of `rgb_matrix_indicators_advanced_user` by
edge-detecting `synced_cg_swap` (not by hooking the keypress), so **both halves
flash in sync** — master updates `synced_cg_swap` locally, slave gets it via the
`USER_OS_SYNC` RPC, and each half's indicator independently sees the same edge.
While a flash is active the function paints all LEDs in `[led_min, led_max)` and
returns early, so the flash owns the whole frame (~720 ms: 3 × 120 ms on/off).
**Brownout — confirmed, and it's total board current, not brightness.** White
lights all 3 channels, so flashing the whole board browns out the rail and kills
the TFT until the next clean boot. Reproducible: green (one channel) survives;
white (three channels) takes the screen down — and capping white brightness
alone (`0x50` across the whole board) still died. The count of lit LEDs is what
matters. Three mitigations, in order of importance:

- **Only the top row flashes** — 12 per-key LEDs (6/half: left 25-30, right
  56-61, the lowest-`y` LEDs in `g_led_config`) instead of ~31/half. This is the
  real fix; it drops peak draw to ~20% of the whole-board flash. Off-phase
  paints the row black against the live effect so it still reads as a blink,
  while the rest of the board keeps animating. The flash still returns `false`,
  so other indicators are skipped for its ~720 ms.
- `CG_FLASH_WHITE_LEVEL` (`0x50`) keeps each white LED's draw under the
  proven-safe green (`3 * 0x50 = 0xF0 < 0xFF`). Secondary margin. Green stays
  full `0xFF`.
- `CG_FLASH_BOOT_GRACE_MS` (3 s) suppresses flashes right after power-on so
  nothing blinks while the TFT is still coming up.

If you make the flash brighter or wider, watch total draw (level × lit LEDs) —
overshoot and the TFT brownout returns.

This **replaced an earlier OS-detection red warning** (host is macOS but swap is
off). That warning quietly stopped working: `KEYBOARD_SHARED_EP = yes` (added
with the Globe/emoji key) restructures the USB descriptors, and QMK's OS
detection fingerprints the host from the descriptor-request pattern during
enumeration — so the shared endpoint made `detected_host_os()` confidently
mis-report macOS as Windows/Linux. The swap flash doesn't depend on OS detection
at all, sidestepping the whole problem. `synced_host_os` is now vestigial (still
synced, no longer read) — left in place in case detection is revisited.

**Caps state** — split across two cues, painted last so they override held-mod
tint on shared LEDs (`;` is RGUI and shares LED 54):

- **Mode**, on both Shift home-row mods (`LED_HRM_SFT_L = 20` / F,
  `LED_HRM_SFT_R = 51` / J): red = caps lock, blue = caps word. Shift is the
  intuitive "caps is on" cue and reads clearly on the display-less build.
- **Exit beacon**, on the TD key (`LED_CAPS_WORD_LOCK = 54`, matrix `[6][5]` =
  `R11`, where `TD(TD_CAPS_WORD_LOCK)` lives on `_NAV`): pulsates **white** for
  **caps lock only** — it's the key you press to get back out. Caps word
  self-exits after a word, so it gets no beacon. Triangle-wave breathe off
  `timer_read32()`, period `CAPS_PULSE_PERIOD_MS` (1600 ms).

OS detection needs:
- `OS_DETECTION_ENABLE = yes` in rules.mk
- `#define OS_DETECTION_SINGLE_REPORT` in config.h — ARM Macs cause repeated
  re-detection that never settles inside the debounce window without this.
- `#define OS_DETECTION_DEBOUNCE 500`

**Caveat — detection is unreliable on this build.** `KEYBOARD_SHARED_EP = yes`
(needed for the Globe key) changes the USB descriptors enough that
`detected_host_os()` mis-fingerprints macOS as Windows/Linux. That's why nothing
keys off `synced_host_os` anymore (see the CG-swap toggle flash above). If you
want OS-aware behavior back, you'd have to either drop the shared endpoint or
re-tune detection against this descriptor set — don't assume `detected_host_os()`
is correct here.

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

## Trackpad: flick gestures on `_NAV`

Also in `pointing_device_task_combined_user` (the `IS_LAYER_ON(_NAV)` branch):
a quick directional swipe on the trackpad fires a macOS Mission Control /
Spaces action.

| Flick | Action | Keys sent |
|-------|--------|-----------|
| Up    | Mission Control   | `Ctrl+Up`    |
| Down  | dismiss it        | `Esc`        |
| Left  | next Space        | `Ctrl+Right` |
| Right | previous Space    | `Ctrl+Left`  |

- Hand-rolled stroke recognizer: accumulate trackpad x/y deltas while the
  finger moves; when the dominant-axis magnitude crosses
  `NAV_FLICK_THRESHOLD` (80, tunable) fire once, then lock (`nav_flick_fired`)
  until motion idles for `NAV_FLICK_IDLE_MS` (150) — finger lift = stroke end.
  One swipe = one action; no auto-repeat.
- Ctrl combos go through **`tap_code16(LCTL(...))` deliberately** so they
  bypass the CG_TOGG swap and reach the host as real Ctrl. Mission Control and
  Spaces are Ctrl shortcuts on macOS, not Cmd — routing through the swap would
  turn `Ctrl+Up` into `Cmd+Up` (= "open enclosing folder"). Same reasoning as
  the `tap_code16` gotcha at the bottom of this file.
- x/y are zeroed afterward (like the `_MOUSE` drag-scroll branch) so a flick
  never moves the cursor or wakes `_AUTO_MOUSE`.
- Not gated on `macos_mode()` — the user typically runs mac mode and the
  Windows fallbacks (`Esc`, `Ctrl+arrows`) are low-harm. Add a `macos_mode()`
  gate if that changes.
- y is positive *downward* (matches the drag-scroll convention above): flick
  up = negative accumulated y.

## Window management (`_WINDOW` → Rectangle)

Held via the left thumb (`LT(_WINDOW, KC_ESC)`), so both hands are free. Sends
macOS [Rectangle](https://rectangleapp.com/) shortcuts. Layout:

- **Left hand = sixths** (⅓ width × ½ height), a 2×3 grid mirroring the screen:
  `W E R` = top L/C/R sixth, `X C V` = bottom L/C/R. **Center** window on `D`.
- **Right hand**: halves on the home-row arrows (`H J K L` = left/bottom/top/right
  half), thirds across the top row (`U I O P` = first-⅓ / first-⅔ / last-⅔ /
  last-⅓), **Maximize** on `Y`.
- **Right encoder**: Make Larger / Smaller.

**CG_TOGG swap bypass — the important footgun.** Rectangle listens on `Ctrl+Opt`,
but the user runs mac mode (`swap_lctl_lgui` on), under which QMK's `mod_config`
rewrites a keymap-array `LCTL(...)` to `LGUI` *before sending* — turning
`Ctrl+Opt+←` into `Cmd+Opt+←` (a Firefox tab switch, not a window snap). So every
action is a **custom keycode** (`WM_*`) handled in `process_record_user` via
`tap_code16`, which emits the literal mods and skips the swap. Same reasoning as
the `_NAV` flick gestures and the `tap_code16` gotcha at the bottom of this file.
Do **not** move these into the keymap array.

**Rectangle shortcut map.** Halves/thirds/maximize/center/resize use Rectangle's
stock defaults. The **sixths ship with no default shortcut**, so they must be
assigned in Rectangle → Settings to match what the firmware sends:

| Action | Firmware sends | Stock default? |
|--------|---------------|----------------|
| Left / Right / Top / Bottom Half | `⌃⌥` ← / → / ↑ / ↓ | yes |
| First Third / Last Third | `⌃⌥` D / G | yes |
| First Two-Thirds / Last Two-Thirds | `⌃⌥` E / T | yes |
| Maximize | `⌃⌥↵` | yes |
| Center | `⌃⌥C` | yes |
| Make Larger / Smaller (encoder) | `⌃⌥=` / `⌃⌥-` | yes |
| Top sixths L / C / R | `⌃⌥⇧` U / I / O | **no — assign** |
| Bottom sixths L / C / R | `⌃⌥⇧` J / K / L | **no — assign** |

The six `⌃⌥⇧` combos are arbitrary — chosen to mirror Rectangle's quarter keys
(U/I/J/K) plus Shift, and to avoid colliding with its defaults. If you rebind a
sixth in Rectangle, update the matching `WM_6*` `tap_code16` in keymap.c.

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
| Left  (idx 0) | `_ADJUST` | `rgb_matrix_increase_hue` / `_decrease_hue` |
| Left  (idx 0) | other | `Cmd+Z` / `Cmd+Shift+Z` (undo/redo) |
| Right (idx 2) | `_WINDOW` | window resize — Rectangle Larger/Smaller (`⌃⌥=` / `⌃⌥-`) |
| Right (idx 2) | `_NAV` | tab/editor switch — `⌘⌥→` / `⌘⌥←` (in-order in Firefox, Safari & VS Code) via `tap_code16` (bypasses CG swap) |
| Right (idx 2) | `_ADJUST` | `rgb_matrix_increase_val` / `_decrease_val` |
| Right (idx 2) | other | scroll 5 lines (`Up×5` / `Down×5`) |

Slots 1 and 3 are the inactive Halcyon-module encoder positions (we only have
the 2 soldered).

**Resolution.** The soldered encoders are 4 pulses/detent, but the board's
keyboard.json defaults `ENCODER_RESOLUTION` to 2, which fires `encoder_update_user`
**twice per detent** (skips a tab, double-scrolls, etc.). `config.h` overrides it
to **4** (`#undef` + `#define`) for all builds — both boards have identical
encoders. If you ever see one-detent-fires-twice again, this is the knob.

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

## Raw HID → macOS companion app

`RAW_ENABLE = yes`. The keymap pushes a compact state packet to a host app
(`~/Development/keyboardstuff/companion-app/`, a Swift menu-bar agent) which
shows an on-screen cheat-sheet of the active layer — hold `_NUM` and the numpad
+ adjacent symbols pop up, etc.

- `hlc_hid_send_state()` builds a 32-byte report: `[0]=0xAB magic`,
  `[1]=0x10 (state) / 0x01 (request)`, `[2]=highest layer`, `[3]=mac mode`,
  `[4]=caps word`, `[5]=caps lock`. **Master-only** (`is_keyboard_master()`
  guard) — raw HID only exists on the half with USB; slave's `raw_hid_send` is
  a no-op.
- Sent from `layer_state_set_user` (on every layer change) and from
  `raw_hid_receive` (the app sends a `0x01` request at launch so it can sync
  before the next layer change).
- **`RAW_EPSIZE` is not pulled in by `raw_hid.h`** (it lives in
  `tmk_core/protocol/usb_descriptor.h`). We use a local `HLC_HID_REPORT_SIZE 32`
  to avoid depending on that header — `raw_hid_send` still requires the full
  endpoint-sized buffer.
- Mac VID/PID for matching: `0x8D1D` / `0x7FCE`, vendor usage page `0xFF60`
  usage `0x61` (no Input Monitoring permission needed).
- The app's layer art (`companion-app/Sources/LayerPeek/Layers.swift`) is a
  hand-transcription of the keymaps here — **the one thing that can drift**.
  Update it when you re-map a layer.
- `RAW_ENABLE` restructures the USB descriptors, so **re-flash both halves**.

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
