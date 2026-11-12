BLUETOOTH_ENABLE 		= yes
BLUETOOTH_DRIVER		= iton_bt
LTO_ENABLE              = yes
# SonixQMK switched SN32F240B boards to -Os by default (62973ee48a).
# Keep -O2: the BT and RGB timing of this firmware was tuned and tested with it.
OPT                     = 2
DEFERRED_EEPROM_SAVE	= yes

DEFERRED_EEPROM_DELAY	= 10000

ifeq ($(strip $(DEFERRED_EEPROM_SAVE)), yes)
    OPT_DEFS += -DDEFERRED_EEPROM_SAVE
    OPT_DEFS += -DEEPROM_FLUSH_DELAY_MS=$(strip $(DEFERRED_EEPROM_DELAY))
endif
