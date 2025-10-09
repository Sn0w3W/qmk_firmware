/* Copyright 2023 KeijoMika (https://github.com/KeijoMika)
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

#define RGB_MATRIX_LED_COUNT 85

// Measures to cope with phantom column/key noise.
// Uncomment the lines below if the ` or Tab keys press themselves in Bluetooth mode.

// 20–50 is a reasonable range. Increase this value if the keyboard both spams and stops working when RGB is turned off.
// #define MATRIX_IO_DELAY 50

// 20–50 is a reasonable range. Increase this value if the keyboard both spams and stops working when RGB is turned off.
// #define MATRIX_UNSELECT_DRIVE_DELAY 50

// 1000–2000 is generally optimal based on testing.
// #define MATRIX_SCAN_RATE 1000

// Per-key debounce type. If you change the debounce mode, you may need to retune the delay value below.
// #define DEBOUNCE_TYPE sym_defer_pk

// Debounce time in milliseconds. Increasing this value too much may cause the keyboard
// to stop responding at low RGB brightness or fail to register keypresses in Bluetooth mode.
// #define DEBOUNCE 8
