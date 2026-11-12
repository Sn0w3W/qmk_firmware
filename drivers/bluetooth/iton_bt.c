// Copyright 2024 1Conan (@1Conan) <me@1conan.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string.h>
#include <hal.h>
#include "gpio.h"
#include "config.h"
#include "iton_bt.h"
#include "timer.h"

#ifndef ITON_BT_SPI_PORT
#    define ITON_BT_SPI_PORT SPID0
#endif

#ifndef ITON_BT_IRQ_LINE
#    define ITON_BT_IRQ_LINE A0
#endif

#ifndef ITON_BT_INT_LINE
#    define ITON_BT_INT_LINE A1
#endif

#ifndef ITON_BT_BUFFER_LEN
#    define ITON_BT_BUFFER_LEN 16
#endif

// How long to wait for the module to clock out the previous frame before the
// transfer is treated as stuck and reset. Generous on purpose: a module that is
// waking up can take a while, and the old code waited forever.
#ifndef ITON_BT_TX_TIMEOUT_MS
#    define ITON_BT_TX_TIMEOUT_MS 3000
#endif

/**
 * Driver Macros
 */
#define HIGH_BITS(x) ((uint8_t)(x >> 8))
#define LOW_BITS(x) ((uint8_t)(x & 0x00FF))

/**
 * Function definitions
 */
void iton_bt_data_cb(SPIDriver *spip);

/**
 * Callbacks
 */
__attribute__((weak)) void iton_bt_battery_voltage_low(void) {}
__attribute__((weak)) void iton_bt_battery_exit_low_battery_mode(void) {}
__attribute__((weak)) void iton_bt_battery_low_power_shutdown(void) {}
__attribute__((weak)) void iton_bt_battery_level(uint8_t level) {}

__attribute__((weak)) void iton_bt_connection_successful(void) {}
__attribute__((weak)) void iton_bt_entered_pairing(void) {}
__attribute__((weak)) void iton_bt_disconnected(void) {}
__attribute__((weak)) void iton_bt_enters_connection_state(void) {}

/**
 * Driver variables
 */
bool iton_bt_is_connected = false;
#ifdef ITON_BT_ENABLE_LED_STATE
volatile uint8_t iton_bt_led_state = 0x00;
#endif
volatile iton_bt_stats_t iton_bt_stats = {0};

// RX and TX never share a buffer: the module can start a transfer (INT) at any
// time, and the main loop must be able to keep a frame for a retry.
static uint8_t iton_bt_rx_buffer[ITON_BT_BUFFER_LEN];
static uint8_t iton_bt_tx_buffer[ITON_BT_BUFFER_LEN];
uint8_t        iton_bt_send_kb_last_key = 0x00;

static bool iton_bt_initialized = false;

// Bus state, shared between the main loop and the ISRs.
// Written only from ISRs or under chSysLock().
static volatile uint8_t iton_bt_tx_len    = 0;     // length of the frame in iton_bt_tx_buffer
static volatile bool    iton_bt_tx_active = false; // that frame is armed and not completed yet
static volatile bool    iton_bt_tx_retry  = false; // that frame was cut off by an incoming packet

#ifdef ITON_BT_ENABLE_ACK
#    define ITON_BT_ACK_QUEUE_SIZE 4
static uint8_t          iton_bt_ack_buffer[3];
static volatile bool    iton_bt_ack_active = false;
static volatile struct {
    uint8_t b1[ITON_BT_ACK_QUEUE_SIZE];
    uint8_t b2[ITON_BT_ACK_QUEUE_SIZE];
    uint8_t head;
    uint8_t count;
} iton_bt_ack_queue = {0};
#endif

const SPIConfig iton_bt_spicfg = {
    .slave   = true,
    .data_cb = iton_bt_data_cb,
#if defined(MCU_SN32)
    .ctrl0 = SPI_DATA_LENGTH(8),
#endif
};

/**
 * Bus helpers. All of them must be called with the system locked
 * (chSysLock() from the main loop, chSysLockFromISR() from an ISR).
 */
static inline bool iton_bt_bus_idle_locked(void) {
    return ITON_BT_SPI_PORT.state == SPI_READY && !iton_bt_tx_active
#ifdef ITON_BT_ENABLE_ACK
           && !iton_bt_ack_active
#endif
           && !gpio_read_pin(ITON_BT_IRQ_LINE);
}

// An incoming packet (INT) cuts off whatever we were sending. Remember it so the
// main loop can send it again instead of silently losing a key report.
static inline void iton_bt_bus_abort_locked(void) {
    if (iton_bt_tx_active) {
        iton_bt_tx_active = false;
        iton_bt_tx_retry  = true;
        iton_bt_stats.tx_aborted++;
    }
#ifdef ITON_BT_ENABLE_ACK
    if (iton_bt_ack_active) {
        iton_bt_ack_active = false;
        if (iton_bt_ack_queue.count < ITON_BT_ACK_QUEUE_SIZE) {
            uint8_t idx               = (iton_bt_ack_queue.head + iton_bt_ack_queue.count) % ITON_BT_ACK_QUEUE_SIZE;
            iton_bt_ack_queue.b1[idx] = iton_bt_ack_buffer[1];
            iton_bt_ack_queue.b2[idx] = iton_bt_ack_buffer[2];
            iton_bt_ack_queue.count++;
        } else {
            iton_bt_stats.ack_dropped++;
        }
    }
#endif
    if (ITON_BT_SPI_PORT.state != SPI_READY) {
        spiStopTransferI(&ITON_BT_SPI_PORT, NULL);
    }
}

// Arm the SPI transfer first, then raise IRQ, so the module never clocks an empty FIFO.
static inline void iton_bt_tx_start_locked(uint8_t len) {
    iton_bt_tx_len    = len;
    iton_bt_tx_retry  = false;
    iton_bt_tx_active = true;
    spiStartSendI(&ITON_BT_SPI_PORT, len, iton_bt_tx_buffer);
    gpio_write_pin_high(ITON_BT_IRQ_LINE);
}

#ifdef ITON_BT_ENABLE_ACK
static inline void iton_bt_ack_start_locked(uint8_t b1, uint8_t b2) {
    iton_bt_ack_buffer[0] = control;
    iton_bt_ack_buffer[1] = b1;
    iton_bt_ack_buffer[2] = b2;
    iton_bt_ack_active    = true;
    spiStartSendI(&ITON_BT_SPI_PORT, 3, iton_bt_ack_buffer);
    gpio_write_pin_high(ITON_BT_IRQ_LINE);
}
#endif

/**
 * Callbacks
 */
#if defined(PAL_USE_CALLBACKS) || defined(PAL_USE_WAIT)
static inline void iton_bt_rx_battery_notif(uint8_t data) {
    switch (data) {
        case batt_voltage_low:
            iton_bt_battery_voltage_low();
            break;
        case batt_exit_low_battery_mode:
            iton_bt_battery_exit_low_battery_mode();
            break;
        case batt_low_power_shutdown:
            iton_bt_battery_low_power_shutdown();
            break;
        case batt_above_70:
        case batt_between_30_70:
        case batt_below_30:
            iton_bt_battery_level(data);
            break;
        case batt_wake_mcu:
#    ifdef ITON_BT_ENABLE_ACK
            iton_bt_send_ack(control_bt, wake_ack);
#    endif
            break;
        case batt_unknown:
#    ifdef ITON_BT_ENABLE_ACK
            iton_bt_send_ack(control_bt, unknown_ack);
#    endif
            break;
        case query_working_mode:
            break;
        case query_bt_name:
            break;
    }
}

static inline void iton_bt_rx_bluetooth_notif(uint8_t data) {
    switch (data) {
        case bt_connection_success:
            iton_bt_is_connected = true;

#    ifdef ITON_BT_ENABLE_ACK
            iton_bt_send_ack(control_bt, connect_ack);
#    endif

            iton_bt_connection_successful();
            break;
        case bt_entered_pairing:
            iton_bt_entered_pairing();
            break;
        case bt_disconected:
            iton_bt_is_connected = false;

#    ifdef ITON_BT_ENABLE_ACK
            iton_bt_send_ack(control_bt, disconnect_ack);
#    endif

            iton_bt_disconnected();
            break;
        case bt_enters_connection:
            iton_bt_enters_connection_state();
            break;
    }
}

static void iton_bt_rx_cb(void *arg) {
    if (gpio_read_pin(ITON_BT_INT_LINE)) {
        chSysLockFromISR();
        iton_bt_bus_abort_locked();
        spiStartReceiveI(&ITON_BT_SPI_PORT, ITON_BT_BUFFER_LEN, &iton_bt_rx_buffer[0]);
        chSysUnlockFromISR();
    } else {
        chSysLockFromISR();
        iton_bt_bus_abort_locked();
        chSysUnlockFromISR();

#    ifdef ITON_BT_ENABLE_ACK
        // hack to make sure irq is low since acks messes with stuff
        gpio_write_pin_low(ITON_BT_IRQ_LINE);
#    endif

        switch (iton_bt_rx_buffer[0]) {
            case led_state:
#    ifdef ITON_BT_ENABLE_LED_STATE
                iton_bt_led_state = iton_bt_rx_buffer[1];
#    endif
                break;
            case notification:
                switch (iton_bt_rx_buffer[1]) {
                    case notif_battery:
                        iton_bt_rx_battery_notif(iton_bt_rx_buffer[2]);
                        break;
                    case notif_bluetooth:
                        iton_bt_rx_bluetooth_notif(iton_bt_rx_buffer[2]);
                        break;
                }
                break;
        }
        // A falling edge without a fresh packet must not replay the previous one.
        iton_bt_rx_buffer[0] = 0;
    }
}
#endif

void iton_bt_data_cb(SPIDriver *spip) {
    iton_bt_tx_active = false;
#ifdef ITON_BT_ENABLE_ACK
    iton_bt_ack_active = false;
#endif
    gpio_write_pin_low(ITON_BT_IRQ_LINE);
}

/**
 * Driver Functions
 */
void iton_bt_init(void) {
    if (iton_bt_initialized) return;

    iton_bt_tx_len           = 0;
    iton_bt_tx_active        = false;
    iton_bt_tx_retry         = false;
    iton_bt_send_kb_last_key = 0x00;
#ifdef ITON_BT_ENABLE_ACK
    iton_bt_ack_active      = false;
    iton_bt_ack_queue.head  = 0;
    iton_bt_ack_queue.count = 0;
#endif

    gpio_set_pin_output(ITON_BT_IRQ_LINE);
    gpio_set_pin_input(ITON_BT_INT_LINE);

    gpio_write_pin_low(ITON_BT_IRQ_LINE);

    spiStart(&ITON_BT_SPI_PORT, &iton_bt_spicfg);

#if defined(PAL_USE_CALLBACKS) || defined(PAL_USE_WAIT)
    palSetLineCallback(ITON_BT_INT_LINE, iton_bt_rx_cb, NULL);
    palEnableLineEvent(ITON_BT_INT_LINE, PAL_EVENT_MODE_BOTH_EDGES);
#endif

    iton_bt_initialized = true;
}

void iton_bt_deinit(void) {
    if (!iton_bt_initialized) return;
    iton_bt_initialized = false;

#if defined(PAL_USE_CALLBACKS) || defined(PAL_USE_WAIT)
    palSetLineCallback(ITON_BT_INT_LINE, NULL, NULL);
    palDisableLineEvent(ITON_BT_INT_LINE);
#endif

    spiStop(&ITON_BT_SPI_PORT);

    chSysLock();
    iton_bt_tx_active = false;
    iton_bt_tx_retry  = false;
#ifdef ITON_BT_ENABLE_ACK
    iton_bt_ack_active      = false;
    iton_bt_ack_queue.count = 0;
#endif
    chSysUnlock();
    iton_bt_is_connected = false;

    gpio_set_pin_input(ITON_BT_IRQ_LINE);
    gpio_set_pin_input(ITON_BT_INT_LINE);
}

/**
 * Waits until the bus is free and returns with the system LOCKED.
 * The previous transfer is normally clocked out by the module within milliseconds,
 * but after ITON_BT_TX_TIMEOUT_MS it is considered stuck: the SPI transfer is
 * stopped and IRQ released, instead of spinning forever (the old hard freeze).
 * Returns false (system NOT locked) when the driver is not running.
 */
static bool iton_bt_bus_acquire(void) {
    if (!iton_bt_initialized) return false;

    uint32_t start = timer_read32();
    for (;;) {
        bool timed_out = timer_elapsed32(start) > ITON_BT_TX_TIMEOUT_MS;
        chSysLock();
        if (iton_bt_bus_idle_locked()) return true;
        if (timed_out) {
            iton_bt_stats.tx_timeouts++;
            if (ITON_BT_SPI_PORT.state != SPI_READY) {
                spiStopTransferI(&ITON_BT_SPI_PORT, NULL);
            }
            iton_bt_tx_active = false;
#ifdef ITON_BT_ENABLE_ACK
            iton_bt_ack_active = false;
#endif
            gpio_write_pin_low(ITON_BT_IRQ_LINE);
            return true;
        }
        chSysUnlock();
    }
}

void iton_bt_send(uint8_t cmd, uint8_t *data, uint8_t len) {
    if (len > ITON_BT_BUFFER_LEN - 1) return;
    if (!iton_bt_bus_acquire()) return;
    iton_bt_tx_buffer[0] = cmd;
    memcpy(&iton_bt_tx_buffer[1], data, len);
    iton_bt_tx_start_locked(len + 1);
    chSysUnlock();
}

void iton_bt_send2(uint8_t cmd, uint8_t b1, uint8_t b2) {
    if (!iton_bt_bus_acquire()) return;
    iton_bt_tx_buffer[0] = cmd;
    iton_bt_tx_buffer[1] = b1;
    iton_bt_tx_buffer[2] = b2;
    iton_bt_tx_start_locked(3);
    chSysUnlock();
}

// Called from the INT ISR right after the incoming packet was parsed.
void iton_bt_send_ack(uint8_t b1, uint8_t b2) {
#ifdef ITON_BT_ENABLE_ACK
    chSysLockFromISR();
    if (iton_bt_bus_idle_locked()) {
        // The bus is free (the packet that triggered the ACK has just ended): reply at once.
        iton_bt_ack_start_locked(b1, b2);
    } else if (iton_bt_ack_queue.count < ITON_BT_ACK_QUEUE_SIZE) {
        uint8_t idx               = (iton_bt_ack_queue.head + iton_bt_ack_queue.count) % ITON_BT_ACK_QUEUE_SIZE;
        iton_bt_ack_queue.b1[idx] = b1;
        iton_bt_ack_queue.b2[idx] = b2;
        iton_bt_ack_queue.count++;
    } else {
        iton_bt_stats.ack_dropped++;
    }
    chSysUnlockFromISR();
#endif
}

// Main loop: sends ACKs that could not go out from the ISR and retries a frame
// that an incoming packet cut off.
void iton_bt_task(void) {
    if (!iton_bt_initialized) return;

#ifdef ITON_BT_ENABLE_ACK
    while (iton_bt_ack_queue.count > 0) {
        if (!iton_bt_bus_acquire()) return;
        if (iton_bt_ack_queue.count > 0) {
            uint8_t b1             = iton_bt_ack_queue.b1[iton_bt_ack_queue.head];
            uint8_t b2             = iton_bt_ack_queue.b2[iton_bt_ack_queue.head];
            iton_bt_ack_queue.head = (iton_bt_ack_queue.head + 1) % ITON_BT_ACK_QUEUE_SIZE;
            iton_bt_ack_queue.count--;
            iton_bt_ack_start_locked(b1, b2);
        }
        chSysUnlock();
    }
#endif

    if (iton_bt_tx_retry) {
        if (!iton_bt_bus_acquire()) return;
        // Only if no newer frame has replaced it in the meantime.
        if (iton_bt_tx_retry) {
            iton_bt_stats.tx_retried++;
            iton_bt_tx_start_locked(iton_bt_tx_len);
        }
        chSysUnlock();
    }
}

void iton_bt_send_fn(bool pressed) {
    uint8_t data = pressed ? 0xA3 : 0x00;

    iton_bt_send(report_fn, &data, 1);
}

void iton_bt_send_system(uint16_t data) {
    iton_bt_send(report_system, (uint8_t *)&data, 1);
}

void iton_bt_send_consumer(uint16_t data) {
    iton_bt_send2(report_consumer, HIGH_BITS(data), LOW_BITS(data));
}

void iton_bt_send_keyboard(report_keyboard_t *report) {
    // iton module only reads 5 of the keys in the hid report
    // so we send the last key as an nkro report.
    if (report->keys[5] != iton_bt_send_kb_last_key) {
        uint8_t nkro_report[15] = {0};
        iton_bt_send_kb_last_key = report->keys[5];

        // The 15-byte bitmap covers keycodes 0x00-0x77 only; anything above that
        // cannot be expressed here, so fall through to the plain HID report.
        if ((report->keys[5] >> 3) < sizeof(nkro_report)) {
            nkro_report[report->keys[5] >> 3] |= (1 << (report->keys[5] & 7));
            return iton_bt_send(report_nkro, &nkro_report[0], 15);
        }
    }

    iton_bt_send(report_hid, &report->mods, 8);
}
