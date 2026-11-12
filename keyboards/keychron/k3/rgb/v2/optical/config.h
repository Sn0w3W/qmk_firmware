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

#ifdef BLUETOOTH_ITON_BT
    // Replies to the module as the stock K3 firmware (V1.17) does: only control_bt
    // wake_ack, about 10 ms after the module woke the MCU. ITON_BT_ENABLE_ACK (enabled
    // since 9aea594458, "Fixing random disconnects") also answers connect, disconnect
    // and 0x08 with 0x50/0x51/0x52, which the stock never sends. To go back to it,
    // replace the next line with #define ITON_BT_ENABLE_ACK.
    #define ITON_BT_WAKE_ACK
    // How long a new frame waits for the module to clock out the previous one before
    // dropping it. The stock gives up after 100 ms; the driver default of 3 s blocks
    // the main loop (keys, LEDs) that long whenever the module does not respond.
    #define ITON_BT_TX_TIMEOUT_MS 100
    #define ITON_BT_ENABLE_LED_STATE
    // Module sleep, as on the stock K3: after this long without key presses the module
    // drops the link and sleeps; the next key press wakes it and it reconnects.
    // sleep_idle_10m, sleep_idle_20m or sleep_idle_30m. Sent with enable_sleep on every
    // switch to BT mode and every connection. Comment out to send nothing to the module.
    #define BT_MODULE_SLEEP_IDLE sleep_idle_10m
    // Like the stock: in BT mode without a connection (and not pairing), the backlight
    // goes off after this long without a key press, and the MCU sleep follows. The stock
    // uses 40 s after a disconnect or a power-up. Comment out to keep only the normal
    // backlight timeout.
    #define BT_DISCONNECTED_DARK_MS 40000
    // Separate LED the stock drives on P1.12: on for 2 s after power-up, blinking while
    // the battery is low. What it lights on this board is not known. Comment out to leave
    // B12 untouched.
    #define BT_STATUS_LED_PIN B12
    // Like the stock, send the Fn key as the Apple Fn (Globe) byte in Mac mode over BT.
    // macOS then sees Globe held with Fn: Globe shortcuts, and a lone Fn press triggers
    // the "Press Globe key to" action (input source, emoji, dictation). Off for now.
    // #define BT_APPLE_FN
#endif

// MCU sleep in USB and BT mode, on battery or USB power alike: once the backlight is dark
// and no key was pressed for KB_IDLE_SLEEP_IDLE_MS (5 s), the main loop pauses
// KB_IDLE_SLEEP_TICK_MS (5 ms) per pass and the core sleeps between interrupts. Key
// scanning keeps running, so a key press wakes it. Comment out to keep the main loop
// running at full speed.
#define KB_IDLE_SLEEP
