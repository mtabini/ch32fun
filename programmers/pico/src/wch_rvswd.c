#include "wch_rvswd.h"

#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"
#include "wch_rvswd.pio.h"

// Two-wire transaction sequence derived from bitbang_rvswdio.h in CNLohr's
// esp32s2-cookbook. Original implementation copyright Charles Lohr, available
// under MIT/X11 or NewBSD terms.

static PIO pio = pio0;
static uint sm;
static uint offset;
static uint dio;
static uint dck;
static bool initialized;

static inline bool transfer_bit(bool output_enable, bool value) {
    uint32_t command = (uint32_t)output_enable | ((uint32_t)value << 1);
    pio_sm_put_blocking(pio, sm, command);
    return (pio_sm_get_blocking(pio, sm) & (1u << 31)) != 0;
}

static inline void write_bit(bool value) {
    (void)transfer_bit(true, value);
}

static inline bool read_bit(void) {
    return transfer_bit(false, true);
}

static void transaction_begin(void) {
    pio_sm_set_enabled(pio, sm, false);
    pio_sm_clear_fifos(pio, sm);
    pio_sm_restart(pio, sm);
    pio_sm_exec(pio, sm, pio_encode_jmp(offset));
    pio_sm_set_pins_with_mask(pio, sm, 0, (1u << dio) | (1u << dck));
    pio_sm_set_consecutive_pindirs(pio, sm, dio, 1, true);
    pio_sm_set_consecutive_pindirs(pio, sm, dck, 1, true);
    busy_wait_at_least_cycles(12);
    pio_sm_set_enabled(pio, sm, true);
}

static inline void write_msb(uint32_t value, unsigned bits, bool *parity) {
    for (uint32_t mask = 1u << (bits - 1); mask; mask >>= 1) {
        bool bit = (value & mask) != 0;
        *parity ^= bit;
        write_bit(bit);
    }
}

static void transaction_end(void) {
    write_bit(false);
    pio_sm_exec(pio, sm, pio_encode_set(pio_pins, 1));
    busy_wait_us_32(8);
}

void wch_rvswd_init(uint dio_pin, uint dck_pin) {
    dio = dio_pin;
    dck = dck_pin;
    if (!initialized) {
        sm = pio_claim_unused_sm(pio, true);
        offset = pio_add_program(pio, &wch_rvswd_bit_program);
        initialized = true;
    }

    pio_sm_set_enabled(pio, sm, false);
    pio_sm_clear_fifos(pio, sm);
    pio_sm_restart(pio, sm);
    pio_gpio_init(pio, dio_pin);
    pio_gpio_init(pio, dck_pin);
    gpio_pull_up(dio_pin);

    pio_sm_config config = wch_rvswd_bit_program_get_default_config(offset);
    sm_config_set_out_pins(&config, dio_pin, 1);
    sm_config_set_set_pins(&config, dio_pin, 1);
    sm_config_set_in_pins(&config, dio_pin);
    sm_config_set_sideset_pins(&config, dck_pin);
    sm_config_set_out_shift(&config, true, false, 32);
    sm_config_set_in_shift(&config, true, false, 32);
    sm_config_set_clkdiv(&config, 1.0f);
    pio_sm_init(pio, sm, offset, &config);
    pio_sm_set_pins_with_mask(pio, sm, (1u << dio_pin) | (1u << dck_pin),
                              (1u << dio_pin) | (1u << dck_pin));
    pio_sm_set_consecutive_pindirs(pio, sm, dio_pin, 1, true);
    pio_sm_set_consecutive_pindirs(pio, sm, dck_pin, 1, true);
    pio_sm_set_enabled(pio, sm, true);
    sleep_ms(10);
}

void wch_rvswd_write_reg(uint8_t reg, uint32_t value) {
    uint32_t irq_state = save_and_disable_interrupts();
    bool parity = true;

    transaction_begin();
    write_msb(reg & 0x7f, 7, &parity);
    write_bit(true);
    write_bit(parity);
    (void)read_bit();
    (void)read_bit();
    (void)read_bit();
    write_bit(false);
    write_bit(false);

    parity = false;
    write_msb(value, 32, &parity);
    write_bit(parity);
    (void)read_bit();
    (void)read_bit();
    (void)read_bit();
    write_bit(true);
    write_bit(false);
    transaction_end();
    restore_interrupts(irq_state);
}

bool wch_rvswd_read_reg(uint8_t reg, uint32_t *value) {
    uint32_t irq_state = save_and_disable_interrupts();
    bool parity = false;

    transaction_begin();
    write_msb(reg & 0x7f, 7, &parity);
    write_bit(false);
    write_bit(parity);
    (void)read_bit();
    (void)read_bit();
    (void)read_bit();
    write_bit(false);
    write_bit(false);

    uint32_t result = 0;
    parity = false;
    for (unsigned i = 0; i < 32; ++i) {
        bool bit = read_bit();
        result = (result << 1) | bit;
        parity ^= bit;
    }
    bool received_parity = read_bit();
    (void)read_bit();
    (void)read_bit();
    (void)read_bit();
    write_bit(true);
    write_bit(false);
    transaction_end();

    restore_interrupts(irq_state);
    *value = result;
    return received_parity == parity;
}
