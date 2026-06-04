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

