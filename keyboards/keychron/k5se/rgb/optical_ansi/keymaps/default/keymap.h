/*
Copyright 2025 Sn0w3W

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include QMK_KEYBOARD_H

enum BT_keycodes {
    BT_PROFILE1 = SAFE_RANGE,
    BT_PROFILE2,
    BT_PROFILE3,
    BT_PAIR,
    BT_TOGGLE,
    BT_RESET,
    BT_BATTERY
};

// Each layer gets a name for readability, which is then used in the keymap matrix below.
// The underscores don't mean anything - you can have a layer called STUFF or any other name.
// Layer names don't all need to be of the same length, obviously, and you can also skip them
// entirely and just use numbers.
enum layer_names {
  MAC_BASE,
  MAC_FN,
  WIN_BASE,
  WIN_FN
};

#define KC_MCTL KC_MISSION_CONTROL  // Mission Control
#define KC_LPAD KC_LAUNCHPAD        // Launchpad
#define KC_MSNP SGUI(KC_5)          // Mac screenshot

#define KC_TASK LGUI(KC_TAB)        // Task viewer
#define KC_FLXP LGUI(KC_E)          // Windows file explorer
#define KC_SNIP LGUI(LSFT(KC_S))    // Windows snip tool
