#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "pico/types.h"

void wch_rvswd_init(uint dio_pin, uint dck_pin);
void wch_rvswd_write_reg(uint8_t reg, uint32_t value);
bool wch_rvswd_read_reg(uint8_t reg, uint32_t *value);
