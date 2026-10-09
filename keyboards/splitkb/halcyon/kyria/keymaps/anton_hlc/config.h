#pragma once

#define POINTING_DEVICE_AUTO_MOUSE_ENABLE
#define AUTO_MOUSE_DEFAULT_LAYER 8  // _AUTO_MOUSE

// Hi-res scroll: declared in HID descriptor; honored fully by Linux/Wayland and
// partially by Windows. macOS often ignores the Resolution Multiplier feature on
// generic HID mice, so we still need the divisor-based throttle below.
#define POINTING_DEVICE_HIRES_SCROLL_ENABLE
// Allow 16-bit wheel values so any large accumulated tick doesn't clip at ±127.
#define WHEEL_EXTENDED_REPORT

// OS detection: ARM Macs cause repeated re-detection that never settles inside
// the debounce window. Lock in the first stable result.
#define OS_DETECTION_SINGLE_REPORT
#define OS_DETECTION_DEBOUNCE 500

// Split sync of OS detection (master is the only side that sees USB traffic).
#define SPLIT_TRANSACTION_IDS_USER USER_OS_SYNC

// Encoder resolution = quadrature pulses per detent. This DIFFERS PER BOARD:
// Fancy's soldered encoders are 4 pulses/detent, Blackout's are 2. Get it wrong
// and one detent either fires twice (resolution too low — skips a browser tab on
// _NAV, double-scrolls, double-undos) or every other detent does nothing at all
// (resolution too high). There is no ENCODER_RESOLUTIONS array in this build, so
// this single scalar covers all 4 encoder slots — fine, because both halves of a
// given board use the same encoders. The per-board value is passed in from
// qmk.json as ENC_PPD (see rules.mk); it defaults to 4 for a bare compile.
#undef ENCODER_RESOLUTION
#define ENCODER_RESOLUTION ENC_PPD

