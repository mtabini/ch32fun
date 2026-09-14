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

## UART bridge

The firmware now exposes two independent CDC ACM serial ports:

- Interface 0: `Ardulink Programmer`, used exclusively by minichlink.
- Interface 2: `CH32 UART Bridge`, a transparent, bidirectional UART0 bridge.

The development USB identity is `1209:7636` with the Pico board's unique serial.
This VID/PID is provisional and must be assigned before distribution. The new
identity avoids reusing the old single-port USB driver's cached configuration.
Both ports use the operating system's CDC driver. Select the programmer port
explicitly when flashing; never send Ardulink commands to the UART bridge.

| Pico | Target | Purpose |
| --- | --- | --- |
| GP0 (physical pin 1) | UART RX | Computer to target |
| GP1 (physical pin 2) | UART TX | Target to computer |
| GND (physical pin 3) | GND | Common ground |

Use 3.3 V logic, not RS-232 voltage levels. Power the target separately.
The target USB cable is unnecessary for UART communication. Existing GP2/GP3
programming wiring remains unchanged. The CH32 application must initialize its
chosen UART pins and baud rate; this bridge adds no CH32 firmware code.

Default UART settings are 115200 baud, 8N1. Terminal line coding configures baud
rates from 300 through 1,000,000, 5-8 data bits, no/odd/even parity, and one/two
stop bits. Unsupported settings retain the previous hardware configuration.
Actual baud is rounded by the RP2040 UART divider. No RTS/CTS or BREAK support.

UART reception/transmission runs on core 1; USB and RVSWD run on core 0.
The queues hold 4096 received bytes and 512 bytes awaiting UART transmission.
Host-to-target traffic backpressures USB when full. Target-to-host overflow drops
new bytes; there is no hardware flow control. Incoming UART data is discarded
while the bridge port is closed. Finite buffering cannot guarantee lossless
logging when the computer is disconnected or stops reading. Open the bridge port
with DTR asserted (the usual terminal default).

Only the programmer port implements 1200-baud close-to-BOOTSEL. Setting the UART
bridge to 1200 baud does not reboot the Pico. Physical BOOTSEL remains the recovery
path. CMake `WCH_UART_TX_PIN` / `WCH_UART_RX_PIN` select other exposed UART0 pins;
configuration rejects unsupported pins and overlap with programming pins.

To use the bridge with Python's pyserial terminal:

```sh
python -m serial.tools.list_ports -v
python -m serial.tools.miniterm /dev/cu.usbmodemYOUR_UART_PORT 115200
```

Windows uses the corresponding COM port; Linux uses `/dev/ttyACM*`.
Serial port names can change when installing the new USB descriptor layout.

Host regression checks: `python3 programmers/pico/tests/run.py`. These exercise
framing, independent ports, backpressure, line coding and BOOTSEL routing with
fake hardware; physical USB/UART qualification is recorded in `QUALIFICATION.md`.
