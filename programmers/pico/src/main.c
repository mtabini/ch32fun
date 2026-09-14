/**
 * @file main.c
 * @brief Dual CDC programmer and UART bridge support.
 * @copyright Copyright © 2026 MTA, Inc.
 */
#include <stdint.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"
#include "pico/bootrom.h"
#include "hardware/uart.h"
#include "tusb.h"
#include "wch_rvswd.h"

// Core 0 exclusively owns USB and RVSWD; core 1 exclusively owns UART0.
// Pico SDK queues synchronize access without sharing TinyUSB across cores.
static queue_t uart_rx, uart_tx, uart_settings;
static uint8_t request[6];
static unsigned request_size;
static uint32_t programmer_baud = 115200;

/**
 * @brief Service bounded UART work on core 1; queues are the only shared state.
 */
static void uart_task(void) {
    static cdc_line_coding_t pending;
    static bool has_pending;
    cdc_line_coding_t next;
    if (queue_try_remove(&uart_settings, &next)) {
        pending = next;
        has_pending = true;
    }
    if (has_pending && !(uart_get_hw(uart0)->fr & UART_UARTFR_BUSY_BITS)) {
        uart_set_baudrate(uart0, pending.bit_rate);
        uart_set_format(uart0, pending.data_bits, pending.stop_bits == 2 ? 2 : 1,
                        pending.parity == 1 ? UART_PARITY_ODD :
                        pending.parity == 2 ? UART_PARITY_EVEN : UART_PARITY_NONE);
        has_pending = false;
    }
    // Bounded batches preserve fairness in both directions.
    for (unsigned i = 0; i < 64 && uart_is_readable(uart0); ++i) {
        uint8_t byte = (uint8_t)uart_getc(uart0);
        // No RTS/CTS: discard newest byte if the host cannot keep up.
        (void)queue_try_add(&uart_rx, &byte);
    }
    for (unsigned i = 0; i < 64 && !has_pending && uart_is_writable(uart0); ++i) {
        uint8_t byte;
        if (!queue_try_remove(&uart_tx, &byte)) break;
        uart_putc_raw(uart0, byte);
    }
}

/**
 * @brief Initialize UART0 and continuously service it independently of RVSWD.
 */
static void uart_core(void) {
    uart_init(uart0, 115200);
    gpio_set_function(WCH_UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(WCH_UART_RX_PIN, GPIO_FUNC_UART);
    gpio_pull_up(WCH_UART_RX_PIN);
    uart_set_hw_flow(uart0, false, false);
    uart_set_format(uart0, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(uart0, true);
    while (true) {
        uart_task();
        tight_loop_contents();
    }
}

/**
 * @brief Queue supported UART settings, or retain programmer-only baud for BOOTSEL.
 */
void tud_cdc_line_coding_cb(uint8_t interface, const cdc_line_coding_t *coding) {
    if (interface == 0) {
        programmer_baud = coding->bit_rate;
        return;
    }
    // UART0 supports 5-8 bits, no/odd/even parity and one/two stop bits.
    // Unsupported coding leaves the previous hardware settings in force.
    if (coding->bit_rate < 300 || coding->bit_rate > 1000000 ||
        coding->data_bits < 5 || coding->data_bits > 8 ||
        coding->parity > 2 || (coding->stop_bits != 0 && coding->stop_bits != 2)) return;
    cdc_line_coding_t discarded;
    while (queue_try_remove(&uart_settings, &discarded)) {}
    (void)queue_try_add(&uart_settings, coding);
}

/**
 * @brief Discard partial programmer frames on close; UART DTR never requests reboot.
 */
void tud_cdc_line_state_cb(uint8_t interface, bool dtr, bool rts) {
    (void)rts;
    if (interface == 0 && !dtr) {
        request_size = 0;
        // Preserve Pico SDK's programming-port 1200-baud BOOTSEL workflow.
        if (programmer_baud == 1200) reset_usb_boot(0, 0);
    }
}

/**
 * @brief Drop partial programmer framing when USB disconnects.
 */
void tud_umount_cb(void) {
    request_size = 0;
}

/**
 * @brief Service at most one Ardulink command without waiting for missing bytes.
 */
static void programmer_task(void) {
    // Reserve enough space for the largest reply before consuming a command.
    if (tud_cdc_n_write_available(0) < 4) return;
    while (tud_cdc_n_available(0) && request_size < sizeof(request)) {
        tud_cdc_n_read(0, request + request_size, 1);
        ++request_size;
        unsigned needed = request[0] == 'w' ? 6 : request[0] == 'r' ? 2 : 1;
        if (request_size < needed) continue;
        uint8_t reply[4] = {'+'};
        unsigned length = 1;
        switch (request[0]) {
        case '?': case 'P': break;
        case 'p': wch_rvswd_init(WCH_RVSWD_DIO_PIN, WCH_RVSWD_DCK_PIN); break;
        case 'w': {
            uint32_t value = 0;
            for (unsigned i = 0; i < 4; ++i) value |= (uint32_t)request[i + 2] << (8 * i);
            wch_rvswd_write_reg(request[1], value);
            break;
        }
        case 'r': {
            uint32_t value;
            if (!wch_rvswd_read_reg(request[1], &value)) value = UINT32_MAX;
            for (unsigned i = 0; i < 4; ++i) reply[i] = (uint8_t)(value >> (8 * i));
            length = 4;
            break;
        }
        default: length = 0; break;
        }
        request_size = 0;
        if (length) tud_cdc_n_write(0, reply, length);
        tud_cdc_n_write_flush(0);
        break;
    }
}

/**
 * @brief Move bounded byte batches between CDC1 and synchronized UART queues.
 */
static void bridge_task(void) {
    // Apply backpressure toward the computer when UART TX is full.
    for (unsigned i = 0; i < 128 && tud_cdc_n_available(1) && !queue_is_full(&uart_tx); ++i) {
        uint8_t byte;
        tud_cdc_n_read(1, &byte, 1);
        queue_try_add(&uart_tx, &byte);
    }
    bool connected = tud_cdc_n_connected(1);
    for (unsigned i = 0; i < 128; ++i) {
        if (connected && !tud_cdc_n_write_available(1)) break;
        uint8_t byte;
        if (!queue_try_remove(&uart_rx, &byte)) break;
        if (connected) tud_cdc_n_write(1, &byte, 1);
    }
    tud_cdc_n_write_flush(1);
}

/**
 * @brief Initialize queues before launching core 1, then exclusively service USB on core 0.
 */
int main(void) {
    queue_init(&uart_rx, sizeof(uint8_t), 4096);
    queue_init(&uart_tx, sizeof(uint8_t), 512);
    queue_init(&uart_settings, sizeof(cdc_line_coding_t), 1);
    wch_rvswd_init(WCH_RVSWD_DIO_PIN, WCH_RVSWD_DCK_PIN);
    multicore_launch_core1(uart_core);
    tusb_init();
    while (true) {
        tud_task();
        programmer_task();
        bridge_task();
    }
}
