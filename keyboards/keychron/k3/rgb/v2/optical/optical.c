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

#include "keymap.h"

#ifdef BLUETOOTH_ITON_BT
    #include "iton_bt.h"
    #include "outputselect.h"
#endif

#ifdef BLUETOOTH_ITON_BT
    #define BT_CONNECTION_SUCCESSFUL_DURATION_MS 2500
    #define BT_DISCONNECTED_DURATION_MS 2500
    #define BT_BATTERY_DURATION_MS 2500
    #define BT_BATTERY_WAIT_QUERY_DURATION_MS 10000

    #define KB_BT_PROFILE_MASK  (0x3u << 0)
    #define KB_BT_PROFILE_SHIFT 0

    #define NUM_BATTERY_LEVELS (sizeof(BATTERY_COLOR_MAP) / sizeof(BATTERY_COLOR_MAP[0]))

    typedef enum {
        BATTERY_LEVEL_NONE = 0,
        BATTERY_LEVEL_CRITICAL = 1,
        BATTERY_LEVEL_LOW = 2,
        BATTERY_LEVEL_MEDIUM = 3,
        BATTERY_LEVEL_FULL = 4
    } battery_level_t;

    const rgb_t BATTERY_COLOR_MAP[] = {
        {RGB_WHITE},
        {RGB_RED},
        {RGB_ORANGE},
        {RGB_YELLOW},
        {RGB_GREEN}
    };

    static struct {
        uint8_t connecting : 1;
        uint8_t pairing    : 1;
        uint8_t dip_switch : 1;
    } flags = {0};

    static uint16_t last_update_time = 0;
    static uint16_t ev_disconnected_timer  = 0;
    static uint16_t ev_connected_timer     = 0;
    static uint16_t ev_battery_level_timer = 0;

    static uint8_t battery_level           = 0;
    static uint8_t bt_profile              = 0;
    static uint8_t bt_prof_index           = 0;

    static uint8_t bt_matrix_indexes[4];
    static uint8_t blinking_color[3];


    static inline void bt_profile_save(void) {
        if (!eeconfig_is_enabled()) {
            eeconfig_init();
        }
        uint32_t kb = eeconfig_read_kb();
        kb &= ~KB_BT_PROFILE_MASK;
        kb |= ((bt_profile & 0x3u) << KB_BT_PROFILE_SHIFT);
        eeconfig_update_kb(kb);
    }

    static inline void bt_profile_load(void) {
        if (!eeconfig_is_enabled()) {
            eeconfig_init();
        }
        uint32_t kb = eeconfig_read_kb();
        uint32_t p  = (kb & KB_BT_PROFILE_MASK) >> KB_BT_PROFILE_SHIFT;
        bt_profile  = (p <= 2) ? p : 0;
    }

    static void set_profile_led_blinking(uint8_t r, uint8_t g, uint8_t b) {
        static uint16_t current_time = 0;

        uint8_t phase = 70;
        uint16_t t = current_time % 280;
        uint8_t val = 0;

        if (t < phase / 2) {
            if (r == 0 && g == 0 && b == 0) {
                if (current_time != 0)
                    current_time = 0;
                return;
            }
            blinking_color[0]  = r;
            blinking_color[1]  = g;
            blinking_color[2]  = b;
            val = 0;
        } else if (t < (phase + phase / 2)) {
            val = (uint8_t)((uint16_t)255 * (t - phase / 2) / phase);
        } else if (t < (phase * 2 + phase / 2)) {
            val = 255;
        } else if (t < (phase * 3 + phase / 2)) {
            val = (uint8_t)(255 - (uint16_t)255 * (t - (phase * 2 + phase / 2)) / phase);
        } else {
            val = 0;
            current_time = 0;
        }

        uint8_t r_out = blinking_color[0] * val / 255;
        uint8_t g_out = blinking_color[1] * val / 255;
        uint8_t b_out = blinking_color[2] * val / 255;

        rgb_matrix_set_color(bt_matrix_indexes[bt_profile], r_out, g_out, b_out);

        current_time += 1;
    }

    void iton_bt_connection_successful() {
        ev_connected_timer = BT_CONNECTION_SUCCESSFUL_DURATION_MS;
        flags.pairing      = 0;
        flags.connecting   = 0;
    }

    void iton_bt_entered_pairing() {
        flags.pairing      = 1;
        ev_connected_timer = 0;
        flags.connecting   = 0;
    }

    void iton_bt_enters_connection_state() {
        flags.connecting   = 1;
        ev_connected_timer = 0;
        flags.pairing      = 0;
    }

    void iton_bt_disconnected() {
        ev_disconnected_timer = BT_DISCONNECTED_DURATION_MS;
        ev_connected_timer    = 0;
        flags.pairing         = 0;
        flags.connecting      = 0;
    }

    void iton_bt_battery_level(uint8_t level) {
        battery_level          = level;
        ev_battery_level_timer = BT_BATTERY_DURATION_MS;
    }
#endif

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    #ifdef BLUETOOTH_ITON_BT
        if (record->event.pressed && flags.dip_switch) {
            switch (keycode) {
                case BT_PROFILE1:
                    {
                        bt_profile = 0;
                        bt_prof_index = bt_matrix_indexes[0];
                        bt_profile_save();
                        iton_bt_switch_profile(0);
                    }
                    return false;
                case BT_PROFILE2:
                    {
                        bt_profile = 1;
                        bt_prof_index = bt_matrix_indexes[1];
                        bt_profile_save();
                        iton_bt_switch_profile(1);
                    }
                    return false;
                case BT_PROFILE3:
                    {
                        bt_profile = 2;
                        bt_prof_index = bt_matrix_indexes[2];
                        bt_profile_save();
                        iton_bt_switch_profile(2);
                    }
                    return false;
                case BT_PAIR:
                    iton_bt_enter_pairing();
                    return false;
                case BT_RESET:
                    iton_bt_reset_pairing();
                    return false;
                case BT_BATTERY:
                    ev_battery_level_timer = BT_BATTERY_WAIT_QUERY_DURATION_MS;
                    iton_bt_query_battery_level();
                    return false;
                default:
                    break;
            }
        }
    #endif

    switch (keycode) {
        case KC_LOPTN:
            if (record->event.pressed) {
                register_code(KC_LOPT);
            } else {
                unregister_code(KC_LOPT);
            }
            return false;
        case KC_ROPTN:
            if (record->event.pressed) {
                register_code(KC_ROPT);
            } else {
                unregister_code(KC_ROPT);
            }
            return false;
        case KC_LCMMD:
            if (record->event.pressed) {
                register_code(KC_LCMD);
            } else {
                unregister_code(KC_LCMD);
            }
            return false;
        case KC_RCMMD:
            if (record->event.pressed) {
                register_code(KC_RCMD);
            } else {
                unregister_code(KC_RCMD);
            }
            return false;
        case KC_MISC:
            if (record->event.pressed) {
                register_code(KC_MISSION_CONTROL);
            } else {
                unregister_code(KC_MISSION_CONTROL);
            }
            return false;
        case KC_LAUN:
            if (record->event.pressed) {
                register_code(KC_LAUNCHPAD);
            } else {
                unregister_code(KC_LAUNCHPAD);
            }
            return false;
        case KC_SPOT:
            if (record->event.pressed) {
                host_consumer_send(0x221);
            } else {
                host_consumer_send(0x0);
            }
            return false;
        case KC_DICT:
            if (record->event.pressed) {
                host_consumer_send(0xCF);
            } else {
                host_consumer_send(0x0);
            }
            return false;
        case KC_DNDB:
            if (record->event.pressed) {
                host_system_send(0x9B);
            } else {
                host_system_send(0);
            }
            return false;
        case KC_MSCR:
            if (record->event.pressed) {
                register_code(KC_LSFT);
                register_code(KC_LCMD);
                register_code(KC_5);
            } else {
                unregister_code(KC_LSFT);
                unregister_code(KC_LCMD);
                unregister_code(KC_5);
            }
            return false;
        case KC_TASK:
            if (record->event.pressed) {
                register_code(KC_LWIN);
                register_code(KC_TAB);
            } else {
                unregister_code(KC_LWIN);
                unregister_code(KC_TAB);
            }
            return false;
        case KC_FLXP:
            if (record->event.pressed) {
                register_code(KC_LWIN);
                register_code(KC_E);
            } else {
                unregister_code(KC_LWIN);
                unregister_code(KC_E);
            }
            return false;
        case KC_SNIP:
            if (record->event.pressed) {
                register_code(KC_LSFT);
                register_code(KC_LWIN);
                register_code(KC_S);
            } else {
                unregister_code(KC_LSFT);
                unregister_code(KC_LWIN);
                unregister_code(KC_S);
            }
            return false;
        default:
            break;
    }
    return process_record_user(keycode, record);
}

bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    #ifdef BLUETOOTH_ITON_BT
        uint16_t current_time = timer_read();
        uint16_t elapsed      = timer_elapsed(last_update_time);
        last_update_time      = current_time;
    #endif

    hsv_t hsv_color = rgb_matrix_config.hsv;
    hsv_color.v     = 255;
    rgb_t color     = hsv_to_rgb(hsv_color);

    uint8_t layer = get_highest_layer(layer_state);
    for (uint8_t row = 0; row < MATRIX_ROWS; ++row) {
        for (uint8_t col = 0; col < MATRIX_COLS; ++col) {
            uint8_t index = g_led_config.matrix_co[row][col];
            if (index >= led_min && index < led_max && index != NO_LED && layer != 0 && layer != 2) {
                uint16_t keycode = keymap_key_to_keycode(layer, (keypos_t){col, row});
                #ifdef BLUETOOTH_ITON_BT
                    if (keycode == KC_TRNS || keycode == KC_NO || ((keycode == BT_PROFILE1 || keycode == BT_PROFILE2 || keycode == BT_PROFILE3 || keycode == BT_PAIR || keycode == BT_RESET || keycode == BT_BATTERY) && !flags.dip_switch)) {
                        rgb_matrix_set_color(index, 0x01, 0x01, 0x01);
                    } else {
                        rgb_matrix_set_color(index, color.r, color.g, color.b);
                    }
                #else
                    if (keycode == KC_TRNS || keycode == KC_NO || keycode == BT_PROFILE1 || keycode == BT_PROFILE2 || keycode == BT_PROFILE3 || keycode == BT_PAIR || keycode == BT_RESET || keycode == BT_BATTERY) {
                        rgb_matrix_set_color(index, 0x01, 0x01, 0x01);
                    } else {
                        rgb_matrix_set_color(index, color.r, color.g, color.b);
                    }
                #endif
            }
            #ifdef BLUETOOTH_ITON_BT
                if (index >= led_min && index < led_max && index != NO_LED && (layer == 0 || layer == 2) && flags.dip_switch) {
                    uint16_t keycode = keymap_key_to_keycode(layer + 1, (keypos_t){col, row});
                    switch (keycode) {
                        case BT_PROFILE1:
                            bt_matrix_indexes[0] = index;
                            break;
                        case BT_PROFILE2:
                            bt_matrix_indexes[1] = index;
                            break;
                        case BT_PROFILE3:
                            bt_matrix_indexes[2] = index;
                            break;
                        case BT_BATTERY:
                            bt_matrix_indexes[3] = index;
                            break;
                        default:
                            break;
                    }

                    switch (bt_profile) {
                        case 0:
                            bt_prof_index = bt_matrix_indexes[0];
                            break;
                        case 1:
                            bt_prof_index = bt_matrix_indexes[1];
                            break;
                        case 2:
                            bt_prof_index = bt_matrix_indexes[2];
                            break;
                        default:
                            break;
                    }
                }
            #endif
        }
    }

    #ifdef BLUETOOTH_ITON_BT
        if (!flags.dip_switch) {
            return true;
        }

        if (flags.pairing) {
            set_profile_led_blinking(RGB_BLUE);
        } else if (flags.connecting) {
            set_profile_led_blinking(RGB_BLUE);
        } else if (ev_connected_timer > 0) {
            set_profile_led_blinking(RGB_WHITE);
        } else if (ev_disconnected_timer > 0) {
            set_profile_led_blinking(RGB_RED);
        } else {
            set_profile_led_blinking(RGB_BLACK);
        }

        if (ev_connected_timer > elapsed)
            ev_connected_timer -= elapsed;
        else
            ev_connected_timer = 0;
        if (ev_disconnected_timer > elapsed)
            ev_disconnected_timer -= elapsed;
        else
            ev_disconnected_timer = 0;

        if (ev_battery_level_timer > 0) {
            rgb_t batt_color = BATTERY_COLOR_MAP[battery_level];
            rgb_matrix_set_color(bt_matrix_indexes[3], batt_color.r, batt_color.g, batt_color.b);

            if (ev_battery_level_timer > elapsed) {
                ev_battery_level_timer -= elapsed;
            } else {
                ev_battery_level_timer = 0;
                battery_level          = BATTERY_LEVEL_NONE;
            }
        }
    #endif
    return true;
}

bool dip_switch_update_user(uint8_t index, bool active) {
    switch (index) {
        case 1:
            layer_move(active ? MAC_BASE : WIN_BASE);
            return false;
#ifdef BLUETOOTH_ITON_BT
        case 0:
            // dip switch is inactive in bt state
            if (!active) {
                set_output(OUTPUT_BLUETOOTH);
                iton_bt_init();
                bt_profile_load();
            } else {
                iton_bt_deinit();
                set_output(OUTPUT_USB);
            }
            flags.dip_switch = !active;
            return false;
#endif
    }
    return true;
}

// Keychron K3-specific. Without it keyboard spams Tab, ` and other keys in bluetooth mode.
void matrix_output_select_delay(void) {
    waitInputPinDelay();
    waitInputPinDelay();
}
