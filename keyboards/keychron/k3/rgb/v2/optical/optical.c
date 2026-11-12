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

#ifdef KB_IDLE_SLEEP
    #ifndef KB_IDLE_SLEEP_IDLE_MS
        #define KB_IDLE_SLEEP_IDLE_MS 5000 // no key press for this long, backlight dark
    #endif
    #ifndef KB_IDLE_SLEEP_TICK_MS
        #define KB_IDLE_SLEEP_TICK_MS 5 // main loop pause per pass while asleep
    #endif
    // What kb_sleep_check() found in the way of sleeping (KB_SLEEP_OK: nothing).
    #define KB_SLEEP_OK         0
    #define KB_SLEEP_ACTIVE     1 // a key was pressed within KB_IDLE_SLEEP_IDLE_MS
    #define KB_SLEEP_LIT        2 // the backlight is still on
    #define KB_SLEEP_BT_CFG     3 // control frames for the module are still queued
    #define KB_SLEEP_BT_PENDING 4 // the BT driver has ACKs or a retry for the main loop
#endif

#ifdef BLUETOOTH_ITON_BT
    #include "iton_bt.h"
    #include "outputselect.h"
    #include "atomic_util.h"

    #define BT_BATTERY_DURATION_MS 2500
    #define BT_BATTERY_WAIT_QUERY_DURATION_MS 10000

    #define KB_BT_PROFILE_MASK  0x3u

    // BT module reports: 0x01 = below 30%, 0x02 = 30-70%, 0x04 = above 70%
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

    // LED of the BT profile 1-3 and battery keys on the Fn layer. NO_LED until found,
    // so an unmapped key never lights an unrelated LED.
    static uint8_t bt_matrix_indexes[4] = {NO_LED, NO_LED, NO_LED, NO_LED};
    static bool    bt_indexes_dirty     = true;

    // Set from the module's battery notifications (ISR context), handled in housekeeping.
    static volatile bool bt_battery_critical = false;

    // ---- Talking to the module the way the stock K3 V1.17 does ----
    // Control frames (A6 ...) are queued and sent from the main loop, at least
    // BT_MODULE_CMD_GAP_MS apart (the module can silently drop control frames sent back to
    // back, SonixQMK PR #485) and not before BT_MODULE_BOOT_DELAY_MS after power-up (the
    // stock sends nothing to the module for its first 2 s).
    #ifndef BT_MODULE_CMD_GAP_MS
        #define BT_MODULE_CMD_GAP_MS 500
    #endif
    #ifndef BT_MODULE_BOOT_DELAY_MS
        #define BT_MODULE_BOOT_DELAY_MS 2000
    #endif
    #define BT_CMDQ_LEN 6
    static struct {
        uint8_t  cmd[BT_CMDQ_LEN];
        uint8_t  param[BT_CMDQ_LEN];
        uint8_t  head;
        uint8_t  count;
        uint32_t last_sent;    // when the last control frame went out
        bool     sent_once;
        bool     deinit_after; // USB mode: stop the driver once the queue is empty
    } bt_cmdq = {0};

    // Module events. The driver calls back from the INT interrupt; housekeeping acts.
    static volatile bool bt_evt_connected = false; // connected: (re)send the sleep setup
    static volatile bool bt_evt_pairing   = false; // the module entered pairing
    static volatile bool bt_module_hold   = false; // batt_mcu_sleep, until the module wakes us
    static volatile bool bt_low_battery   = false; // batt_voltage_low, until batt_exit_low_battery_mode

    // Pairing: like the stock, up to 3 enter_pairing frames ~300 ms apart (here: at least
    // BT_MODULE_CMD_GAP_MS) until the module reports pairing mode.
    #define BT_PAIR_TRIES    3
    #define BT_PAIR_RETRY_MS 300
    static uint8_t bt_pair_tries = 0; // frames queued in the current attempt, 0 = none
    // The stock stops its sleep countdown while pairing, for up to 180 s.
    #define BT_PAIRING_WINDOW_MS 180000
    static bool     bt_pairing      = false;
    static uint32_t bt_pairing_time = 0;

    static void bt_cmd_mark_sent(void) {
        bt_cmdq.last_sent = timer_read32();
        bt_cmdq.sent_once = true;
    }

    static bool bt_cmd_waiting(uint8_t cmd, uint8_t param) {
        for (uint8_t i = 0; i < bt_cmdq.count; i++) {
            uint8_t idx = (bt_cmdq.head + i) % BT_CMDQ_LEN;
            if (bt_cmdq.cmd[idx] == cmd && bt_cmdq.param[idx] == param) return true;
        }
        return false;
    }

    // Main loop only. A frame already waiting is not queued twice; a full queue drops it.
    static void bt_cmd_push(uint8_t cmd, uint8_t param) {
        if (bt_cmd_waiting(cmd, param) || bt_cmdq.count == BT_CMDQ_LEN) return;
        uint8_t idx        = (bt_cmdq.head + bt_cmdq.count) % BT_CMDQ_LEN;
        bt_cmdq.cmd[idx]   = cmd;
        bt_cmdq.param[idx] = param;
        bt_cmdq.count++;
    }

    static void bt_cmd_clear(void) {
        bt_cmdq.head         = 0;
        bt_cmdq.count        = 0;
        bt_cmdq.deinit_after = false;
        bt_pair_tries        = 0;
    }

    static void bt_cmd_task(void) {
        if (!bt_cmdq.count) return;
        if (bt_cmdq.sent_once && timer_elapsed32(bt_cmdq.last_sent) < BT_MODULE_CMD_GAP_MS) return;
        if (timer_read32() < BT_MODULE_BOOT_DELAY_MS) return;
        uint8_t idx  = bt_cmdq.head;
        bt_cmdq.head = (bt_cmdq.head + 1) % BT_CMDQ_LEN;
        bt_cmdq.count--;
        iton_bt_control(bt_cmdq.cmd[idx], bt_cmdq.param[idx]);
        bt_cmd_mark_sent();
    }

    #ifdef BT_MODULE_SLEEP_IDLE
    // Our addition (the stock sends these only on its Fn+S+... combos): the idle timeout,
    // then enable_sleep, which also undoes a disable_sleep left by older firmware (the
    // module keeps its settings while it has power, across MCU resets).
    static void bt_module_sleep_setup(void) {
        bt_cmd_push(control_power, BT_MODULE_SLEEP_IDLE);
        bt_cmd_push(control_bt, enable_sleep);
    }
    #endif


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

    static void bt_profile_save(void) {
        if (!eeconfig_is_enabled()) return;
        uint32_t kb = eeconfig_read_kb();
        kb &= ~KB_BT_PROFILE_MASK;
        kb |= (bt_profile & KB_BT_PROFILE_MASK);
        eeconfig_update_kb(kb);
    }

    static void bt_profile_load(void) {
        if (!eeconfig_is_enabled()) return;
        uint32_t kb = eeconfig_read_kb();
        uint32_t p  = kb & KB_BT_PROFILE_MASK;
        bt_profile  = (p <= 2) ? p : 0;
    }

    // All module callbacks run in the INT interrupt: flags only. Any of these packets
    // also means the module is awake, which ends a batt_mcu_sleep hold.
    void iton_bt_connection_successful() {
        bt_blink_queue(RGB_WHITE, false);
        bt_evt_connected = true;
        bt_module_hold   = false;
    }

    void iton_bt_entered_pairing() {
        bt_blink_queue(RGB_BLUE, true);
        bt_evt_pairing = true;
        bt_module_hold = false;
    }

    void iton_bt_enters_connection_state() {
        bt_blink_queue(RGB_BLUE, true);
        bt_module_hold = false;
    }

    void iton_bt_disconnected() {
        bt_blink_queue(RGB_RED, false);
        bt_module_hold = false;
    }

    // Stock: on batt_mcu_sleep the MCU turns everything off and waits for the module.
    // Here the backlight goes off and the MCU sleeps between interrupts until the module
    // sends anything; keys are still scanned and sent (the stock does not scan them).
    void iton_bt_mcu_sleep_request(void) {
        bt_module_hold = true;
    }

    void iton_bt_mcu_wake(void) {
        bt_module_hold = false;
    }

    // Battery about to run out: get pending settings into flash while the
    // supply is still good (flash writes at low voltage can corrupt the store).
    void iton_bt_battery_voltage_low(void) {
        bt_battery_critical = true;
        bt_low_battery      = true;
    }

    void iton_bt_battery_exit_low_battery_mode(void) {
        bt_low_battery = false;
    }

    void iton_bt_battery_low_power_shutdown(void) {
        bt_battery_critical = true;
    }

    void iton_bt_battery_level(uint8_t level) {
        bt_module_hold = false;
        // 0x01 -> 0, 0x02 -> 1, 0x04 -> 2
        if (level == 0x01)      battery_level = 0;
        else if (level == 0x02) battery_level = 1;
        else                    battery_level = 2;
        ev_battery_level_timer = BT_BATTERY_DURATION_MS;
    }

    static void bt_recalc_matrix_indexes(void) {
        uint8_t fn_layer = (get_highest_layer(layer_state) <= 1) ? MAC_FN : WIN_FN;
        for (uint8_t i = 0; i < sizeof(bt_matrix_indexes); i++) bt_matrix_indexes[i] = NO_LED;
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
        iton_bt_task();
        if (record->event.pressed && flags.dip_switch) {
            switch (keycode) {
                case BT_PROFILE1:
                case BT_PROFILE2:
                case BT_PROFILE3:
                {
                    uint8_t p = keycode - BT_PROFILE1;
                    bt_profile = p;
                    bt_profile_save();
                    bt_cmd_push(control_bt, switch_profile + p);
                }
                    return false;
                case BT_PAIR:
                    bt_pairing    = false; // wait for the module to report it again
                    bt_pair_tries = 1;
                    bt_cmd_push(control_bt, enter_pairing);
                    return false;
                case BT_RESET:
                    bt_cmd_push(control_bt, reset_pairing);
                    return false;
                case BT_BATTERY:
                    ev_battery_level_timer = BT_BATTERY_WAIT_QUERY_DURATION_MS;
                    bt_cmd_push(control_bt, query_battery_level);
                    return false;
                default:
                    break;
            }
        }
    #endif

    switch (keycode) {
        case KC_LOPTN: keycode = KC_LOPT; break;
        case KC_ROPTN: keycode = KC_ROPT; break;
        case KC_LCMMD: keycode = KC_LCMD; break;
        case KC_RCMMD: keycode = KC_RCMD; break;
        case KC_MISC:  keycode = KC_MISSION_CONTROL; break;
        case KC_LAUN:  keycode = KC_LAUNCHPAD; break;
        case KC_SPOT:
            host_consumer_send(record->event.pressed ? 0x221 : 0x0);
            return false;
        case KC_DICT:
            host_consumer_send(record->event.pressed ? 0xCF : 0x0);
            return false;
        case KC_DNDB:
            host_system_send(record->event.pressed ? 0x9B : 0);
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
            return process_record_user(keycode, record);
    }

    if (record->event.pressed) {
        register_code(keycode);
    } else {
        unregister_code(keycode);
    }
    return process_record_user(keycode, record);
}

#ifdef K3_DIAG
// Debug-only (-DK3_DIAG): for 10 s after boot Esc shows why the MCU last reset
// (red LVD/brown-out, blue external reset pin, yellow watchdog, magenta software
// reset e.g. after flashing, green power-on); F1 flashes orange for 2 s whenever
// the BT bus waited for the module until ITON_BT_TX_TIMEOUT_MS.
static uint8_t  diag_reset_flags   = 0;
static uint16_t diag_boot_time     = 0;
static bool     diag_boot_shown    = false; // one-shot: timer_elapsed() is 16-bit and wraps every 65.5 s
static uint16_t diag_seen_timeouts = 0;
static uint16_t diag_timeout_time  = 0;
static bool     diag_timeout_flash = false;

#    ifdef KB_IDLE_SLEEP
// MCU sleep report, shown for 5 s after the key press that ends an idle period:
// - it slept for 1 s or more: Esc shows the share of that period the main loop spent
//   paused (cyan 90 % and up, yellow 50 % and up, red below) and F1-F12 light cyan, one
//   per whole minute (all twelve: 12 min or more). The core sleeps in those pauses
//   except while an interrupt runs, so the share is an upper bound of its sleep time;
// - it should have slept (no input, backlight dark) but never did: Esc purple, and red
//   F1 if queued module commands were in the way, red F2 if the BT driver was busy.
// Nothing at all: the idle period ended before the backlight went dark.
static bool     diag_sleeping       = false; // an idle period with pauses is in progress
static uint8_t  diag_block_seen     = 0;     // bit 0 KB_SLEEP_BT_CFG, bit 1 KB_SLEEP_BT_PENDING
static uint8_t  diag_block_shown    = 0;     // the bits of the report on screen, 0 = slept
static uint32_t diag_sleep_start    = 0;     // first pause of that period
static uint32_t diag_sleep_paused   = 0;     // total pause time in it, ms
static uint8_t  diag_sleep_minutes  = 0;
static uint8_t  diag_sleep_share    = 0;     // percent
static uint16_t diag_sleep_shown_at = 0;
static bool     diag_sleep_show     = false;

static void diag_sleep_paused_for(uint32_t pause_start, uint32_t ms) {
    if (!diag_sleeping) {
        diag_sleeping     = true;
        diag_sleep_start  = pause_start;
        diag_sleep_paused = 0;
    }
    diag_sleep_paused += ms;
}

// Blockers count only once the board should sleep (no input, backlight dark).
static void diag_sleep_note(uint8_t block) {
    if (block >= KB_SLEEP_BT_CFG) diag_block_seen |= 1 << (block - KB_SLEEP_BT_CFG);
}

// Called on every main loop pass. Short BT-driven wakes keep the period open; only
// input (a key press) ends it.
static void diag_sleep_check_wake(void) {
    if (!diag_sleeping && !diag_block_seen) return;
    if (last_input_activity_elapsed() >= KB_IDLE_SLEEP_IDLE_MS) return;
    uint8_t blocked = diag_block_seen;
    diag_block_seen = 0;
    if (diag_sleeping) {
        diag_sleeping = false;
        uint32_t span = timer_elapsed32(diag_sleep_start);
        if (span < 1000) return;
        uint32_t minutes    = span / 60000;
        uint32_t share      = diag_sleep_paused / (span / 100);
        diag_sleep_minutes  = minutes > 12 ? 12 : minutes;
        diag_sleep_share    = share > 100 ? 100 : share;
        diag_block_shown    = 0; // it slept: blockers were only short BT wakes in between
    } else {
        diag_block_shown = blocked;
    }
    diag_sleep_shown_at = timer_read();
    diag_sleep_show     = true;
}
#    endif

void keyboard_pre_init_kb(void) {
    diag_reset_flags = SN_SYS0->RSTST & 0x1F;
    SN_SYS0->RSTST   = 0x1F; // assumed write-1-to-clear; a stale cause after a plain power-on means it is not
    keyboard_pre_init_user();
}

static void diag_indicators(void) {
    if (!diag_boot_shown && timer_elapsed(diag_boot_time) >= 10000) {
        diag_boot_shown = true;
    }
    if (!diag_boot_shown) {
        uint8_t esc = g_led_config.matrix_co[0][0];
        if (diag_reset_flags & 0x04) {
            rgb_matrix_set_color(esc, RGB_RED); // LVD
        } else if (diag_reset_flags & 0x02) {
            rgb_matrix_set_color(esc, RGB_YELLOW); // watchdog
        } else if (diag_reset_flags & 0x08) {
            rgb_matrix_set_color(esc, RGB_BLUE); // external reset pin
        } else if (diag_reset_flags & 0x01) {
            rgb_matrix_set_color(esc, RGB_MAGENTA); // software reset
        } else if (diag_reset_flags & 0x10) {
            rgb_matrix_set_color(esc, RGB_GREEN); // power-on
        } else {
            rgb_matrix_set_color(esc, RGB_WHITE); // no flag
        }
    }
#    ifdef BLUETOOTH_ITON_BT
    uint16_t timeouts = iton_bt_stats.tx_timeouts;
    if (timeouts != diag_seen_timeouts) {
        diag_seen_timeouts = timeouts;
        diag_timeout_time  = timer_read();
        diag_timeout_flash = true;
    }
    if (diag_timeout_flash) {
        if (timer_elapsed(diag_timeout_time) < 2000) {
            rgb_matrix_set_color(g_led_config.matrix_co[0][1], RGB_ORANGE);
        } else {
            diag_timeout_flash = false;
        }
    }
#    endif
#    ifdef KB_IDLE_SLEEP
    if (diag_sleep_show) {
        if (timer_elapsed(diag_sleep_shown_at) < 5000) {
            uint8_t esc = g_led_config.matrix_co[0][0];
            if (diag_block_shown) {
                rgb_matrix_set_color(esc, RGB_PURPLE);
                if (diag_block_shown & 1) rgb_matrix_set_color(g_led_config.matrix_co[0][1], RGB_RED);
                if (diag_block_shown & 2) rgb_matrix_set_color(g_led_config.matrix_co[0][2], RGB_RED);
            } else if (diag_sleep_share >= 90) {
                rgb_matrix_set_color(esc, RGB_CYAN);
            } else if (diag_sleep_share >= 50) {
                rgb_matrix_set_color(esc, RGB_YELLOW);
            } else {
                rgb_matrix_set_color(esc, RGB_RED);
            }
            for (uint8_t col = 1; col <= diag_sleep_minutes; col++) {
                rgb_matrix_set_color(g_led_config.matrix_co[0][col], RGB_CYAN);
            }
        } else {
            diag_sleep_show = false;
        }
    }
#    endif
}
#endif

bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
#ifdef K3_DIAG
    diag_indicators();
#endif

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
            if (index >= led_min && index < led_max && index != NO_LED && layer != MAC_BASE && layer != WIN_BASE) {
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
                    rgb_matrix_set_color(index, 0, 0, 0);
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

#ifdef DEFERRED_EEPROM_SAVE
static void eeprom_flush_pending(void) {
    if (wear_leveling_cache_is_dirty()) {
        wear_leveling_flush_cache();
    }
}

// Deferred writes only live in RAM until flushed. Flush before any reset or
// bootloader jump (QK_BOOT, QK_REBOOT, EE_CLR, VIA bootloader jump), otherwise
// the last changes are lost and EE_CLR does not clear anything at all.
bool shutdown_kb(bool jump_to_bootloader) {
#    ifdef RGB_MATRIX_ENABLE
    eeconfig_force_flush_rgb_matrix();
#    endif
    eeprom_flush_pending();
    return shutdown_user(jump_to_bootloader);
}
// No suspend_power_down_kb() flush: BLUETOOTH_ENABLE sets NO_USB_STARTUP_CHECK,
// which compiles the USB suspend loop (and that hook) out of chibios.c.
#endif

#ifdef BLUETOOTH_ITON_BT
// bluetooth_init() runs after dip_switch_init() and starts the BT driver
// unconditionally. In USB mode the stock tells the module mode_usb 2 s after power-up:
// queue it and stop the driver once it is out (housekeeping).
void keyboard_post_init_kb(void) {
#    ifdef K3_DIAG
    diag_boot_time = timer_read();
#    endif
    if (!flags.dip_switch) {
        bt_cmd_clear();
        bt_cmd_push(control_usb, mode_usb);
        bt_cmdq.deinit_after = true;
    }
    keyboard_post_init_user();
}

// BT keys live on the Fn layer: re-read their LEDs whenever it is activated,
// so a VIA remap is picked up without a reboot.
layer_state_t layer_state_set_kb(layer_state_t state) {
    if (layer_state_cmp(state, MAC_FN) || layer_state_cmp(state, WIN_FN)) {
        bt_indexes_dirty = true;
    }
#    ifdef BT_APPLE_FN
    // Stock: in Mac mode the Fn key also sets the Apple Fn (Globe) byte of the keyboard
    // report. MAC_FN is only reachable from the Mac layer. No-op while the driver is off.
    iton_bt_set_apple_fn(flags.dip_switch && layer_state_cmp(state, MAC_FN));
#    endif
    return layer_state_set_user(state);
}
#endif

#ifdef BLUETOOTH_ITON_BT
static void kb_set_forced_dark(bool dark) {
#    if defined(RGB_MATRIX_ENABLE) && defined(RGB_MATRIX_SLEEP)
    static bool forced = false;
    if (dark == forced) return;
    forced = dark;
    rgb_matrix_set_suspend_state(dark); // without RGB_MATRIX_SLEEP this does nothing
#    else
    (void)dark;
#    endif
}

#    ifdef BT_STATUS_LED_PIN
// Stock: a separate LED on P1.12 (B12), lit for the first 2 s after power-up and blinking
// 1 s on / 1 s off while the module reports low battery; off while the MCU sleeps.
// What it is physically is not known. Active high assumed (the stock clears it before
// sleeping).
static void bt_status_led_task(bool sleeping) {
    static bool ready = false;
    if (!ready) {
        gpio_set_pin_output(BT_STATUS_LED_PIN);
        ready = true;
    }
    uint32_t now = timer_read32();
    bool     on  = false;
    if (now < 2000) {
        on = true;
    } else if (bt_low_battery && flags.dip_switch && !sleeping) {
        on = (now / 1000) & 1;
    }
    gpio_write_pin(BT_STATUS_LED_PIN, on);
}
#    endif
#endif

#ifdef KB_IDLE_SLEEP
// The board never sleeps on its own, in either mode: BLUETOOTH_ENABLE sets
// NO_USB_STARTUP_CHECK, which compiles out the USB suspend loop (the only place QMK
// pauses the main loop), so the loop spins at 100 % all the time. Pause it while there
// is nothing to do, in USB and BT mode, on battery or USB power alike; any key press
// ends that within a few passes.
static uint8_t kb_sleep_check(void) {
    uint32_t idle = last_input_activity_elapsed();
#    ifdef BLUETOOTH_ITON_BT
    // The module asked the MCU to sleep (batt_mcu_sleep): key presses do not keep it awake.
    bool hold = bt_module_hold;
#    else
    bool hold = false;
#    endif
    if (!hold && idle < KB_IDLE_SLEEP_IDLE_MS) return KB_SLEEP_ACTIVE;
#    ifdef RGB_MATRIX_ENABLE
    // Animations and BT indicators are rendered by the main loop: wait for the
    // backlight timeout (plus the margin, for the frame that turns everything off).
    if (rgb_matrix_is_enabled() && !rgb_matrix_get_suspend_state()) {
#        if RGB_MATRIX_TIMEOUT > 0
        if (idle < (uint32_t)RGB_MATRIX_TIMEOUT + KB_IDLE_SLEEP_IDLE_MS) return KB_SLEEP_LIT;
#        else
        return KB_SLEEP_LIT; // the backlight never times out
#        endif
    }
#    endif
#    ifdef BLUETOOTH_ITON_BT
    if (bt_cmdq.count || bt_cmdq.deinit_after || bt_pair_tries || bt_evt_connected || bt_evt_pairing) return KB_SLEEP_BT_CFG;
    if (iton_bt_task_pending()) return KB_SLEEP_BT_PENDING; // never in USB mode: the driver is stopped
#    endif
    return KB_SLEEP_OK;
}
#endif

#if defined(BLUETOOTH_ITON_BT) || defined(DEFERRED_EEPROM_SAVE) || defined(KB_IDLE_SLEEP)
void housekeeping_task_kb(void) {
#ifdef BLUETOOTH_ITON_BT
    if (bt_indexes_dirty) {
        bt_indexes_dirty = false;
        bt_recalc_matrix_indexes();
    }
    iton_bt_task();

    if (bt_evt_pairing) {
        bt_evt_pairing  = false;
        bt_pairing      = true;
        bt_pairing_time = timer_read32();
    }
    if (bt_evt_connected) {
        bt_evt_connected = false;
        bt_pairing       = false;
#    ifdef BT_MODULE_SLEEP_IDLE
        if (flags.dip_switch) bt_module_sleep_setup();
#    endif
    }
    if (bt_pairing && timer_elapsed32(bt_pairing_time) > BT_PAIRING_WINDOW_MS) bt_pairing = false;

    // Pairing retries until the module reports pairing mode.
    if (bt_pair_tries) {
        if (bt_pairing || bt_pair_tries >= BT_PAIR_TRIES) {
            bt_pair_tries = 0;
        } else if (!bt_cmd_waiting(control_bt, enter_pairing) && timer_elapsed32(bt_cmdq.last_sent) >= BT_PAIR_RETRY_MS) {
            bt_cmd_push(control_bt, enter_pairing);
            bt_pair_tries++;
        }
    }

    bt_cmd_task();
    if (bt_cmdq.deinit_after && !bt_cmdq.count) {
        iton_bt_flush(); // let mode_usb reach the module before the bus stops
        iton_bt_deinit();
        bt_cmdq.deinit_after = false;
    }

    // Backlight forced off through the rgb_matrix suspend state, as the stock does:
    // - the module asked the MCU to sleep (batt_mcu_sleep), until it sends anything;
    // - BT mode, no connection, not pairing and no key press for BT_DISCONNECTED_DARK_MS
    //   (the stock sleeps 40 s after a disconnect or a power-up without connection).
    bool dark = bt_module_hold;
#    ifdef BT_DISCONNECTED_DARK_MS
    if (flags.dip_switch && !iton_bt_is_connected && !bt_pairing && last_input_activity_elapsed() >= BT_DISCONNECTED_DARK_MS) dark = true;
#    endif
    kb_set_forced_dark(dark);
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
#    ifdef BLUETOOTH_ITON_BT
    if (bt_battery_critical) {
        bt_battery_critical = false;
        eeprom_flush_pending();
    }
#    endif
#endif
#ifdef KB_IDLE_SLEEP
    uint8_t sleep_block = kb_sleep_check();
#endif
#if defined(BLUETOOTH_ITON_BT) && defined(BT_STATUS_LED_PIN)
#    ifdef KB_IDLE_SLEEP
    bt_status_led_task(sleep_block == KB_SLEEP_OK);
#    else
    bt_status_led_task(false);
#    endif
#endif
#ifdef KB_IDLE_SLEEP
    if (sleep_block == KB_SLEEP_OK) {
#    ifdef K3_DIAG
        uint32_t pause_start = timer_read32();
#    endif
        // The main thread pauses, ChibiOS runs its idle thread, and the SN32 idle hook
        // puts the core to sleep until the next interrupt (LED/key scan timer every
        // ~0.46 ms, BT module INT, USB, the end of this pause).
        wait_ms(KB_IDLE_SLEEP_TICK_MS);
#    ifdef K3_DIAG
        diag_sleep_paused_for(pause_start, timer_elapsed32(pause_start));
#    endif
    }
#    ifdef K3_DIAG
    diag_sleep_note(sleep_block);
    diag_sleep_check_wake();
#    endif
#endif
}
#endif

bool dip_switch_update_user(uint8_t index, bool active) {
    switch (index) {
        case 1:
            layer_move(active ? MAC_BASE : WIN_BASE);
#ifdef BLUETOOTH_ITON_BT
            bt_indexes_dirty = true;
#endif
            return false;
#ifdef BLUETOOTH_ITON_BT
        case 0:
            // dip switch is inactive in bt state
            if (!active) {
                set_output(OUTPUT_BLUETOOTH);
                iton_bt_init();
                bt_profile_load();
                bt_cmd_clear();
                // Stock: on every switch to BT, power-up included (after its 2 s delay),
                // select the saved profile again.
                bt_cmd_push(control_bt, switch_profile + bt_profile);
#    ifdef BT_MODULE_SLEEP_IDLE
                bt_module_sleep_setup();
#    endif
            } else {
                bt_cmd_clear();
                // Stock: tell the module mode_usb before starting USB. At power-up the
                // driver is not running yet; keyboard_post_init_kb() queues it instead.
                if (iton_bt_running()) {
                    while (bt_cmdq.sent_once && timer_elapsed32(bt_cmdq.last_sent) < BT_MODULE_CMD_GAP_MS) {
                        wait_ms(1);
                    }
                    iton_bt_mode_usb();
                    bt_cmd_mark_sent();
                    iton_bt_flush();
                }
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
