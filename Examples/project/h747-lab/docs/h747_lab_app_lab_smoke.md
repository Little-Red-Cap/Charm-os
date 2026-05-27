# H747 Lab App Lab Smoke

This document defines the board-level smoke protocol for `h747_lab_app_lab`.
It validates the first-generation dynamic App ABI resident monitor:

```text
resident runtime/monitor -> load App ELF -> charm_app_main(api, argc, argv)
```

## Verified Target

- Firmware target: `h747_lab_app_lab`
- Serial: `USART1 / 115200 8N1`
- Monitor prompt: `app-lab>`
- Embedded App ELF images: `hello_app`, `player_min`
- Entry ABI: `charm_app_main(const CharmAppApi* api, int argc, char** argv)`

## Build

```powershell
cmake --preset h747-lab-debug -DH747_LAB_ARM_GNU_TOOLCHAIN_ROOT="D:/Toolchains/Arm GNU Toolchain arm-none-eabi/latest"
cmake --build --preset build-h747-lab-app-lab-debug
```

Expected artifacts:

```text
cmake-build-h747-lab-debug/h747_lab_app_lab.elf
cmake-build-h747-lab-debug/h747_lab_app_lab.bin
```

## Flash

Use the App Lab wrapper:

```powershell
.\tools\flash-app-lab-pyocd.ps1
```

The default short-term flashing path writes the binary image to internal Flash
base `0x08000000`:

```powershell
pyocd load -u 0001 -t stm32h747xihx -f 1000k --format bin -a 0x08000000 .\cmake-build-h747-lab-debug\h747_lab_app_lab.bin
```

This is intentional: the current board workflow keeps `.bin @ 0x08000000` as
the fast-path smoke input and treats ELF flashing as an explicit debug fallback.
Use `.\tools\flash-app-lab-pyocd.ps1 -Elf <path>` only when ELF-specific load
behavior is being investigated.

`Exception reading AP#2 IDR: Memory transfer fault` can appear with the current
CMSIS-DAP/pyOCD path. Treat flashing as successful only when pyOCD exits with
code 0 and prints the final erase/program summary.

## Capture

Use the capture wrapper after flashing:

```powershell
.\tools\capture-app-lab-smoke.ps1
```

By default it opens `COM16` at `115200`, tries reset through a fallback chain
(`pyocd reset -M halt -m hw` first, `pyocd commander` second), sends
`app smoke`, and writes:

```text
cmake-build-h747-lab-debug/h747_lab_app_lab_smoke.log
```

The script exits with code 0 only when the serial capture contains all required
tokens:

- `status: monitor=ready display_ready=true input_ready=true file_backed=false`
- `[app-smoke] builtin=ok`
- `[app-smoke] install=ok`
- `[app-smoke] qspi_named=ok`
- `[app-smoke] qspi_raw=ok`
- `[app-smoke] generic_stub=ok`
- `[app-smoke] result=ok`
- `status: source embedded entries=2 readable=true valid=true qspi_ready=true qspi_readable=true qspi_valid=true`
- `status: store install attempted=true ready=true code=ok`
- `status: last request=/not-supported image=/not-supported source=file-backed stage=file-backed code=not_supported exited=false exit=0 backend=0`
- `status: elf_load=[0x24070000,0x24072000) size=8192`

The wrapper now sends `app smoke` first and then `app status`, so board smoke
proves both execution closure and the staged diagnostics report.
The startup banner is intentionally not a required token because the serial
capture can attach after the banner has already been emitted; monitor readiness
is validated through `app status` instead.

For host-only token validation without serial/reset access:

```powershell
.\tools\capture-app-lab-smoke.ps1 -ValidateLog .\cmake-build-h747-lab-debug\h747_lab_app_lab_smoke.log
```

That mode only validates the required tokens in an existing log. It does not
open the serial port or touch the probe/reset path.

For host-only validation of the capture token logic itself:

```powershell
.\tools\capture-app-lab-smoke.ps1 -SelfTest
```

That mode uses synthetic in-memory logs and does not open the serial port,
invoke pyOCD, reset the board, or read board output.

## Manual Smoke

At the `app-lab>` prompt:

```text
app list
app store install
app store status
app store list
app status
app run hello_app demo
app run player_min
app run-path qspi:hello_app demo
app smoke
```

The minimum acceptable result is that:

- builtin `hello_app` and `player_min` both report exit code `0`
- `app store install` reports `code=ok`
- `app run-path qspi:hello_app demo` reports exit code `0`
- `app smoke` prints the full builtin/install/qspi/stub summary and
  `[app-smoke] result=ok`
- `app status` reports the fixed diagnostics order:
  monitor -> source/store -> install -> last result -> elf_load -> display/input
- after `app smoke`, `app status` must show the stable final blocked-stub fact:
  `last request=/not-supported ... source=file-backed stage=file-backed code=not_supported exited=false`

## Current Status

Build verification is complete for `h747_lab_app_lab` with the latest local Arm
GNU toolchain.

Fresh board closure is complete for the current `app_lab` smoke path. The latest
run used `.bin @ 0x08000000`, then `capture-app-lab-smoke.ps1` reset the target,
sent `app smoke`, sent `app status`, and passed the required token set.

Observed board facts include:

- builtin `hello_app` and `player_min` both exited with code `0`
- QSPI install reported `code=ok`, `written=10416`, `erased=12288`
- `qspi:hello_app`, `qspi:player_min`, and raw `qspi:@offset:size` execution
  reached `charm_app_main` and returned `0`
- `status: source embedded entries=2 readable=true valid=true qspi_ready=true qspi_readable=true qspi_valid=true`
- `status: qspi jedec=0x00ef4019 capacity=33554432`
- `status: store install attempted=true ready=true code=ok`
- `status: last request=/not-supported image=/not-supported source=file-backed stage=file-backed code=not_supported exited=false exit=0 backend=0`
- `status: elf_load=[0x24070000,0x24072000) size=8192`

Remaining probe-path caveat:

- pyOCD still prints AP#2 discovery faults on reset/load, but the latest
  `app_lab` flash and capture both completed with exit code `0`
- flashing `238972` bytes at `1000k` still took about 185s, around `1.43 KiB/s`
