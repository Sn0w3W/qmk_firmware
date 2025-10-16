/*
Copyright 2024 mintyleaf

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

enum custom_keycodes {
    KC_LOPTN = QK_KB_0,
    KC_ROPTN,
    KC_LCMMD,
    KC_RCMMD,
    KC_MISC,
    KC_LAUN,
    KC_MSCR,
    KC_TASK,
    KC_FLXP,
    KC_SNIP,
    BT_PROFILE1,
    BT_PROFILE2,
    BT_PROFILE3,
    BT_BATTERY,
    BT_PAIR,
    BT_RESET
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

#define BT_PRF1 BT_PROFILE1          // Bluetooth Profile 1
#define BT_PRF2 BT_PROFILE2          // Bluetooth Profile 2
#define BT_PRF3 BT_PROFILE3          // Bluetooth Profile 3
#define BT_BATT BT_BATTERY           // Bluetooth Battery Level
#define BT_RESS BT_RESET             // Bluetooth Reset
