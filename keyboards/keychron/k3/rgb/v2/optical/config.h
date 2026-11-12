/* Copyright 2022 Philip Mourdjis <philip.j.m@gmail.com>
 * Copyright 2023 KeijoMika (https://github.com/KeijoMika)
 * Copyright 2023 Santanu Paik (https://github.com/ITNerd69)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#define SN32F2XX_RGB_MATRIX_ROW_PINS { C3, C1, C0, C6, C5, C4, C9, C8, C7, C12, C11, C10, B13, C14, C13, B14, B15, D3 }

// QMK now limits the System Control usage range to 0x01-0x8F (#26295).
// KC_DNDB sends 0x9B (System Do Not Disturb), so keep the previous range.
#define SYSTEM_CONTROL_USAGE_MAXIMUM 0x00B7

// 4th row shows red/purple instead of the chosen colour when few LEDs are lit
// (buglist BUG-004). Both workarounds are off: default LED behaviour. Enable the
// one that matches the hardware test result:
// - pink only at low brightness / during fades (short pulses): min duty for lit channels,
//   unlit LEDs stay dark.
// #define SN32F2XX_LED_MIN_ON_DUTY 8
// - pink at any brightness when few LEDs are lit (light load): while anything is lit,
//   every LED gets this duty (faint glow on unlit keys while awake, dark in sleep).
// #define SN32F2XX_LED_MIN_LOAD_DUTY 1

#ifdef BLUETOOTH_ITON_BT
    #define ITON_BT_ENABLE_ACK
    #define ITON_BT_ENABLE_LED_STATE
    // Keep the BT link while the host is connected (disable_sleep on connect,
    // enable_sleep on disconnect). Without it the module drops the link on its
    // own idle timer (stock K3: auto sleep after 10 min in BT mode).
    // Comment out to get the stock module behaviour back.
    #define BT_KEEP_CONNECTION
#endif
