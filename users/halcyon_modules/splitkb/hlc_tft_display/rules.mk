SRC += $(USER_PATH)/splitkb/hlc_tft_display/hlc_tft_display.c
POST_CONFIG_H += $(USER_PATH)/splitkb/hlc_tft_display/config.h

# Layer icons
SRC += $(USER_PATH)/splitkb/hlc_tft_display/graphics/icons/splitkb.qgf.c \
       $(USER_PATH)/splitkb/hlc_tft_display/graphics/icons/open_with.qgf.c \
       $(USER_PATH)/splitkb/hlc_tft_display/graphics/icons/mouse.qgf.c \
       $(USER_PATH)/splitkb/hlc_tft_display/graphics/icons/music_note.qgf.c \
       $(USER_PATH)/splitkb/hlc_tft_display/graphics/icons/calculate.qgf.c \
       $(USER_PATH)/splitkb/hlc_tft_display/graphics/icons/data_object.qgf.c \
       $(USER_PATH)/splitkb/hlc_tft_display/graphics/icons/functions.qgf.c \
       $(USER_PATH)/splitkb/hlc_tft_display/graphics/icons/settings.qgf.c \
       $(USER_PATH)/splitkb/hlc_tft_display/graphics/icons/trackpad_input.qgf.c

# Modifier-key icons
SRC += $(USER_PATH)/splitkb/hlc_tft_display/graphics/icons/keyboard_control_key.qgf.c \
       $(USER_PATH)/splitkb/hlc_tft_display/graphics/icons/keyboard_option_key.qgf.c \
       $(USER_PATH)/splitkb/hlc_tft_display/graphics/icons/keyboard_command_key.qgf.c \
       $(USER_PATH)/splitkb/hlc_tft_display/graphics/icons/shift.qgf.c \
       $(USER_PATH)/splitkb/hlc_tft_display/graphics/icons/diamond_mod.qgf.c

# Lock-state icons (caps_word, caps_lock) + CG_TOGG state (apple/diamond)
SRC += $(USER_PATH)/splitkb/hlc_tft_display/graphics/icons/uppercase.qgf.c \
       $(USER_PATH)/splitkb/hlc_tft_display/graphics/icons/keyboard_capslock.qgf.c \
       $(USER_PATH)/splitkb/hlc_tft_display/graphics/icons/apple_logo.qgf.c \
       $(USER_PATH)/splitkb/hlc_tft_display/graphics/icons/diamond.qgf.c
