/*
Copyright 2020 Dimitris Mantzouranis
Copyright 2022 Pablo Ramirez

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
#include "keymap.h"

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

/*  Mac base layout
    +------------------------------------------------------------------------------+
    | ESC|BRID|BRIU|MCTL|LPAD|RMVD|RMVD|MPRV|MPLY|MNXT|MUTE|VOLD|VOLU|MSNP|DEL|RMTG|
    +------------------------------------------------------------------------------+
    |  ~ |  1 |  2 |  3 |  4 |  5 |  6 |  7 |  8 |  9 |  0 |  - |  = | BACKSP |PGUP|
    +------------------------------------------------------------------------------+
    |  TAB |  Q |  W |  E |  R |  T |  Y |  U |  I |  O |  P |  [ |  ] |      |PGDN|
    +-------------------------------------------------------------------  RET -----+
    |CAPSLCK|  A |  S |  D |  F |  G |  H |  J |  K |  L |  ; |  ' |  # |     |HOME|
    +------------------------------------------------------------------------------+
    |LSHFT|  \ |  Z |  X |  C |  V |  B |  N |  M |  , |  . |  / | RSHIFT| UP | END|
    +------------------------------------------------------------------------------+
    |LCTRL| LGUI| LALT|            SPACE            | RALT| FN |RCTL|LEFT|DOWN|RGHT|
    +------------------------------------------------------------------------------+
*/
  [MAC_BASE] = LAYOUT_75_iso(
  /*  0          1          2          3        4        5        6         7        8        9          10          11          12         13         14         15        */
      KC_ESC,    KC_BRID,   KC_BRIU,   KC_MISC, KC_LAUN, RM_VALD, RM_VALU,  KC_MPRV, KC_MPLY, KC_MNXT,   KC_MUTE,    KC_VOLD,    KC_VOLU,   KC_MSCR,   KC_DEL,    RM_TOGG   ,
      KC_GRV,    KC_1,      KC_2,      KC_3,    KC_4,    KC_5,    KC_6,     KC_7,    KC_8,    KC_9,      KC_0,       KC_MINS,    KC_EQL,    KC_BSPC,              KC_PGUP   ,
      KC_TAB,    KC_Q,      KC_W,      KC_E,    KC_R,    KC_T,    KC_Y,     KC_U,    KC_I,    KC_O,      KC_P,       KC_LBRC,    KC_RBRC,                         KC_PGDN   ,
      KC_CAPS,   KC_A,      KC_S,      KC_D,    KC_F,    KC_G,    KC_H,     KC_J,    KC_K,    KC_L,      KC_SCLN,    KC_QUOT,    KC_NUHS,   KC_ENT,               KC_HOME   ,
      KC_LSFT,   KC_NUBS,   KC_Z,      KC_X,    KC_C,    KC_V,    KC_B,     KC_N,    KC_M,    KC_COMM,   KC_DOT,     KC_SLSH,               KC_RSFT,   KC_UP,     KC_END    ,
      KC_LCTL,   KC_LOPTN,  KC_LCMMD,                             KC_SPC,                                KC_RCMMD,   MO(MAC_FN), KC_RCTL,   KC_LEFT,   KC_DOWN,   KC_RGHT
  ),

/*  Mac Fn overlay
    +------------------------------------------------------------------------------+
    |BOOT| F1 | F2 | F3 | F4 | F5 | F6 | F7 | F8 | F9 | F10| F11| F12|    |   |RMNX|
    +------------------------------------------------------------------------------+
    |    |    |    |    |    |    |    |    |    |    |    |    |    |        |    |
    +------------------------------------------------------------------------------+
    |      |    |    |    |    |    |    |    |    |    |    |    |    |      |    |
    +-------------------------------------------------------------------      -----+
    |       |    |    |    |    |    |    |    |    |    |    |    |    |     |    |
    +------------------------------------------------------------------------------+
    |     |    |    |    |    |    |    |    |    |    |    |    |       |RMSU|    |
    +------------------------------------------------------------------------------+
    |     |     |     |                             |     |    |    |RMHD|RMSD|RMHU|
    +------------------------------------------------------------------------------+
*/
  [MAC_FN] = LAYOUT_75_iso(
  /*  0           1           2           3           4           5           6           7           8           9           10          11          12          13          14          15         */
      QK_BOOT,    KC_F1,      KC_F2,      KC_F3,      KC_F4,      KC_F5,      KC_F6,      KC_F7,      KC_F8,      KC_F9,      KC_F10,     KC_F11,     KC_F12,     _______,    _______,    RM_NEXT    ,
      _______,    BT_PRF1,    BT_PRF2,    BT_PRF3,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,                _______    ,
      _______,    _______,    _______,    _______,    BT_RESS,    _______,    _______,    _______,    _______,    _______,    BT_PAIR,    _______,    _______,                            _______    ,
      _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,                _______    ,
      _______,    _______,    _______,    _______,    _______,    _______,    BT_BATT,    _______,    _______,    _______,    _______,    _______,                _______,    RM_SATU,    _______    ,
      _______,    _______,    _______,                                        _______,                                        _______,    _______,    _______,    RM_HUED,    RM_SATD,    RM_HUEU
  ),

/*  Windows base layout
    +------------------------------------------------------------------------------+
    | ESC| F1 | F2 | F3 | F4 | F5 | F6 | F7 | F8 | F9 | F10| F11| F12|PSCR|DEL|RMTG|
    +------------------------------------------------------------------------------+
    |  ~ |  1 |  2 |  3 |  4 |  5 |  6 |  7 |  8 |  9 |  0 |  - |  = | BACKSP |PGUP|
    +------------------------------------------------------------------------------+
    |  TAB |  Q |  W |  E |  R |  T |  Y |  U |  I |  O |  P |  [ |  ] |      |PGDN|
    +-------------------------------------------------------------------  RET -----+
    |CAPSLCK|  A |  S |  D |  F |  G |  H |  J |  K |  L |  ; |  ' |  # |     |HOME|
    +------------------------------------------------------------------------------+
    |LSHFT|  \ |  Z |  X |  C |  V |  B |  N |  M |  , |  . |  / | RSHIFT| UP | END|
    +------------------------------------------------------------------------------+
    |LCTRL| LGUI| LALT|            SPACE            | RALT| FN |RCTL|LEFT|DOWN|RGHT|
    +------------------------------------------------------------------------------+
*/
  [WIN_BASE] = LAYOUT_75_iso(
  /*  0          1          2          3        4        5        6         7        8        9          10          11          12         13         14         15        */
      KC_ESC,    KC_F1,     KC_F2,     KC_F3,   KC_F4,   KC_F5,   KC_F6,    KC_F7,   KC_F8,   KC_F9,     KC_F10,     KC_F11,     KC_F12,    KC_PSCR,   KC_DEL,    RM_TOGG   ,
      KC_GRV,    KC_1,      KC_2,      KC_3,    KC_4,    KC_5,    KC_6,     KC_7,    KC_8,    KC_9,      KC_0,       KC_MINS,    KC_EQL,    KC_BSPC,              KC_PGUP   ,
      KC_TAB,    KC_Q,      KC_W,      KC_E,    KC_R,    KC_T,    KC_Y,     KC_U,    KC_I,    KC_O,      KC_P,       KC_LBRC,    KC_RBRC,                         KC_PGDN   ,
      KC_CAPS,   KC_A,      KC_S,      KC_D,    KC_F,    KC_G,    KC_H,     KC_J,    KC_K,    KC_L,      KC_SCLN,    KC_QUOT,    KC_NUHS,   KC_ENT,               KC_HOME   ,
      KC_LSFT,   KC_NUBS,   KC_Z,      KC_X,    KC_C,    KC_V,    KC_B,     KC_N,    KC_M,    KC_COMM,   KC_DOT,     KC_SLSH,               KC_RSFT,   KC_UP,     KC_END    ,
      KC_LCTL,   KC_LGUI,   KC_LALT,                              KC_SPC,                                KC_RALT,    MO(WIN_FN), KC_RCTL,   KC_LEFT,   KC_DOWN,   KC_RGHT
  ),

/*  Windows Fn overlay
    +------------------------------------------------------------------------------+
    |BOOT|BRID|BRIU|TASK|FLXP|RMVD|RMVU|MPRV|MPLY|MNXT|MUTE|VOLD|VOLU|    |   |RMNX|
    +------------------------------------------------------------------------------+
    |    |    |    |    |    |    |    |    |    |    |    |    |    |        |    |
    +------------------------------------------------------------------------------+
    |      |    |    |    |    |    |    |    |    |    |    |    |    |      |    |
    +-------------------------------------------------------------------      -----+
    |       |    |    |    |    |    |    |    |    |    |    |    |    |     |    |
    +------------------------------------------------------------------------------+
    |     |    |    |    |    |    |    |    |    |    |    |    |       |RMSU|    |
    +------------------------------------------------------------------------------+
    |     |     |     |                             |     |    |    |RMHD|RMSD|RMHU|
    +------------------------------------------------------------------------------+
*/
  [WIN_FN] = LAYOUT_75_iso(
  /*  0           1           2           3           4           5           6           7           8           9           10          11          12          13          14          15         */
      QK_BOOT,    KC_BRID,    KC_BRIU,    KC_TASK,    KC_FLXP,    RM_VALD,    RM_VALU,    KC_MPRV,    KC_MPLY,    KC_MNXT,    KC_MUTE,    KC_VOLD,    KC_VOLU,    _______,    _______,    RM_NEXT    ,
      _______,    BT_PRF1,    BT_PRF2,    BT_PRF3,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,                _______    ,
      _______,    _______,    _______,    _______,    BT_RESS,    _______,    _______,    _______,    _______,    _______,    BT_PAIR,    _______,    _______,                            _______    ,
      _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,                _______    ,
      _______,    _______,    _______,    _______,    _______,    _______,    BT_BATT,    _______,    _______,    _______,    _______,    _______,                _______,    RM_SATU,    _______    ,
      _______,    _______,    _______,                                        _______,                                        _______,    _______,    _______,    RM_HUED,    RM_SATD,    RM_HUEU
  )
};
