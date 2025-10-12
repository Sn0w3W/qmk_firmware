// Copyright 2023 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#include "keymap.h"

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    /*
     * ┌───┐   ┌───┬───┬───┬───┐ ┌───┬───┬───┬───┐ ┌───┬───┬───┬───┐ ┌───┬───┬───┐
     * │Esc│   │F1 │F2 │F3 │F4 │ │F5 │F6 │F7 │F8 │ │F9 │F10│F11│F12│ │SCR│Nan│RGB│
     * └───┘   └───┴───┴───┴───┘ └───┴───┴───┴───┘ └───┴───┴───┴───┘ └───┴───┴───┘
     * ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───────┐ ┌───┬───┬───┐ ┌───┬───┬───┬───┐
     * │ ` │ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │ 8 │ 9 │ 0 │ - │ = │ Backsp│ │Ins│Hom│PgU│ │Num│ / │ * │ - │
     * ├───┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─────┤ ├───┼───┼───┤ ├───┼───┼───┼───┤
     * │ Tab │ Q │ W │ E │ R │ T │ Y │ U │ I │ O │ P │ [ │ ] │  \  │ │Del│End│PgD│ │ 7 │ 8 │ 9 │   │
     * ├─────┴┬──┴┬──┴┬──┴┬──┴┬──┴┬──┴┬──┴┬──┴┬──┴┬──┴┬──┴┬──┴─────┤ └───┴───┴───┘ ├───┼───┼───┤ + │
     * │ Caps │ A │ S │ D │ F │ G │ H │ J │ K │ L │ ; │ ' │  Enter │               │ 4 │ 5 │ 6 │   │
     * ├──────┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴────────┤     ┌───┐     ├───┼───┼───┼───┤
     * │ Shift  │ Z │ X │ C │ V │ B │ N │ M │ , │ . │ / │    Shift │     │ ↑ │     │ 1 │ 2 │ 3 │   │
     * ├────┬───┴┬──┴─┬─┴───┴───┴───┴───┴───┴──┬┴───┼───┴┬────┬────┤ ┌───┼───┼───┐ ├───┴───┼───┤Ent│
     * │Ctrl│GUI │Alt │                        │ Alt│ GUI│Menu│Ctrl│ │ ← │ ↓ │ → │ │   0   │ . │   │
     * └────┴────┴────┴────────────────────────┴────┴────┴────┴────┘ └───┴───┴───┘ └───────┴───┴───┘
     */
    [MAC_BASE] = LAYOUT_fullsize_ansi(
    /*  0           1           2           3           4           5           6           7           8           9           10          11          12          13          14          15          16          17          18          19          20           */
/*0*/   KC_ESC,     KC_BRID,    KC_BRIU,    KC_MCTL,    KC_LPAD,    RGB_VAD,    RGB_VAI,    KC_MPRV,    KC_MPLY,    KC_MNXT,    KC_MUTE,    KC_VOLD,    KC_VOLU,                KC_MSNP,    KC_NO,      RGB_TOG,

/*1*/   KC_GRV,     KC_1,       KC_2,       KC_3,       KC_4,       KC_5,       KC_6,       KC_7,       KC_8,       KC_9,       KC_0,       KC_MINS,    KC_EQL,     KC_BSPC,    KC_INS,     KC_HOME,    KC_PGUP,    KC_NUM,     KC_PSLS,    KC_PAST,    KC_PMNS,
/*2*/   KC_TAB,     KC_Q,       KC_W,       KC_E,       KC_R,       KC_T,       KC_Y,       KC_U,       KC_I,       KC_O,       KC_P,       KC_LBRC,    KC_RBRC,    KC_BSLS,    KC_DEL,     KC_END,     KC_PGDN,    KC_P7,      KC_P8,      KC_P9,      KC_PPLS,
/*3*/   KC_CAPS,    KC_A,       KC_S,       KC_D,       KC_F,       KC_G,       KC_H,       KC_J,       KC_K,       KC_L,       KC_SCLN,    KC_QUOT,                KC_ENT,                                         KC_P4,      KC_P5,      KC_P6,
/*4*/   KC_LSFT,                KC_Z,       KC_X,       KC_C,       KC_V,       KC_B,       KC_N,       KC_M,       KC_COMM,    KC_DOT,     KC_SLSH,                KC_RSFT,                KC_UP,                  KC_P1,      KC_P2,      KC_P3,      KC_PENT,
/*5*/   KC_LCTL,    KC_LALT,    KC_LGUI,                                        KC_SPC,                                         KC_RGUI,    KC_RALT,    MO(MAC_FN), KC_RCTL,    KC_LEFT,    KC_DOWN,    KC_RGHT,    KC_P0,                  KC_PDOT
    ),
    [MAC_FN] = LAYOUT_fullsize_ansi(
    /*  0           1           2           3           4           5           6           7           8           9           10          11          12          13          14          15          16          17          18          19          20           */
/*0*/   QK_BOOT,    KC_F1,      KC_F2,      KC_F3,      KC_F4,      KC_F5,      KC_F6,      KC_F7,      KC_F8,      KC_F9,      KC_F10,     KC_F11,     KC_F12,                 _______,    _______,    RGB_MOD,

/*1*/   _______,    BT_PROFILE1,BT_PROFILE2,BT_PROFILE3,_______,    _______,    _______,    _______,    _______,    _______,    _______,    RGB_SPD,    RGB_SPI,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,
/*2*/   _______,    _______,    _______,    _______,    BT_RESET,   _______,    _______,    _______,    _______,    _______,    BT_PAIR,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,
/*3*/   _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,                _______,                                        _______,    _______,    _______,
/*4*/   _______,                _______,    _______,    _______,    _______,    BT_BATTERY, _______,    _______,    _______,    _______,    _______,                _______,                RGB_SAI,                _______,    _______,    _______,    _______,
/*5*/   _______,    _______,    _______,                                        _______,                                        _______,    _______,    _______,    _______,    RGB_HUD,    RGB_SAD,    RGB_HUI,    _______,                _______
    ),
    [WIN_BASE] = LAYOUT_fullsize_ansi(
    /*  0           1           2           3           4           5           6           7           8           9           10          11          12          13          14          15          16          17          18          19          20           */
/*0*/   KC_ESC,     KC_F1,      KC_F2,      KC_F3,      KC_F4,      KC_F5,      KC_F6,      KC_F7,      KC_F8,      KC_F9,      KC_F10,     KC_F11,     KC_F12,                 KC_SNIP,    KC_NO,      RGB_TOG,

/*1*/   KC_GRV,     KC_1,       KC_2,       KC_3,       KC_4,       KC_5,       KC_6,       KC_7,       KC_8,       KC_9,       KC_0,       KC_MINS,    KC_EQL,     KC_BSPC,    KC_INS,     KC_HOME,    KC_PGUP,    KC_NUM,     KC_PSLS,    KC_PAST,    KC_PMNS,
/*2*/   KC_TAB,     KC_Q,       KC_W,       KC_E,       KC_R,       KC_T,       KC_Y,       KC_U,       KC_I,       KC_O,       KC_P,       KC_LBRC,    KC_RBRC,    KC_BSLS,    KC_DEL,     KC_END,     KC_PGDN,    KC_P7,      KC_P8,      KC_P9,      KC_PPLS,
/*3*/   KC_CAPS,    KC_A,       KC_S,       KC_D,       KC_F,       KC_G,       KC_H,       KC_J,       KC_K,       KC_L,       KC_SCLN,    KC_QUOT,                KC_ENT,                                         KC_P4,      KC_P5,      KC_P6,
/*4*/   KC_LSFT,                KC_Z,       KC_X,       KC_C,       KC_V,       KC_B,       KC_N,       KC_M,       KC_COMM,    KC_DOT,     KC_SLSH,                KC_RSFT,                KC_UP,                  KC_P1,      KC_P2,      KC_P3,      KC_PENT,
/*5*/   KC_LCTL,    KC_LALT,    KC_LGUI,                                        KC_SPC,                                         KC_RGUI,    KC_RALT,    MO(WIN_FN), KC_RCTL,    KC_LEFT,    KC_DOWN,    KC_RGHT,    KC_P0,                  KC_PDOT
    ),
    [WIN_FN] = LAYOUT_fullsize_ansi(
    /*  0           1           2           3           4           5           6           7           8           9           10          11          12          13          14          15          16          17          18          19          20           */
/*0*/   QK_BOOT,    KC_BRID,    KC_BRIU,    KC_TASK,    KC_FLXP,    RGB_VAD,    RGB_VAI,    KC_MPRV,    KC_MPLY,    KC_MNXT,    KC_MUTE,    KC_VOLD,    KC_VOLU,                _______,    _______,    RGB_MOD,

/*1*/   _______,    BT_PROFILE1,BT_PROFILE2,BT_PROFILE3,_______,    _______,    _______,    _______,    _______,    _______,    _______,    RGB_SPD,    RGB_SPI,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,
/*2*/   _______,    _______,    _______,    _______,    BT_RESET,   _______,    _______,    _______,    _______,    _______,    BT_PAIR,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,
/*3*/   _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,                _______,                                        _______,    _______,    _______,
/*4*/   _______,                _______,    _______,    _______,    _______,    BT_BATTERY, _______,    _______,    _______,    _______,    _______,                _______,                RGB_SAI,                _______,    _______,    _______,    _______,
/*5*/   _______,    _______,    _______,                                        _______,                                        _______,    _______,    _______,    _______,    RGB_HUD,    RGB_SAD,    RGB_HUI,    _______,                _______
    )
};
