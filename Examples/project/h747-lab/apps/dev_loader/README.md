# H747 Dev Loader

`dev_loader` is the first resident development-loader skeleton for H747. It is
not a product bootloader and does not replace `app_lab`.

In Resident Image Platform v1 terms, `dev_loader` is a resident development
runtime. Its job is to bind H747 byte transports, QSPI/eMMC App Store media,
SDRAM staging arenas, the D1 App image execution region, and diagnostic monitor
commands to the shared `ImageSource/ImageStore -> AppImage -> staged
AppImageSource -> AppRuntime -> CharmAppApi` chain. It must not grow a second
App entry, Store protocol, or capability table.

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
- places the large receive buffer and App staging scratch in SDRAM2-backed
  `.sdram` NOLOAD storage, while keeping the executable App image region in D1
  RAM at `0x24070000..0x24080000`;
- validates the shared `Examples/dev_loader/charm_dev_loader.hpp` session model;
- keeps binary receive semantics in the shared `BinaryReceiveRuntime`, so future
  USB transport can feed byte chunks without changing monitor state rules;
- wires transport packet v0 through the shared `PacketRuntime` and
  `ByteTransportRuntime`, with a console hex-ingest frontend for board smoke;
- routes console commands through the reusable
  `Examples/dev_loader/charm_dev_loader_commands.hpp` command runtime;
- exposes console commands to exercise begin/write/verify/launch-ready stages;
- keeps `dev launch dry-run` as the generic receive-session marker, while
  explicit App execution is owned by `dev app run`;
- adds a first USB CDC packetstream byte-source frontend for development
  downloads.

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

- no product bootloader jump path; received App image execution is only exposed
  through the explicit `dev app run` development command
- no composite USB device or shared USB ownership
- no promise to restore the previous USB state after download mode
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
- `dev usb begin`
- `dev usb status`
- `dev usb abort`
- `dev store status`
- `dev store install qspi`
- `dev store install emmc`
- `dev store list qspi`
- `dev store list emmc`
- `dev store stage qspi:<name>`
- `dev store stage emmc:<name>`
- `dev app stage <name>`
- `dev app probe <name>`
- `dev app prepare <name> [args...]`
- `dev app run <name>|qspi:<name>|emmc:<name> [args...]`
- `dev app status`

`dev app status` now prints a canonical resident ELF/App run record in addition
to the legacy command line:

```text
dev: app record source=<received|qspi|emmc> format=<elf|modulex> name=<app>
  command=<stage|probe|prepare|run> argv=<argc> load=<addr> entry=<addr>
  span=<bytes> segments=<n> run_stage=<stage> run_code=<code>
  exited=<0|1> exit=<code> caps_console=<bytes> caps_present=<n> caps_input=<n>
```

For ELF capacity work, the same status output also prints the fixed H747 run
region and the latest load-span pressure:

```text
dev: app run-region name=ram_d1_app_elf base=0x24070000 size=65536 align=16 linked_elf_base=0x24070000
dev: app capacity needed=<load_span> free=<bytes> fits=<0|1> region=65536 probe=<elf_probe_code>
```

This is the v1 boundary for large App ELF diagnostics. A payload that exceeds
the D1 run region must fail in probe/load with `load_buffer_too_small`; it must
not turn into a private jump path, SDRAM execute experiment, or App ABI change.

For ELF, `received`, `qspi`, and `emmc` are expected to converge on the same
`AppImage(format=elf) -> staged AppImageSource -> AppRuntime -> CharmAppApi`
result model. The legacy `dev: app command=... name=qspi:<name>` token remains
for existing scripts, but new platform checks should prefer the canonical
`source=... format=elf name=...` record.

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

`dev usb begin` switches the monitor into an exclusive USB CDC packetstream
receive mode. The existing UART console stays as the control channel, while the
newly enumerated USB CDC COM port is treated as a binary-transparent download
pipe. USB bytes are read from the `usb_dev_loader` service and fed into the same
`ByteTransportRuntime -> PacketRuntime -> BinaryReceiveRuntime` path used by
UART raw mode. The mode automatically exits after `launch_ready` or
packet/transport failure, but keeps the USB service state available for
`dev usb status` diagnostics. `dev usb status` prints a sticky `exit=` reason
(`active`, `launch_ready`, `packet_error`, `transport_error`, or `abort`) plus
the last packet/transport state and the current frontend parameters
(`packet_buffer`, `max_payload`, `read_chunk`, and `drain_limit`). `dev usb
abort` is the explicit cleanup path: it stops/disconnects the USB device and
resets the receive session instead of trying to restore any previous USB
function. A later App that needs USB must own and initialize its own USB
backend.

USB CDC board validation status:

- `dev usb begin` reaches `USBD_Init`, class/interface registration, PCD start,
  soft-disconnect release, Windows CDC enumeration, and `cdc_ready=1` when the
  board USB device cable is connected.
- `dev usb status` prints OTG core/device registers, EP0 event counters, the
  last setup packet if present, and device/config descriptor prefixes.
- If the USB device cable is not connected, both CDC and the USB MSC comparison
  firmware show `setup=0 reset=0 connect=0 disconnect=0`; classify that as a
  physical host/device path issue before changing packet v0 or AppRuntime.
- With the cable connected, current board evidence shows `setup>0`, `reset>0`,
  `cdc_ready=1`, and a Windows CDC port such as `COM27`.
- `hello_app.elf.packetstream` over USB CDC reached `launch_ready`, then
  `dev app run hello_app alpha beta` entered `charm_app_main()` and returned
  `exit=0`.
- `player_min.elf.packetstream` over USB CDC reached `launch_ready`, then
  `dev app run player_min` presented one stub frame, polled input once, and
  returned `exit=0`.
- The first measured USB CDC packetstream path is about `50 KiB/s` to
  `launch_ready` for the small App ELF samples, already materially faster than
  the current pyOCD internal Flash path.
- The larger `.appstore.bin.packetstream` exposed USB CDC frontend instability:
  Windows CDC can disappear during transfer after partial RX. Treat this as a
  USB byte-source issue, not an App Store, QSPI, ELF, packet v0, or AppRuntime
  issue. Re-test with `dev usb status` after failure and classify by
  `exit=`, USB rx/drop/overflow counters, and the last packet result before
  changing packetstream or store semantics.
- After the USB failure-retention update, the same `.appstore.bin.packetstream`
  was re-tested on board. A 4096-byte host write chunk failed cleanly with
  `exit=transport_error`, `packet transport=buffer_too_small`,
  `received=256`, and later USB RX overflow counters, proving the failure was
  a frontend buffering/host pacing issue. Retrying with 256-byte writes and a
  1 ms inter-chunk delay reached `exit=launch_ready`, `received=10416`,
  CRC `0x73de4894/0x73de4894`, and `dropped=0 overflow=0`. The staged payload
  then installed to QSPI and ran both `qspi:hello_app alpha beta` and
  `qspi:player_min` through `AppRuntime` with exit code `0`.
- The USB CDC sender default is now the validated safe path: 256-byte writes
  with a 1 ms inter-chunk delay. Use larger writes such as 4096 bytes only as
  an explicit stress case for frontend buffering diagnostics.
- `capture-dev-loader-usb-cdc-appstore-transfer-smoke.ps1` has validated the
  safe USB App Store transfer path with three consecutive `.appstore.bin`
  packetstream downloads. Each run reached `exit=launch_ready`,
  `received=10416`, CRC `0x73de4894/0x73de4894`, and `dropped=0 overflow=0`;
  measured throughput was about `13.65..13.95 KiB/s`.
- USB frontend hardening v1 raises the byte-transport buffer to `1024`, reads
  USB CDC in `512` byte chunks, and drains up to `4096` bytes per monitor loop.
  Packet v0 and the `256` byte packet payload limit remain unchanged.
- The same hardening pass made `dev usb abort` idempotent before USB init or
  after USB stop. This removed a board HardFault in `HAL_PCD_Stop()` when the
  transfer smoke pre-cleaned a fresh monitor session.
- Board validation after the hardening build (`h747_lab_dev_loader.bin`
  `258540` bytes, `RAM_D1=476240 B / 90.84%`, `FLASH=258540 B / 12.33%`)
  passed the safe default `.appstore.bin.packetstream` path three consecutive
  times with `256` byte writes plus `1 ms` delay. Throughput was
  `12.97..13.52 KiB/s`, CRC stayed `0x73de4894/0x73de4894`, and
  `dropped=0 overflow=0`.
- The explicit throughput sweep with zero inter-chunk delay also passed three
  consecutive runs for each host write chunk: `256` bytes at
  `65.37..77.38 KiB/s`, `512` bytes at `64.63..75.33 KiB/s`, and `1024` bytes
  at `64.92..68.11 KiB/s`. All runs reached `exit=launch_ready`,
  `received=10416`, CRC `0x73de4894/0x73de4894`, and `dropped=0 overflow=0`.
- The resident runtime prefers SDRAM2 for receive/stage arenas when SDRAM2 is
  healthy: `sdram2_receive_buffer` at `0xD0000000` with `256 KiB`, and
  `sdram2_stage_cache` at `0xD0040000` with `128 KiB`. If SDRAM2 smoke fails,
  startup continues and the runtime falls back to D1 RAM receive/stage arenas
  (`128 KiB` each). `dev status` and `dev app status` print the actual arena
  names, addresses, capacity, and SDRAM2 `ready/init/smoke` state so board logs
  can distinguish platform fallback from Store/AppRuntime failures.
- SDRAM2 diagnostic closure: the dedicated `diag_shell` probes passed SDRAM2
  `probe/bus/addr/lane/repeat/locate/verify` across the `0xD0000000..0xD2000000`
  window, and timing sweep passed all tested presets except the expected
  burst-length-8 variant. Storage/eMMC initialization did not break SDRAM2.
  The earlier `dev_loader` fallback was caused by probing memory before the
  storage power profile was applied, not by data/address lines or partial-bank
  failure. `dev_loader` now applies `storage_stage_a` before SDRAM2 smoke, and
  the board smoke reports `sdram2 ready=1 init=1 smoke=1` with receive arena
  `sdram2_receive_buffer`.
- The SDRAM build passed the safe USB App Store transfer smoke three
  consecutive times with 256-byte writes plus 1 ms delay. Throughput was
  `12.77..13.45 KiB/s`; every run reached `exit=launch_ready`, received
  `10416` payload bytes, matched CRC `0x73de4894/0x73de4894`, and reported
  `dropped=0 overflow=0`.
- The same `launch_ready` `.appstore.bin` installed to QSPI with
  `receive=ok`, `store=ok`, `code=ok`, `written=10416`, and `erased=12288`.
  `dev store list qspi` reported `hello_app` and `player_min`, and
  `dev app run qspi:hello_app alpha beta` plus `dev app run qspi:player_min`
  both loaded from QSPI, used the SDRAM stage cache, and exited with code `0`.
- The SDRAM build also passed the zero-delay USB throughput sweep. Three
  consecutive runs passed for each host write chunk: `256` bytes at
  `66.25..67.51 KiB/s`, `512` bytes at `65.92..77.32 KiB/s`, and `1024`
  bytes at `73.86..76.60 KiB/s`, with CRC match and no drop/overflow.
- `capture-dev-loader-usb-cdc-appstore-platform-matrix-smoke.ps1` is the
  recommended resident platform regression entry. It runs the existing USB
  App Store platform smoke for both QSPI and eMMC, preserving the same download,
  install, list, and AppRuntime run semantics while producing one matrix log.
  Matrix validation also requires the SDRAM2 stage cache token and the fixed
  D1 App ELF run-region token, so a media pass cannot hide an arena regression.
  Use `capture-dev-loader-usb-cdc-appstore-platform-smoke.ps1 -Media qspi|emmc`
  when only one Store media needs to be checked.
- Before running the matrix, regenerate the canonical resident-platform input
  artifacts with `Examples/app_abi/elf_samples/build_resident_platform_artifacts.ps1`.
  The script emits `hello_app.elf`, `player_min.elf`,
  `modulex_hello_app.modulex`, the mixed `appstore.bin`, packetstreams for each,
  and `artifact_manifest.json` under `Examples/app_abi/elf_samples/out`. The
  manifest is a host/CI evidence index only; it is not Store v1 metadata and does
  not change the board protocol.
- The App Store platform, matrix, and App-run capture scripts now accept
  `-ArtifactManifest` and default to the generated manifest. The recommended
  resident-platform evidence flow is now
  `capture-resident-platform-evidence-bundle.ps1`. By default it runs only the
  off-board chain: `build_resident_platform_artifacts.ps1 -Validate`,
  `resident-platform-inspect --strict`, host regression smokes, and H747
  `dev_loader` build-only. Passing `-BoardMatrix` explicitly appends the
  QSPI/eMMC USB CDC platform matrix; without that switch it does not open serial,
  USB, reset, or flash the board. Passing `-InstalledStoreMatrix` explicitly runs
  a lighter persistence check that does not download or install anything; it only
  lists the already-installed QSPI/eMMC stores and runs the three resident apps
  by name. Explicit `-PacketStream` on the lower-level scripts remains the
  override path for temporary stress inputs.
- The first matrix board run passed for both media with the safe USB defaults:
  QSPI reported about `13.28 KiB/s`, `written=10416`, `erased=12288`, and eMMC
  reported about `13.27 KiB/s`, `written=10416`, `erased=10752`. Both media
  ran `hello_app` and `player_min` through `AppRuntime` with exit code `0`.
- ModuleX is the second App image format on this resident platform. Store v1
  keeps the same header/entry layout and uses `AppStoreEntry.flags & 0xF`:
  `0` means ELF and `1` means ModuleX. `dev app run <name>` detects received
  ModuleX payloads by ModuleX magic/version, while `dev app run qspi:<name>` and
  `dev app run emmc:<name>` use Store flags. Both paths still enter the same
  staged `AppImageSource -> AppRuntime -> CharmAppApi` chain.
- ModuleX App v1 is materialized-only. It supports local/global/external symbols
  and `abs_addr` / `rel32` relocations, and the `modulex_hello_app` board sample
  now requires an `abs_addr` relocation to patch its console message pointer.
  The App ABI ModuleX loader rejects non-zero BSS and XIP flags. `dev app
  status` prints ModuleX diagnostics including `validate`, `dep`, `entry_off`,
  `span`, and `relocated`; successful board evidence should show
  `format=modulex modulex=ok` and `relocated=1`.
- ModuleX stabilization v1 board validation passed with the resident dev_loader
  build `h747_lab_dev_loader.bin=315748` bytes. The received ModuleX path used
  `modulex_hello_app.modulex.packetstream` (`436` bytes, payload `268`, CRC
  `0xa2e96be3`) and `dev app run modulex_hello_app alpha beta` exited `0` with
  `validate=ok`, `relocated=1`, and span `268`. The mixed Store matrix used
  `.appstore.bin.packetstream` (`11992` bytes, payload `10732`, CRC
  `0x25330f78`) and passed both QSPI and eMMC: QSPI wrote `10732` bytes/erased
  `12288` bytes at about `13.59 KiB/s`, eMMC wrote `10732` bytes/erased `10752`
  bytes at about `13.91 KiB/s`, and all three apps (`hello_app`, `player_min`,
  `modulex_hello_app`) exited `0` on both media.

The resident App Store path uses the same packetstream receive boundary, but
expects the received payload to be a `.appstore.bin` generated by the App ABI
store packer:

```text
dev usb begin or dev raw begin
  -> packetstream .appstore.bin
  -> launch_ready RAM receive buffer
  -> dev store install qspi
  -> AppStoreReader over QSPI
  -> dev store list qspi
  -> dev app run qspi:<name> [args...]
```

`dev store install qspi` does not hand-write QSPI protocol logic in the monitor.
It reads the verified received payload with `received_image_read`, validates the
Store v1 header, then calls `app_store_install_image()` through a QSPI-backed
`AppStoreWritableMedia`. Readback verification is part of that shared
installer. `dev store stage qspi:<name>` and `dev app run qspi:<name>` use the
same `AppStoreReader -> app_store_stage_named_image -> staged AppImageSource ->
AppRuntime` boundary as host smokes and future eMMC must reuse.

The eMMC backend is wired as the second App Store media using the same Store v1
and AppRuntime boundary. `dev store install emmc` writes the verified
`.appstore.bin` into a fixed development slot at the tail of the exposed eMMC
block window, not into a filesystem path. The adapter maps Store byte offsets
onto 512-byte block reads/writes and performs read-modify-write for unaligned
Store ranges, so the monitor still consumes the byte-oriented
`AppStoreReader/AppStoreWritableMedia` contracts. Commands are intentionally
parallel to QSPI: `dev store list emmc`, `dev store stage emmc:<name>`, and
`dev app run emmc:<name> [args...]`. This is a development slot, not a product
partition, manifest, slot manager, or filesystem.

eMMC App Store board validation status:

- The eMMC raw development slot is board-validated as the second Store v1
  media. Current diagnostics report `ready=1`, `init=1`, `block_ready=1`,
  `block_size=512`, `part_lba=2048`, slot LBA `30500864`, slot blocks `32768`,
  and slot size `16 MiB`.
- `capture-dev-loader-usb-cdc-appstore-platform-smoke.ps1 -Media emmc`
  downloaded the generated `.appstore.bin.packetstream` over USB CDC to
  `launch_ready`, installed the 10,416-byte Store v1 image to the eMMC raw
  slot, listed `hello_app` and `player_min`, and ran both through
  `dev app run emmc:<name>`.
- The validated eMMC run reported `dev store install emmc receive=ok`,
  `store=ok`, `code=ok`, `written=10416`, `erased=10752`, and USB throughput
  about `13.19 KiB/s` with the safe 256-byte write chunk plus 1 ms delay.
- `dev app run emmc:hello_app alpha beta` and `dev app run emmc:player_min`
  both loaded from the SDRAM stage cache into the fixed D1 App ELF execution
  region and exited with code `0`; `player_min` still uses the diagnostic stub
  display/input capability backend.

QSPI and eMMC are therefore two media backends for the same App Store v1
contract. Future media such as a filesystem, network fetch, or product slot
manager must still stage into `AppImage` before entering `AppRuntime`. Future
M4/Linux/remote execution support must appear as an explicit runtime domain and
capability proxy boundary; it must not be hidden as a normal thread or leak
mailbox/RPC details into `CharmAppApi`.

After a packetstream reaches `launch_ready`, `dev app stage <name>` reads the
verified payload back from the SDRAM2 receive buffer into a 128 KiB SDRAM2
staging scratch buffer and stages it as `AppImage(format=elf)`. `dev app probe <name>`
additionally performs a dry ELF load probe into a private 64 KiB diagnostic
buffer and prints entry offset, load span, segment count, runnable flag, and
the would-be loaded entry address. `dev app prepare <name> [args...]` performs
the same stage + ELF load path and then calls `AppRuntime::prepare()` to
validate the C ABI header and build argv; it stops at the `start` stage and
prints `ready`, `argc`, and the would-be entry address. These commands print
`run=disabled`; they do not call `charm_app_main` and do not jump.

`dev app run <name> [args...]` is the first explicit execution command. The
current payload format is App ELF. The packetstream receiver stores bytes in
the SDRAM2 receive buffer, `dev app run` reads the verified payload back into a
SDRAM2 staging cache, stages it as `AppImage(format=elf)`, wraps it with the shared
staged-image runtime adapter, loads it into the fixed App execution region,
cleans DCache, invalidates ICache, and calls
`charm_app_main(api, argc, argv)`.

The first App execution region is fixed to
`0x24070000..0x24080000` (`64 KiB`). This must match
`Examples/app_abi/elf_samples/app_elf.ld` where `ELF_BASE = 0x24070000`.
`dev app status` prints the region descriptor as:

```text
dev: app run-region name=ram_d1_app_elf base=0x24070000 size=65536 align=16 linked_elf_base=0x24070000
```

The first capability table is minimal and diagnostic-first: console and time
are real, display/input are small in-memory stubs that record present/poll
counters, and storage/AFE remain unsupported. This is a development execution
path, not a product app launcher or a real display/touch backend.

Current board facts: raw UART DMA RX has been validated with real App ELF
payloads. `hello_app.elf` and `player_min.elf` reached `launch_ready`,
`dev app probe`, `dev app prepare`, and `dev app run`, and both returned exit
code `0`. The measured raw UART path is roughly `10 KiB/s` at `115200 8N1`;
pyOCD internal Flash programming remains much slower on the current CMSIS-DAP
path. USB CDC receive is build-wired as the next faster byte-source adapter and
still needs board validation.

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
- Send a USB CDC packetstream after `dev usb begin`:
  `powershell -ExecutionPolicy Bypass -File tools/send-dev-loader-usb-cdc-packetstream.ps1 -PacketStream output.packetstream -UsbPort COMxx`
- Run the repeatable USB CDC App Store transfer smoke with the safe defaults:
  `powershell -ExecutionPolicy Bypass -File tools/capture-dev-loader-usb-cdc-appstore-transfer-smoke.ps1`
- Run the full resident App Store platform smoke with the safe defaults:
  `powershell -ExecutionPolicy Bypass -File tools/capture-dev-loader-usb-cdc-appstore-platform-smoke.ps1`
- Run the full QSPI/eMMC resident App Store platform matrix smoke:
  `powershell -ExecutionPolicy Bypass -File tools/capture-dev-loader-usb-cdc-appstore-platform-matrix-smoke.ps1`
- Run the ELF-only resident platform smoke across received, QSPI, and eMMC:
  `powershell -ExecutionPolicy Bypass -File tools/capture-dev-loader-usb-cdc-elf-platform-smoke.ps1`
- Run the same ELF-only resident platform smoke through raw UART when USB device
  enumeration is unavailable:
  `powershell -ExecutionPolicy Bypass -File tools/capture-dev-loader-raw-elf-platform-smoke.ps1`
- Run only the installed-store persistence matrix, without USB download/install:
  `powershell -ExecutionPolicy Bypass -File tools/capture-dev-loader-installed-store-matrix-smoke.ps1`
- Run the default off-board resident platform evidence bundle:
  `powershell -ExecutionPolicy Bypass -File tools/capture-resident-platform-evidence-bundle.ps1`
- Run the same evidence bundle and append the off-board QEMU ELF virtual-board smoke:
  `powershell -ExecutionPolicy Bypass -File tools/capture-resident-platform-evidence-bundle.ps1 -QemuElf`
  The bundle summary expands the generated QEMU `domain-summary.json` into
  `qemu_elf_domain`, `qemu_elf_display`, `qemu_elf_coverage`, and
  `qemu_elf_player_min_gui` tokens so the archived evidence shows the virtual
  runtime domain, fixed display mode, coverage counts, and GUI timeline without
  opening the JSON manually.
- Run only the off-board QEMU ELF virtual-board smoke:
  `powershell -ExecutionPolicy Bypass -File ../../system/run-resident-elf-qemu-smoke.ps1`
  The QEMU smoke covers direct App ELF plus QEMU-local Store v1 staging into the
  same `AppImage(format=elf) -> AppRuntime` path; it is not a replacement for
  USB/QSPI/eMMC board evidence.
- Run the same evidence bundle and append the QSPI/eMMC board matrix:
  `powershell -ExecutionPolicy Bypass -File tools/capture-resident-platform-evidence-bundle.ps1 -BoardMatrix -UsbPort COMxx`
- Run the same evidence bundle and append only the installed-store persistence matrix:
  `powershell -ExecutionPolicy Bypass -File tools/capture-resident-platform-evidence-bundle.ps1 -InstalledStoreMatrix`
- Inspect the canonical resident-platform artifacts before using the board:
  `Examples/system/resident_platform_inspect_tool/cmake-build-resident-platform-inspect-tool/Debug/resident-platform-inspect.exe Examples/app_abi/elf_samples/out/artifact_manifest.json`
- Sweep USB CDC chunk sizes against the same App Store packetstream:
  `powershell -ExecutionPolicy Bypass -File tools/capture-dev-loader-usb-cdc-throughput-sweep.ps1 -UsbPort COMxx`
- Capture a received App ELF run smoke:
  `powershell -ExecutionPolicy Bypass -File tools/capture-dev-loader-app-run-smoke.ps1 -PacketStream output.packetstream -AppName hello_app -AppArgs "alpha beta" -Expect "hello_app: charm_app_main entered","hello_app: argv1=alpha"`
- Capture a USB CDC received App ELF run smoke:
  `powershell -ExecutionPolicy Bypass -File tools/capture-dev-loader-usb-cdc-app-run-smoke.ps1 -PacketStream output.packetstream -AppName hello_app -AppArgs "alpha beta" -UsbPort COMxx -Expect "hello_app: charm_app_main entered","hello_app: argv1=alpha"`
- Build a development App Store image from sample App ELFs:
  `powershell -ExecutionPolicy Bypass -File ../../app_abi/elf_samples/build_app_store.ps1`
- Send the generated `.appstore.bin` as a packetstream, then run:
  `dev store install qspi`, `dev store list qspi`, and
  `dev app run qspi:hello_app alpha beta`.
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
- raw UART packetstream download reached `launch_ready` for real
  `hello_app.elf` and `player_min.elf` payloads.
- `dev app run hello_app alpha beta` called `charm_app_main`, printed the
  expected argv output, and returned exit code `0`.
- `dev app run player_min` called the same App ABI path, presented one stub
  frame, polled input once, and returned exit code `0`.
- USB CDC packetstream download now reaches `launch_ready` for real
  `hello_app.elf` and `player_min.elf` payloads when the board USB device cable
  is connected. `player_min.elf.packetstream` measured about `50 KiB/s` to
  `launch_ready` on the first successful run.
- Resident App Store v1 has reached the first QSPI NOR board closure. A
  generated `.appstore.bin` containing `hello_app` and `player_min` was received
  as a packetstream payload, installed to QSPI offset `0`, enumerated by
  `dev store list qspi`, staged by name, and executed with
  `dev app run qspi:<name>`.
- The validated App Store payload was `10,416` bytes, carried by an `11,648`
  byte packetstream with CRC `0x73de4894`. USB CDC was not used as the final
  evidence for this larger store transfer because the device COM port became
  unstable during the run; raw UART delivered the same packetstream to
  `launch_ready` at about `7.66 KiB/s`.
- QSPI NOR reported `ready=1`, JEDEC `0x00ef4019`, capacity `32 MiB`,
  erase block `4096`, and write alignment `1`. Store install reported
  `receive=ok`, `store=ok`, `code=ok`, `written=10416`, and `erased=12288`.
- `dev store list qspi` reported two runnable entries:
  `hello_app` at offset `0x70`, size `5132`; `player_min` at offset `0x1480`,
  size `5168`.
- `dev app run qspi:hello_app alpha beta` entered `charm_app_main`, printed the
  expected argv token, loaded at `0x24070000`, and exited with code `0`.
- `dev app run qspi:player_min` loaded from QSPI, presented one stub frame,
  polled input once, and exited with code `0`.
- `capture-dev-loader-usb-cdc-app-run-smoke.ps1` is the current preferred
  repeatable USB App board smoke. It drives the UART control channel through
  `dev packet reset-session`, `dev usb begin`, USB CDC packetstream transfer,
  and `dev app probe/prepare/run/status`, then validates the same
  `lookup/load/abi/argv/start/exit` tokens used by the raw UART App smoke.
- `flash-dev-loader-pyocd.ps1` previously used `100k` SWD and took about 513s
  for a 94 KiB image. The default is now `1000k`; lower it explicitly only when
  probe stability requires it.

The current handoff target after `launch_ready` is now explicit:
`dev app stage/probe/prepare/run` must continue to reuse the shared received
image, App ABI staging, staged-image source adapter, ELF load, and AppRuntime
boundaries. Do not add a direct raw jump path in this monitor.

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
