# Dual-CDC UART qualification — 2026-09-14

## Tested configuration

- Raspberry Pi Pico RP2040 B2, flash ID/serial `425030503137330F`, 2 MiB flash.
- Pico SDK 2.1.1; Arm GCC 14.2.1; release build.
- USB `1209:7636`, development identity. Programmer CDC interface 0 / data 1;
  UART CDC interface 2 / data 3.
- macOS ports: programmer `/dev/cu.usbmodem141201`; UART `/dev/cu.usbmodem141203`.
- CH32X035F8U6 serial `CDAB22F969BE5863FFFFFFFF`, 62 KiB code flash,
  read protection disabled. Test UART4: PB0 TX, PB1 RX; default AFIO mapping,
  verified against WCH CH32X035 reference manual v1.9, page 72.
- Wiring: Pico GP0 to CH32 PB1, Pico GP1 to CH32 PB0, common ground; 115200 8N1.

Pico UF2 SHA-256:
`c0bd39b14437c5bd0276ae6c6f7614803c301d5072bbe00fa7905826fa7341fe`.
Pico programmed binary: 29,192 bytes, versus 26,232 bytes previously (+2,960).
ELF BSS: 8,008 bytes versus 4,056. Queue payload storage is additionally allocated
at startup (4096 RX bytes, 512 TX bytes, one line-coding structure).

Temporary CH32 application: 700 bytes, linked at `0x08002100`; SHA-256
`e582da099680bc96078acb5235c17a5e0fb5fee821d1a63f191da23b120ec06a`.
Source retained only as `tests/hardware/ch32_uart_echo.cpp`; it is not part of
normal programmer or Hardware Core builds. It disables CH32 USB, services resident
housekeeping, and echoes UART bytes. A test-only source override using Hardware
Core's `CH32_PRODUCT_SOURCES` built the image. Do not install it on active products.

## Results

- Pico ROM flash load with verification: pass. Original full Pico flash retained
  before installation. BOOTSEL is the recovery path.
- Two independent macOS CDC ports, physical serial retained: pass.
- Original minichlink chip detection/options/identity through programmer CDC: pass.
- First 18-byte UART echo: exact match.
- Binary echo of every byte value, 128 blocks of 257 bytes (32,896 total): exact match.
- 14,857 concurrent Ardulink `?`/`+` protocol exchanges during 3.163 seconds: pass.
  These are protocol liveness exchanges, not simultaneous target flash writes.
- CH32 USB absent during UART test: verified by USB enumeration.
- Opening/closing UART CDC at 1200 baud does not trigger Pico BOOTSEL: pass.
- Hardware Core macOS port selection excludes UART and explicitly rejects it: pass.
- Native ASan/UBSan checks cover fragmented requests, little-endian registers,
  parity-failure reply, port isolation, both backpressure directions, closed-port
  RX discard, UART configuration deferral, and programmer-only BOOTSEL routing.
- Unsupported UART pins and UART/RVSWD overlap rejected by CMake.

## Restoration and limits

Only CH32 offsets `0x2000..0x23ff` (manifest and three application pages) changed.
Those four pages were restored through Pico. Double read-back confirmed exact
original code (63,488 bytes), system flash (3,328 bytes), option area (256 bytes),
and UID. No UART code was added to the Hardware Core platform. CH32 was returned
to its original updater-only state; the new Pico bridge remains installed.

Restored code SHA-256:
`d7c3c4057f4cd6c8c84accc030c924b772487d5b761160dfddb4253d6e1e2bb6`.
Restored system SHA-256:
`729255ea3c3ad319fce1140699e1bb75fafb7a76c9e2aa0a65c099121fdc71fe`.
Restored option area SHA-256:
`1d2c25ded851401d89cb3d3b56826cec0fc6e8500480d82666e97b54296524b0`.

No Windows/Linux driver qualification, other physical UART rates/formats,
RTS/CTS, electrical timing measurement, or sustained overload qualification.
No lossless claim under RX overflow or disconnected/slow hosts. Tests did not
require CH32 USB for communications, but its cable remained physically attached.
