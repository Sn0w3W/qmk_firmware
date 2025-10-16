/*
Copyright 2025 mintyleaf

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

#include "quantum.h"
#include "keymap.h"
#include <stdint.h>

#ifdef BLUETOOTH_ITON_BT
#    include "iton_bt.h"
#    include "outputselect.h"
#    include "eeconfig.h"
#endif

#define STARTUP_FLASH_DURATION_MS 1500
static uint32_t startup_flash = STARTUP_FLASH_DURATION_MS;

static uint32_t last_update_time = 0;

#ifdef BLUETOOTH_ITON_BT
#    define BT_CONNECTION_SUCCESSFUL_DURATION_MS 2500
#    define BT_DISCONNECTED_DURATION_MS 2500
#    define BT_BATTERY_DURATION_MS 2500
#    define BT_BATTERY_WAIT_QUERY_DURATION_MS 10000

#    define BT_PROFILE_LED_START_INDEX 21

#    define BT_PAIRING_BLINK_MS 250
#    define BT_CONNECTING_BLINK_MS 250
#    define BT_DISCONNECTED_BLINK_MS 250
#    define KB_BT_PROFILE_MASK  (0x3u << 0)
#    define KB_BT_PROFILE_SHIFT 0

#    define NUM_BATTERY_LEVELS (sizeof(BATTERY_COLOR_MAP) / sizeof(BATTERY_COLOR_MAP[0]))

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
#endif

#ifdef BLUETOOTH_ITON_BT
static bool     ev_connecting_flag     = false;
static bool     ev_pairing_flag        = false;
static uint32_t ev_disconnected_timer  = 0;
static uint32_t ev_connected_timer     = 0;
static uint32_t ev_battery_level_timer = 0;

static uint32_t battery_level = 0;
static uint32_t bt_profile    = 0;
static uint8_t  mac_switch  = 0;

static bool bluetooth_dip_switch = false;

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

static void set_profile_led_blinking(uint32_t current_time, uint32_t blink_ms, uint8_t r, uint8_t g, uint8_t b) {
    uint8_t profile_index = BT_PROFILE_LED_START_INDEX + bt_profile;
    if ((current_time / blink_ms) % 2 == 0) {
        rgb_matrix_set_color(profile_index, r, g, b);
    } else {
        rgb_matrix_set_color(profile_index, 0, 0, 0);
    }
}

void iton_bt_connection_successful() {
    ev_connected_timer = BT_CONNECTION_SUCCESSFUL_DURATION_MS;
    ev_pairing_flag    = false;
    ev_connecting_flag = false;
}

void iton_bt_entered_pairing() {
    ev_pairing_flag    = true;
    ev_connected_timer = 0;
    ev_connecting_flag = false;
}

void iton_bt_enters_connection_state() {
    ev_connecting_flag = true;
    ev_connected_timer = 0;
    ev_pairing_flag    = false;
}

void iton_bt_disconnected() {
    ev_disconnected_timer = BT_DISCONNECTED_DURATION_MS;
    ev_connected_timer    = 0;
    ev_pairing_flag       = false;
    ev_connecting_flag    = false;
}

void iton_bt_battery_level(uint8_t level) {
    battery_level          = level;
    ev_battery_level_timer = BT_BATTERY_DURATION_MS;
}
#endif

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {

#ifdef BLUETOOTH_ITON_BT
    if (record->event.pressed && bluetooth_dip_switch) {
        switch (keycode) {
            case BT_PROFILE1:
            case BT_PROFILE2:
            case BT_PROFILE3:
                {
                    bt_profile_save();
                    uint8_t profile_idx = keycode - BT_PROFILE1;
                    bt_profile = profile_idx;
                    bt_profile_save();
                    iton_bt_switch_profile(profile_idx);
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
    uint32_t current_time = timer_read();
    uint32_t elapsed      = (current_time >= last_update_time) ? (current_time - last_update_time) : 1;
    last_update_time      = current_time;

    if (startup_flash > 0) {
        hsv_t flash     = (hsv_t){HSV_WHITE};
        flash.v         = (uint8_t)((startup_flash * 255UL) / STARTUP_FLASH_DURATION_MS);
        rgb_t flash_rgb = hsv_to_rgb(flash);
        rgb_matrix_set_color_all(flash_rgb.r, flash_rgb.g, flash_rgb.b);
        if (elapsed >= startup_flash) {
            startup_flash = 0;
        } else {
            startup_flash -= elapsed;
        }
        return true;
    }

    uint8_t layer = get_highest_layer(layer_state);
    if (layer != 0 && layer != 2) {
        for (uint8_t row = 0; row < MATRIX_ROWS; ++row) {
            for (uint8_t col = 0; col < MATRIX_COLS; ++col) {
                uint8_t index = g_led_config.matrix_co[row][col];

                if (index >= led_min && index < led_max && index != NO_LED) {
                uint16_t keycode = keymap_key_to_keycode(layer, (keypos_t){col, row});
                    #ifdef BLUETOOTH_ITON_BT
                        if (keycode == KC_TRNS || ((keycode == BT_PROFILE1 || keycode == BT_PROFILE2 || keycode == BT_PROFILE3 || keycode == BT_PAIR || keycode == BT_RESET || keycode == BT_BATTERY) && !bluetooth_dip_switch)) {
                            rgb_matrix_set_color(index, 0x01, 0x01, 0x01);
                        } else {
                            rgb_matrix_set_color(index, RGB_WHITE);
                        }
                    #else
                        if (keycode == KC_TRNS || keycode == BT_PROFILE1 || keycode == BT_PROFILE2 || keycode == BT_PROFILE3 || keycode == BT_PAIR || keycode == BT_RESET || keycode == BT_BATTERY) {
                            rgb_matrix_set_color(index, 0x01, 0x01, 0x01);
                        } else {
                            rgb_matrix_set_color(index, 255, 255, 255);
                        }
                    #endif
                }
            }
        }
    }

    led_t led_state = host_keyboard_led_state();

    const uint8_t NUM_LED_INDEX  = 16;
    const uint8_t CAPS_LED_INDEX = 17;
    const uint8_t MAC_LED_INDEX  = 18;
    const uint8_t WIN_LED_INDEX  = 19;

    if (led_state.num_lock)
        rgb_matrix_set_color(NUM_LED_INDEX, RGB_WHITE);
    else
        rgb_matrix_set_color(NUM_LED_INDEX, 0, 0, 0);

    if (led_state.caps_lock)
        rgb_matrix_set_color(CAPS_LED_INDEX, RGB_WHITE);
    else
        rgb_matrix_set_color(CAPS_LED_INDEX, 0, 0, 0);

    if (mac_switch) {
        rgb_matrix_set_color(MAC_LED_INDEX, RGB_WHITE);
        rgb_matrix_set_color(WIN_LED_INDEX, 0, 0, 0);
    } else {
        rgb_matrix_set_color(WIN_LED_INDEX, RGB_WHITE);
        rgb_matrix_set_color(MAC_LED_INDEX, 0, 0, 0);
    }

#ifdef BLUETOOTH_ITON_BT
    if (!bluetooth_dip_switch) {
        return true;
    }

    if (ev_pairing_flag) {
        set_profile_led_blinking(current_time, BT_PAIRING_BLINK_MS, RGB_BLUE);
    } else if (ev_connecting_flag) {
        set_profile_led_blinking(current_time, BT_CONNECTING_BLINK_MS, RGB_BLUE);
    } else if (ev_connected_timer > 0) {
        rgb_matrix_set_color(BT_PROFILE_LED_START_INDEX + bt_profile, RGB_WHITE);
    } else if (ev_disconnected_timer > 0) {
        set_profile_led_blinking(current_time, BT_DISCONNECTED_BLINK_MS, RGB_RED);
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
        rgb_t color = BATTERY_COLOR_MAP[battery_level];
        rgb_matrix_set_color_all(color.r, color.g, color.b);

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
            mac_switch = active;
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
            bluetooth_dip_switch = !active;
            return false;
#endif
    }
    return true;
}


// Keychron K5SE-specific. Without it keyboard spams Tab, ` and other keys.
void matrix_output_select_delay(void) {
    waitInputPinDelay();
    waitInputPinDelay();
    waitInputPinDelay();
    waitInputPinDelay();
}
