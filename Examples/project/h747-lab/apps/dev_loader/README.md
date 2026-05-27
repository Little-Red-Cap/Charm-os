# H747 Dev Loader

`dev_loader` is the first resident development-loader skeleton for H747. It is
not a product bootloader and does not replace `app_lab`.

In the current H747 dynamic-boundary roadmap it is explicitly post-mainline:
`app_lab` closes the resident App ABI first, and `dev_loader` stays a
development acceleration experiment until that path is stable.

The goal is to avoid repeatedly erasing/programming internal Flash during app
development:

```text
flash resident dev_loader once -> receive image over transport -> place in RAM
or external storage -> verify -> launch/copy/execute later
```

Current first cut:

- uses a RAM-backed receive buffer inside the resident firmware;
- validates the shared `Examples/dev_loader/charm_dev_loader.hpp` session model;
- keeps binary receive semantics in the shared `BinaryReceiveRuntime`, so future
  USB transport can feed byte chunks without changing monitor state rules;
- wires transport packet v0 through the shared `PacketRuntime` and
  `ByteTransportRuntime`, with a console hex-ingest frontend for board smoke;
- routes console commands through the reusable
  `Examples/dev_loader/charm_dev_loader_commands.hpp` command runtime;
- exposes console commands to exercise begin/write/verify/launch-ready stages;
- keeps launch as dry-run, so it does not jump into downloaded code yet;
- leaves USB bulk/CDC receive as the next transport adapter.

Commands:

- `dev status`
- `dev begin <size> [crc_hex]`
- `dev fill <hex_byte> <count>`
- `dev verify`
- `dev launch dry-run`
- `dev abort`

This target is intentionally small and diagnostic-first. Console and future USB
transport frontends should feed the same command/runtime and
`Session::write_chunk()` path rather than creating another download model.

Current non-goals for this stage:

- no real jump into downloaded code
- no USB requirement
- no USB packet transport yet; UART raw packetstream is only a development
  frontend for the existing packet v0 byte stream
- no `app_lab` image handoff requirement
- no promotion to product bootloader/runtime policy

Packet frontend commands:

- `dev packet status`
- `dev packet ingest <hex>`
- `dev packet reset`
- `dev packet reset-session`
- `dev raw begin`
- `dev raw status`
- `dev raw abort`
- `dev app stage <name>`
- `dev app probe <name>`
- `dev app prepare <name> [args...]`
- `dev app status`

`dev packet ingest` accepts continuous hex pairs or space-separated hex pairs.
The current console line buffer is 128 bytes, so keep each command small; use
roughly 48 decoded bytes or less per line and split large packet streams across
multiple `dev packet ingest` commands.
`dev packet reset` only clears partially buffered transport bytes. Use
`dev packet reset-session` before replaying a new packetstream from sequence 0
after a failed or aborted transfer.

`dev raw begin` switches the monitor into raw UART packetstream mode. In this
mode incoming USART bytes are fed directly into the same
`ByteTransportRuntime -> PacketRuntime -> BinaryReceiveRuntime` path. The mode
automatically exits after `launch_dry_run` reaches `launch_ready`, or after a
packet/transport error. It still does not jump into downloaded code.

The console RX frontend now attempts to start USART1 RX DMA in circular mode
and falls back to direct `RDR` polling if DMA cannot start. `dev packet status`
and `dev raw status` print RX counters (`dma_started`, `dma_bytes`, `fallback`,
`overrun`, and DMA read/write positions) so board smoke can distinguish packet
semantic failures from UART frontend loss. The DMA RX ring is treated as a
cache-coherent frontend boundary: the monitor invalidates DCache lines before
reading DMA-written bytes, and does not mix direct `RDR` polling after DMA has
started.

While raw mode is active, the UART byte stream is treated as binary-transparent
packetstream data. Text commands are not parsed until raw mode exits. This is
intentional: escape bytes such as Ctrl-C could appear inside a real ELF payload.
`dev raw abort` is therefore a command-mode cleanup/reset command, not an
in-band raw payload escape. A packet v0 `abort` packet can still terminate a raw
session without inventing a second protocol.

USB CDC or USB bulk should reuse the same frontend boundary: transport code
only supplies byte chunks to `ByteTransportRuntime::ingest()`. It must not
define a second begin/data/verify/launch protocol.

After a packetstream reaches `launch_ready`, `dev app stage <name>` reads the
verified payload back from the RAM receive buffer into a 128 KiB staging scratch
buffer and stages it as `AppImage(format=elf)`. `dev app probe <name>`
additionally performs a dry ELF load probe into a private 64 KiB diagnostic
buffer and prints entry offset, load span, segment count, runnable flag, and
the would-be loaded entry address. `dev app prepare <name> [args...]` performs
the same stage + ELF load path and then calls `AppRuntime::prepare()` to
validate the C ABI header and build argv; it stops at the `start` stage and
prints `ready`, `argc`, and the would-be entry address. These commands print
`run=disabled`; they do not call `charm_app_main` and do not jump.

Current board-free hardening note: this target is build-verified only in the
current round. The latest board pass validates both the text command receive
path and the console-friendly packet hex ingest path on H747. The raw UART DMA
RX path is wired but still needs the next board pass. It still does not
implement USB receive or real launch.

Board validation helpers:

- Build from `Examples/project/h747-lab`:
  `cmake --build --preset build-h747-lab-dev-loader-debug`
- Convert a payload into raw packetstream bytes:
  `Examples/system/dev_loader_packet_stream_tool/cmake-build-dev-loader-packet-stream-tool/dev-loader-packet-stream.exe input.bin output.packetstream --chunk 128`
- Convert that packetstream into console command lines:
  `Examples/system/dev_loader_packet_console_tool/cmake-build-dev-loader-packet-console-tool/dev-loader-packet-console.exe output.packetstream output.commands --bytes-per-line 48`
- Flash the binary image, not the ELF:
  `powershell -ExecutionPolicy Bypass -File tools/flash-dev-loader-pyocd.ps1`
- Capture the console smoke:
  `powershell -ExecutionPolicy Bypass -File tools/capture-dev-loader-smoke.ps1`
- Capture a packetstream console smoke:
  `powershell -ExecutionPolicy Bypass -File tools/capture-dev-loader-packet-smoke.ps1 -Commands output.commands`
- Send a raw UART packetstream:
  `powershell -ExecutionPolicy Bypass -File tools/send-dev-loader-raw-packetstream.ps1 -PacketStream output.packetstream`
- If raw UART loses bytes, keep the same packetstream and slow only the sender:
  `powershell -ExecutionPolicy Bypass -File tools/send-dev-loader-raw-packetstream.ps1 -PacketStream output.packetstream -WriteChunkSize 64 -InterChunkDelayMs 1`
- Before judging raw packet semantics, run `dev packet status` or
  `dev raw status` and check the RX frontend counters. The expected DMA path is
  `dma_started=1`, `dma_fail=0`, `fallback=0` or close to zero, and
  `overrun=0`. If `fallback` grows after DMA has started, or `overrun` grows
  during raw receive, classify the failure as UART frontend loss before
  changing packetstream/App semantics.
- Host-check raw frontend semantics without a board:
  `ctest --test-dir ../../system/dev_loader_raw_uart_smoke/cmake-build-dev-loader-raw-uart-smoke --output-on-failure`
- Host-check received ELF stage/probe semantics without a board:
  `ctest --test-dir ../../system/dev_loader_stage_probe_smoke/cmake-build-dev-loader-stage-probe-smoke --output-on-failure`

For manual board smoke, send the generated `output.commands` lines to the
console, then run `dev packet status`. A successful dry-run receive should end
at `launch_ready`. This still does not jump into downloaded code.

Latest board facts:

- `capture-dev-loader-smoke.ps1` reached `stage=launch_ready` with a 16-byte
  RAM receive session.
- `capture-dev-loader-packet-smoke.ps1` reached `stage=launch_ready` with a
  64-byte packetstream delivered as `dev packet ingest <hex>` lines.
- `flash-dev-loader-pyocd.ps1` previously used `100k` SWD and took about 513s
  for a 94 KiB image. The default is now `1000k`; lower it explicitly only when
  probe stability requires it.

The next handoff target after `launch_ready` is the shared
`dev_loader_app_handoff_smoke` path: read the verified payload back as a received
image, stage it as an App ABI `AppImage`, and let `AppRuntime` own loading and
entry invocation. Do not add a direct raw jump path in this monitor.

The received ELF load semantics are covered off-board by
`dev_loader_received_elf_smoke`: real App ELF bytes are received and load-probed
without executing target code on the host. H747 should reuse that load/staging
boundary before adding a monitor command that runs downloaded apps.

Current local probe facts:

- `STM32CubeProgrammer_CLI` v2.17.0 is installed, but it only enumerates
  DFU/J-Link/ST-Link and does not see the current Charm CMSIS-DAP probe.
- pyOCD can connect to `Charm CMSIS-DAP v1` / `0001`, but stale `pyocd load`
  processes must be cleared before flashing or the next CMSIS-DAP handshake can
  fail with `expected DAP_INFO`.
- pyOCD may print `Exception reading AP#2 IDR: Memory transfer fault` on H747;
  this is not enough by itself to classify the flash as failed.
- The flash command is considered successful only when pyOCD exits with `0`.
  DAP transfer errors during smart/partial Flash reads indicate a probe
  transport problem, not a dev-loader firmware problem.

For the current role split against `app_lab` and `posix_lab`, read:

- `../docs/h747_lab_dynamic_boundary_roadmap.md`
