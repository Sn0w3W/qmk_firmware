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
#ifdef DEFERRED_EEPROM_SAVE
    #include "wear_leveling.h"
#endif

#ifdef BLUETOOTH_ITON_BT
    #include "iton_bt.h"
    #include "outputselect.h"
#endif

#ifdef BLUETOOTH_ITON_BT
    #define BT_BATTERY_DURATION_MS 2500
    #define BT_BATTERY_WAIT_QUERY_DURATION_MS 10000

    #define KB_BT_PROFILE_MASK  0x3u

    // BT module reports: 0x01 = below 30%, 0x02 = 30-70%, 0x04 = above 70%
    // bt_battery_map_level() converts raw values to 0/1/2 index
    static const rgb_t BATTERY_COLOR_MAP[] = {
        {RGB_RED},
        {RGB_YELLOW},
        {RGB_GREEN}
    };

    #define BT_BLINK_MAX_CYCLES 3
    #define BT_BLINK_RAMP_MS    200
    #define BT_BLINK_DWELL_MS   200
    #define BT_BLINK_CYCLE_MS   ((BT_BLINK_DWELL_MS + BT_BLINK_RAMP_MS) * 2)

    static struct {
        uint8_t dip_switch : 1;
    } flags = {0};

    static uint16_t last_update_time       = 0;
    static uint16_t ev_battery_level_timer = 0;

    static uint8_t battery_level           = 0;
    static uint8_t bt_profile              = 0;

    static uint8_t bt_matrix_indexes[4];

    static struct {
        uint8_t  color[3];          // текущий цвет анимации
        uint8_t  pending_color[3];  // последнее событие из очереди
        bool     has_pending;       // есть новое событие
        bool     active;            // анимация идёт
        bool     continuous;        // мигать пока не придёт новое событие
        uint16_t start_time;        // начало текущего цикла
        uint8_t  cycles_done;       // циклов без новых событий
        uint8_t  led_index;         // LED текущей анимации (меняется на границе цикла)
    } blink = {0};

    static void bt_blink_queue(uint8_t r, uint8_t g, uint8_t b, bool continuous) {
        blink.pending_color[0] = r;
        blink.pending_color[1] = g;
        blink.pending_color[2] = b;
        blink.has_pending = true;
        blink.continuous = continuous;
    }

    static void bt_blink_update(void) {
        // На границе цикла или при старте: подхватить pending
        if (!blink.active) {
            if (!blink.has_pending) return;
            blink.color[0] = blink.pending_color[0];
            blink.color[1] = blink.pending_color[1];
            blink.color[2] = blink.pending_color[2];
            blink.has_pending = false;
            blink.active = true;
            blink.cycles_done = 0;
            blink.led_index = bt_matrix_indexes[bt_profile];
            blink.start_time = timer_read();
        }

        uint16_t elapsed = timer_elapsed(blink.start_time);

        // Цикл завершён — проверяем очередь
        if (elapsed >= BT_BLINK_CYCLE_MS) {
            blink.start_time += (elapsed / BT_BLINK_CYCLE_MS) * BT_BLINK_CYCLE_MS;
            elapsed = timer_elapsed(blink.start_time);

            blink.led_index = bt_matrix_indexes[bt_profile];

            if (blink.has_pending) {
                // Новое событие — переключаем цвет, сбрасываем счётчик
                blink.color[0] = blink.pending_color[0];
                blink.color[1] = blink.pending_color[1];
                blink.color[2] = blink.pending_color[2];
                blink.has_pending = false;
                blink.cycles_done = 0;
            } else if (!blink.continuous) {
                blink.cycles_done++;
                if (blink.cycles_done >= BT_BLINK_MAX_CYCLES) {
                    blink.active = false;
                    return;
                }
            }
        }

        // Анимация: ⬛ dwell → ramp up → 💡 dwell → ramp down
        uint8_t val;
        if (elapsed < BT_BLINK_DWELL_MS) {
            val = 0;
        } else if (elapsed < BT_BLINK_DWELL_MS + BT_BLINK_RAMP_MS) {
            uint16_t t = elapsed - BT_BLINK_DWELL_MS;
            val = (uint8_t)((uint16_t)255 * t / BT_BLINK_RAMP_MS);
        } else if (elapsed < BT_BLINK_DWELL_MS * 2 + BT_BLINK_RAMP_MS) {
            val = 255;
        } else {
            uint16_t t = elapsed - BT_BLINK_DWELL_MS * 2 - BT_BLINK_RAMP_MS;
            val = (uint8_t)(255 - (uint16_t)255 * t / BT_BLINK_RAMP_MS);
        }

        // S-кривая: плавно у 0 и у 255
        if (val < 128) {
            val = (uint8_t)((uint16_t)val * val >> 7);
        } else {
            uint8_t inv = 255 - val;
            val = 255 - (uint8_t)((uint16_t)inv * inv >> 7);
        }

        uint8_t r_out = blink.color[0] * val / 255;
        uint8_t g_out = blink.color[1] * val / 255;
        uint8_t b_out = blink.color[2] * val / 255;

        rgb_matrix_set_color(blink.led_index, r_out, g_out, b_out);
    }

    static inline void bt_profile_save(void) {
        if (!eeconfig_is_enabled()) {
            eeconfig_init();
        }
        uint32_t kb = eeconfig_read_kb();
        kb &= ~KB_BT_PROFILE_MASK;
        kb |= (bt_profile & KB_BT_PROFILE_MASK);
        eeconfig_update_kb(kb);
    }

    static inline void bt_profile_load(void) {
        if (!eeconfig_is_enabled()) {
            eeconfig_init();
        }
        uint32_t kb = eeconfig_read_kb();
        uint32_t p  = kb & KB_BT_PROFILE_MASK;
        bt_profile  = (p <= 2) ? p : 0;
    }

    void iton_bt_connection_successful() {
        bt_blink_queue(RGB_WHITE, false);
    }

    void iton_bt_entered_pairing() {
        bt_blink_queue(RGB_BLUE, true);
    }

    void iton_bt_enters_connection_state() {
        bt_blink_queue(RGB_BLUE, true);
    }

    void iton_bt_disconnected() {
        bt_blink_queue(RGB_RED, false);
    }

    void iton_bt_battery_level(uint8_t level) {
        // 0x01 -> 0, 0x02 -> 1, 0x04 -> 2
        if (level == 0x01)      battery_level = 0;
        else if (level == 0x02) battery_level = 1;
        else                    battery_level = 2;
        ev_battery_level_timer = BT_BATTERY_DURATION_MS;
    }

    static void bt_recalc_matrix_indexes(void) {
        uint8_t fn_layer = (get_highest_layer(layer_state) <= 1) ? MAC_FN : WIN_FN;
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                uint8_t index = g_led_config.matrix_co[row][col];
                if (index == NO_LED) continue;
                uint16_t keycode = keymap_key_to_keycode(fn_layer, (keypos_t){col, row});
                switch (keycode) {
                    case BT_PROFILE1: bt_matrix_indexes[0] = index; break;
                    case BT_PROFILE2: bt_matrix_indexes[1] = index; break;
                    case BT_PROFILE3: bt_matrix_indexes[2] = index; break;
                    case BT_BATTERY:  bt_matrix_indexes[3] = index; break;
                }
            }
        }

    }
#endif

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    #ifdef BLUETOOTH_ITON_BT
        if (record->event.pressed && flags.dip_switch) {
            switch (keycode) {
                case BT_PROFILE1:
                case BT_PROFILE2:
                case BT_PROFILE3:
                {
                    uint8_t p = keycode - BT_PROFILE1;
                    bt_profile = p;
                    bt_profile_save();
                    iton_bt_switch_profile(p);
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

    static const uint16_t keycode_map[][2] = {
        {KC_LOPTN, KC_LOPT},
        {KC_ROPTN, KC_ROPT},
        {KC_LCMMD, KC_LCMD},
        {KC_RCMMD, KC_RCMD},
        {KC_MISC,  KC_MISSION_CONTROL},
        {KC_LAUN,  KC_LAUNCHPAD},
    };

    for (uint8_t i = 0; i < sizeof(keycode_map) / sizeof(keycode_map[0]); i++) {
        if (keycode == keycode_map[i][0]) {
            if (record->event.pressed) {
                register_code(keycode_map[i][1]);
            } else {
                unregister_code(keycode_map[i][1]);
            }
            return false;
        }
    }

    switch (keycode) {
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

    static hsv_t cached_hsv   = {0};
    static rgb_t cached_color = {0};

    hsv_t current_hsv = rgb_matrix_config.hsv;
    current_hsv.v = 255;

    if (current_hsv.h != cached_hsv.h || current_hsv.s != cached_hsv.s) {
        cached_color = hsv_to_rgb(current_hsv);
        cached_hsv   = current_hsv;
    }
    rgb_t color = cached_color;

    uint8_t layer = get_highest_layer(layer_state);
    for (uint8_t row = 0; row < MATRIX_ROWS; ++row) {
        for (uint8_t col = 0; col < MATRIX_COLS; ++col) {
            uint8_t index = g_led_config.matrix_co[row][col];
            if (index >= led_min && index < led_max && index != NO_LED && layer != 0 && layer != 2) {
                uint16_t keycode = keymap_key_to_keycode(layer, (keypos_t){col, row});
                bool is_inactive = (keycode == KC_TRNS || keycode == KC_NO);

                if (!is_inactive && keycode >= BT_PROFILE1 && keycode <= BT_RESET) {
                #ifdef BLUETOOTH_ITON_BT
                    is_inactive = !flags.dip_switch;
                #else
                    is_inactive = true;
                #endif
                }

                if (is_inactive) {
                    rgb_matrix_set_color(index, 0x01, 0x01, 0x01);
                } else {
                    rgb_matrix_set_color(index, color.r, color.g, color.b);
                }
            }
        }
    }

    #ifdef BLUETOOTH_ITON_BT
        if (!flags.dip_switch) {
            return true;
        }

        bt_blink_update();

        uint16_t elapsed = timer_elapsed(last_update_time);
        last_update_time = timer_read();

        if (ev_battery_level_timer > 0) {
            rgb_t batt_color = BATTERY_COLOR_MAP[battery_level];
            rgb_matrix_set_color(bt_matrix_indexes[3], batt_color.r, batt_color.g, batt_color.b);

            if (ev_battery_level_timer > elapsed) {
                ev_battery_level_timer -= elapsed;
            } else {
                ev_battery_level_timer = 0;
                battery_level          = 0;
            }
        }
    #endif
    return true;
}

#if defined(BLUETOOTH_ITON_BT) || defined(DEFERRED_EEPROM_SAVE)
void housekeeping_task_kb(void) {
#ifdef BLUETOOTH_ITON_BT
    static bool bt_indexes_initialized = false;
    if (!bt_indexes_initialized) {
        bt_recalc_matrix_indexes();
        bt_indexes_initialized = true;
    }
    iton_bt_task();
#ifdef ITON_BT_ENABLE_LED_STATE
    static uint8_t prev_bt_led_state = 0xFF;
    if (iton_bt_led_state != prev_bt_led_state) {
        prev_bt_led_state = iton_bt_led_state;
        led_set(iton_bt_led_state);
    }
#endif
#endif
#ifdef DEFERRED_EEPROM_SAVE
    static bool deferred_initialized = false;
    if (!deferred_initialized) {
        wear_leveling_set_deferred(true);
        deferred_initialized = true;
    }

    if (wear_leveling_cache_is_dirty() && timer_elapsed(wear_leveling_last_dirty_time()) > EEPROM_FLUSH_DELAY_MS) {
        wear_leveling_flush_cache();
    }
#endif
}
#endif

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
