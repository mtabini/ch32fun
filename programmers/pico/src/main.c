#include <stdint.h>

#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "wch_rvswd.h"

#ifndef WCH_RVSWD_DIO_PIN
#define WCH_RVSWD_DIO_PIN 2
#endif

#ifndef WCH_RVSWD_DCK_PIN
#define WCH_RVSWD_DCK_PIN 3
#endif

static int read_byte(void) {
    int value;
    do {
        value = getchar_timeout_us(1000);
        tight_loop_contents();
    } while (value == PICO_ERROR_TIMEOUT);
    return value;
}

static uint32_t read_u32_le(void) {
    uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) value |= (uint32_t)(uint8_t)read_byte() << (8 * i);
    return value;
}

static void write_u32_le(uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) putchar_raw((int)(uint8_t)(value >> (8 * i)));
}

int main(void) {
    stdio_init_all();
    wch_rvswd_init(WCH_RVSWD_DIO_PIN, WCH_RVSWD_DCK_PIN);

    while (true) {
        int command = getchar_timeout_us(1000);
        if (command == PICO_ERROR_TIMEOUT) {
            tight_loop_contents();
            continue;
        }

        switch (command) {
        case '?':
            putchar_raw('+');
            break;
        case 'p':
            wch_rvswd_init(WCH_RVSWD_DIO_PIN, WCH_RVSWD_DCK_PIN);
            putchar_raw('+');
            break;
        case 'P':
            putchar_raw('+');
            break;
        case 'w': {
            uint8_t reg = (uint8_t)read_byte();
            uint32_t value = read_u32_le();
            wch_rvswd_write_reg(reg, value);
            putchar_raw('+');
            break;
        }
        case 'r': {
            uint8_t reg = (uint8_t)read_byte();
            uint32_t value = 0;
            if (!wch_rvswd_read_reg(reg, &value)) value = 0xffffffffu;
            write_u32_le(value);
            break;
        }
        default:
            break;
        }
    }
}
