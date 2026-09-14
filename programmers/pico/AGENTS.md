# Pico programmer agent instructions

Read [README.md](README.md) before using or changing the dual-CDC programmer and
[QUALIFICATION.md](QUALIFICATION.md) before making hardware acceptance claims.

- CDC instance 0 is Ardulink programming; instance 1 is the UART bridge. Never mix their traffic. Host device paths are ephemeral; identify device serial and interface ancestry.
- Default UART0 pins are GP0 TX and GP1 RX. Cross TX to target RX and RX to target TX, connect ground, and use 3.3 V logic. Default framing is 115200 8N1; the host terminal controls supported line coding.
- VS Code Microsoft Serial Monitor (`ms-vscode.vscode-serial-monitor`) can read/write the UART port independently of the programmer. Close competing applications using that same port. Only the programmer port uses 1200-baud close-to-BOOTSEL.
- Core 0 owns TinyUSB and RVSWD; core 1 owns UART. Preserve bounded service loops, synchronized queues, backpressure, and documented overflow behavior. Never call TinyUSB concurrently from both cores.
- Target UART initialization and protocols belong to target products. Keep CH32 UART qualification code under tests and out of the Hardware Core platform. The bridge does not automatically add SCPI or firmware uploading over UART.
- Before hardware writes, identify the exact target, flash geometry, artifact and recovery path. Preserve/restore temporary target test firmware. If an initial UART check fails with uncertain wiring, stop and report it instead of assuming a firmware defect or trying different pins.
- Run `python3 programmers/pico/tests/run.py`, build the Pico UF2, and check `git diff --check`. Physical claims require actual device evidence, including independent programmer and UART operation.
