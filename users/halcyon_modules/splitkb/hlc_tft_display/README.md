# `hlc_tft_display` — internal reference

Notes-to-self about how this module wires into QMK and how the default demo works,
so I can rewrite the display contents from scratch without re-deriving everything.

## Hardware

- Panel: ST7789, **135 × 240** portrait, 16-bit RGB565, SPI mode 3.
- Pins (RP2040 GPIO, defined in `config.h`):
  - `LCD_CS_PIN = GP13`, `LCD_DC_PIN = GP16`, `LCD_RST_PIN = GP26`
  - `BACKLIGHT_PIN = GP27` (PWM driver `PWMD5`, channel B, 10 levels — see `splitkb/config.h`)
- Viewport offsets: `LCD_OFFSET_X = 52`, `LCD_OFFSET_Y = 40` — applied via
  `qp_set_viewport_offsets`. The ST7789 driver in QP would normally apply these
  automatically; here `ST7789_NO_AUTOMATIC_VIEWPORT_OFFSETS` is set so the module
  controls them.
- One module instance per build: `ST7789_NUM_DEVICES = 1`, `SURFACE_NUM_DEVICES = 1`.

## Build integration

Triggered by `-e HLC_TFT_DISPLAY=1`. From there:

- `users/halcyon_modules/splitkb/rules.mk` includes
  `hlc_tft_display/rules.mk`, which adds `hlc_tft_display.c`, the two
  font `.qff.c` files (Retron2000 27 and underline variant), and the 0–9 + `undef`
  number glyph `.qgf.c` files into the build.
- `users/halcyon_modules/splitkb/rules.mk` always enables
  `QUANTUM_PAINTER_ENABLE = yes` with drivers `st7789_spi surface`, so the QP
  framework is always compiled in even on non-TFT builds.
- `POST_CONFIG_H` brings in this module's `config.h` after the keymap's.

`graphics/fonts/` holds Quantum Painter font binaries (`.qff`). `graphics/numbers/`
holds glyph images (`.qgf`). Both formats are produced by `qmk painter-convert-graphics`
and `qmk painter-convert-font`; the generated `.c` files are checked in.

## Lifecycle hooks

Everything is plumbed through `splitkb/halcyon.c`, which provides weak `_kb` hooks
and weak `_user` hooks. The TFT module overrides three `_kb` hooks:

| Hook | When | Purpose in TFT module |
|------|------|-----------------------|
| `module_post_init_kb()` | Called from `keyboard_post_init_kb` after split init | Enables backlight, creates `lcd` (SPI device) + `lcd_surface` (RGB565 framebuffer), inits QP, clears, powers on. Then calls `module_post_init_user()`. |
| `display_module_housekeeping_task_kb(bool second_display)` | Every iteration of `housekeeping_task_kb`, called once on master with `false`, and once on slave with `true` iff the master detected the slave is also a TFT display | Runs the per-frame draw. Calls `display_module_housekeeping_task_user` first. Always finishes with `qp_surface_draw(lcd_surface, lcd, 0, 0, 0)` + `qp_flush(lcd)`. |
| `module_suspend_power_down_kb()` / `module_suspend_wakeup_init_kb()` | USB suspend/resume | `qp_power(lcd, false/true)`. |

The `_user` variants of all three (`module_post_init_user`,
`module_housekeeping_task_user`, `display_module_housekeeping_task_user`,
`module_suspend_power_down_user`, `module_suspend_wakeup_init_user`) are weak no-ops
that the **keymap** can override to inject behavior without forking the module.

### Master vs second display

`housekeeping_task_kb` in `halcyon.c` does:

```c
if (is_keyboard_master()) {
    display_module_housekeeping_task_kb(false);  // master is never the second
} else {
    display_module_housekeeping_task_kb(module_master == hlc_tft_display);
}
```

The `second_display` arg flips the branch inside the module:
- `false` (master) → runs `update_display()` (layer indicator + caps/num/scroll lock).
- `true` (slave with TFT, master is also TFT) → runs Conway's Game of Life animation.

`module_master` gets populated via the `MODULE_SYNC` split RPC registered in
`keyboard_post_init_kb`; the master sends its `module` enum value to the slave once
the transport is connected.

## Drawing model: surface + flush

The module uses **double-buffering through a QP surface**, not direct draws:

1. `lcd` is the actual ST7789 device.
2. `lcd_surface` is a `qp_make_rgb565_surface` backed by `lcd_surface_fb` — a
   `SURFACE_REQUIRED_BUFFER_BYTE_SIZE(135, 240, 16)` static byte array
   (~64.8 KB — sizeable on RP2040 but fits).
3. All drawing in `update_display()` / `draw_grid()` targets `lcd_surface`.
4. End of every housekeeping tick: `qp_surface_draw(lcd_surface, lcd, 0, 0, 0)`
   copies the surface to the device, then `qp_flush(lcd)` pushes pixels over SPI.

Surface-only draws are cheap; the SPI flush is the expensive part, so partial
redraws on the surface are fine.

## Backlight

- PWM-driven via QMK's `backlight` feature (`BACKLIGHT_PIN = GP27`, PWMD5/Ch B).
- `HLC_BACKLIGHT_TIMEOUT = 120000` ms (2 min) — defined in `splitkb/config.h`.
- `halcyon.c` polls `last_input_activity_elapsed()` each housekeeping tick:
  on idle past the timeout → `backlight_suspend()`; on activity → `backlight_wakeup()`.
- `QUANTUM_PAINTER_DISPLAY_TIMEOUT = HLC_BACKLIGHT_TIMEOUT` — QP also blanks the
  display after the same interval (separate code path).

## Default demo internals (what `update_display` and `draw_grid` actually do)

Master side (`update_display`):
- Lazy-loads the two fonts on first call.
- Diffed redraws keyed on `host_keyboard_led_state().raw` and `layer_state`.
- Layer indicator: top-left 5,5 corner draws one of `gfx_0..gfx_7` or `gfx_undef`,
  recolored to a per-layer HSV tone (see `HSV_LAYER_*` macros in `hlc_tft_display.h`).
- Lock indicators: bottom-left "Caps" / "Num" / "Scroll" lines, drawn with the
  underlined font variant when active.

Slave side (`draw_grid` + `update_grid`):
- 27×48 grid of `CELL_SIZE = 4` px cells with `OUTLINE_SIZE = 1` (fits 135×240 with margins).
- `init_grid` seeds 20% live cells; `update_grid` runs Conway's rules and tracks a
  `changed_grid` dirty-cell mask so `draw_grid` only redraws cells that changed.
- Throttled to ~10 fps (`timer_elapsed32(last_draw) >= 100`).
- Color cycles through `HSV_LAYER_*` based on a `color_value` int; bumped + new
  3×3 cluster added whenever `last_matrix_activity_time()` changes (i.e. user typed).
- Seeded RNG via `get_random_32bit()` which reads `rosc_hw->randombit` 32 times
  (RP2040 ring oscillator entropy).

## Exposed symbols (`hlc_tft_display.h`)

```c
extern painter_device_t lcd;          // ST7789 SPI device
extern painter_device_t lcd_surface;  // RGB565 framebuffer surface

void draw_grid(void);
void update_grid(void);
void init_grid(void);
void add_cell_cluster(void);
uint8_t get_random_color_index(void);
void update_display(void);
void backlight_wakeup(void);
void backlight_suspend(void);
```

Plus the `HSV_*` color macros (8 layer colors, lock-state on/off, brand color).

## Starting from scratch — where to hook in

Easiest paths, in order of "least invasive":

1. **Override user hooks from the keymap** (no module changes). In keymap.c:
   ```c
   bool display_module_housekeeping_task_user(bool second_display) {
       // returning false aborts the _kb side, so return true to let it continue
       // draw to `lcd_surface` here (extern from hlc_tft_display.h)
       return true;
   }
   ```
   The module's `_kb` body still runs after this (it calls `_user` first then draws),
   so we'd be drawing on top of the demo. Useful only for adding overlays.

2. **Replace the module's draw entirely** — edit `hlc_tft_display.c`:
   - Gut `update_display()` (master draw) and/or the second-display branch.
   - Reuse `lcd_surface` and the per-frame flush pattern (don't write to `lcd` directly
     unless you have a reason — losing the surface means losing diff-redraw and
     pulling SPI on every shape).
   - Remove unused font/image includes and trim `rules.mk` to save flash.

3. **Build a new module from scratch** — copy this folder to a new name, add a new
   `HLC_*` flag to `splitkb/rules.mk` and `halcyon.c`, register in `halcyon.h`'s
   `module_t` enum. Heavy lift; only do this if we want to keep `hlc_tft_display`
   around as-is for other builds.

For "just play with what's on screen," approach **2** is the right move.

## Quantum Painter cheat-sheet (what's already used in this module)

```c
// Devices
qp_st7789_make_spi_device(w, h, cs, dc, rst, divisor, mode);
qp_make_rgb565_surface(w, h, framebuffer_byte_array);
qp_init(device, rotation);                // QP_ROTATION_0|90|180|270
qp_set_viewport_offsets(device, x, y);
qp_power(device, on);
qp_clear(device);
qp_flush(device);
qp_surface_draw(surface, target_device, x, y, entire_surface);

// Primitives — HSV: hue 0-255, sat 0-255, val 0-255
qp_rect(device, l, t, r, b, h, s, v, filled);
qp_drawimage_recolor(device, x, y, image_handle, hsv_fg, hsv_bg);
qp_drawtext_recolor(device, x, y, font_handle, "str", hsv_fg, hsv_bg);

// Resources
qp_load_font_mem(font_Retron2000_27);     // returns painter_font_handle_t
qp_load_image_mem(gfx_0);                 // returns painter_image_handle_t
qp_close_image(handle);                   // pair with each load
```

The font handle exposes `->line_height` (used to stack the lock-state lines).

## Gotchas

- The surface framebuffer is **65 KB** — keep that in mind for RP2040 RAM budget.
- `qp_load_image_mem` allocates; `update_display()` calls `qp_close_image` after each
  layer draw to avoid leaking, but `qp_load_font_mem` is only called once (lazy `first_run`).
  If we rewrite the draw, preserve that pattern.
- The Halcyon kb hooks return `bool`. Returning `false` from the `_kb` body
  *prevents the rest of the housekeeping pipeline from running on that side this tick* —
  not just the draw flush. Look at `housekeeping_task_kb` in `halcyon.c` if confused.
- `is_keyboard_master()` decides who runs `update_display` vs. the grid; if the user
  flips USB sides, both behaviors swap automatically (assuming both halves have TFT —
  the slave branch only runs when `module_master == hlc_tft_display`, which is what
  the split RPC sync ensures).
- Editing this module's `config.h`, `.c`, or `.h` affects every keymap in this userspace
  that uses `HLC_TFT_DISPLAY=1`. If we want per-keymap behavior, keep changes inside
  the keymap via the `_user` hooks.
