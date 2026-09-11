# Raspberry Pi Pico CH32 programmer

Experimental CH32X035 programmer firmware for Raspberry Pi Pico (RP2040). It exposes
the Ardulink register protocol over USB CDC, allowing `minichlink` to perform
chip detection, flash erase, programming, and verification.

RVSWD wire timing runs on an RP2040 PIO state machine. The implementation has
been validated on a CH32X035F8U6 using an RP2040 Pico.

## Wiring

| Pico | CH32X035 | Purpose |
| --- | --- | --- |
| GP2 | PC18 | DIO / SWDIO |
| GP3 | PC19 | DCK / SWCLK |
| GND | GND | Common ground |

Power the target separately at 3.3 V. Do not connect Pico's 3V3 output when
the target already has power. Ardulink `p` and `P` commands are acknowledged but
do not switch target power. The `p` command reinitializes the RVSWD pins before
each `minichlink` session so a previous interrupted transaction cannot poison the
next connection.

## Build

Requires Pico SDK 2.1 or newer and an Arm embedded GCC toolchain.

Initialize Pico SDK's USB dependency once:

```sh
git -C "$PICO_SDK_PATH" submodule update --init lib/tinyusb
```

```sh
export PICO_SDK_PATH=/path/to/pico-sdk
cmake -S programmers/pico -B build/pico-programmer \
  -DPICO_BOARD=pico \
  -DPICO_TOOLCHAIN_PATH=/path/to/arm-gnu-toolchain \
  -DWCH_RVSWD_DIO_PIN=2 \
  -DWCH_RVSWD_DCK_PIN=3
cmake --build build/pico-programmer --parallel
```

Hold BOOTSEL while connecting Pico, then copy
`build/pico-programmer/ch32fun_pico_programmer.uf2` to the `RPI-RP2` volume.

## Test target connection

Replace the serial device with the Pico USB CDC path:

```sh
make -C minichlink
minichlink/minichlink -C ardulink -c /dev/cu.usbmodemXXXX -b
```

Do not attempt a flash write until chip detection reports `CH32X035` reliably.

## Status

- RP2040 USB CDC and Ardulink framing: implemented
- RVSWD wire timing: RP2040 PIO state machine
- CH32X035 two-wire RVSWD register transactions: hardware validated
- Chip detection through `minichlink`: hardware validated
- Erase/write/read-back verification through `minichlink`: hardware validated

RVSWD transaction sequence follows the existing `ch32fun` ESP32-S2 programmer.
