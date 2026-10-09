ENCODER_MAP_ENABLE   = no
TAP_DANCE_ENABLE     = yes
CAPS_WORD_ENABLE     = yes
OS_DETECTION_ENABLE  = yes
# Raw HID: push the active layer (+ mac/caps state) to the macOS companion app
# (companion-app/) so it can show a contextual layer cheat-sheet on screen.
RAW_ENABLE           = yes
# Needed so the Apple Globe key (Consumer page) can be held as a modifier
# for chords like Globe+E, Globe+F, etc.
KEYBOARD_SHARED_EP   = yes

# Encoder pulses-per-detent, passed per build target from qmk.json (see config.h).
# Blackout's encoders are 2, Fancy's are 4. Default 4 so a bare `qmk compile`
# without -e still builds; every target in qmk.json sets it explicitly.
ENC_PPD ?= 4
OPT_DEFS += -DENC_PPD=$(ENC_PPD)

# This adds module functionality to your keyboard (files found in users/halcyon_modules)
USER_NAME := halcyon_modules
