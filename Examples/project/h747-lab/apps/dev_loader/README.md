# H747 Dev Loader

`dev_loader` is the H747 resident development runtime. It is not a product
bootloader and does not replace `app_lab`. Its platform chain is:

```text
UART/USB packetstream or App Store
-> AppImage
-> staged AppImageSource
-> ELF/ModuleX loader
-> AppRuntime
-> CharmAppApi
```

The monitor binds H747 transports, SDRAM staging, QSPI/eMMC Store v1 media and
the D1 execution region to shared prototypes under
[`Examples/dev_loader`](../../../../dev_loader/README.md) and
[`Examples/app_abi`](../../../../app_abi/README.md). It must not create a second
packet protocol, Store format, App entry or capability table.

## Resource Map

| Use | Preferred region | Capacity | Notes |
|---|---:|---:|---|
| received packetstream | SDRAM2 `0xD0000000` | 256 KiB | `sdram2_receive_buffer` |
| staged image cache | SDRAM2 `0xD0040000` | 128 KiB | `sdram2_stage_cache` |
| receive/stage fallback | D1 `0x24040000` | 128 KiB | used when SDRAM2 smoke fails |
| ELF probe buffer | private RAM scratch | 64 KiB | probe only, not execution |
| App ELF execution | D1 `0x24070000..0x24080000` | 64 KiB | 16-byte alignment |

The ELF execution base must match
[`app_elf.ld`](../../../../app_abi/elf_samples/app_elf.ld). SDRAM is a receive
and stage arena; this target does not execute ELF text from SDRAM. `dev status`
and `dev app status` print the selected arenas and SDRAM2 init/smoke result.

## Monitor Commands

Receive session:

- `dev status`
- `dev begin <size> [crc_hex]`
- `dev fill <hex_byte> <count>`
- `dev verify`
- `dev launch dry-run`
- `dev abort`

Packet and byte-source frontends:

- `dev packet status`
- `dev packet ingest <hex>`
- `dev packet reset`
- `dev packet reset-session`
- `dev raw begin|status|abort`
- `dev usb begin|status|abort`

App Store:

- `dev store status`
- `dev store install qspi|emmc`
- `dev store list qspi|emmc`
- `dev store stage qspi:<name>|emmc:<name>`

App runtime:

- `dev app stage <name>`
- `dev app probe <name>`
- `dev app prepare <name> [args...]`
- `dev app run <name>|qspi:<name>|emmc:<name> [args...]`
- `dev app status`

`dev packet ingest` accepts continuous or space-separated hex pairs. The console
line buffer is 128 bytes, so keep each command to about 48 decoded bytes.
`dev packet reset` clears only partial transport bytes; use
`dev packet reset-session` before replaying sequence zero after a failed session.

## Transport Semantics

Packet v0 remains `begin -> data* -> verify -> launch_dry_run`. Raw UART and USB
CDC are only byte sources for the same
`ByteTransportRuntime -> PacketRuntime -> BinaryReceiveRuntime` chain.

`dev raw begin` makes UART binary-transparent until `launch_ready`, packet error
or transport error. Text escape bytes are not interpreted because they may occur
inside an ELF. A packet-v0 abort can terminate the binary session; `dev raw
abort` is command-mode cleanup.

UART RX prefers circular DMA and falls back to direct RDR polling only if DMA
cannot start. Packet/raw status exposes DMA positions, byte counts, fallback and
overrun counters. Treat growing overrun or fallback counters as frontend loss,
not an ELF or packet semantic failure.

`dev usb begin` starts an exclusive CDC packetstream receiver while UART remains
the control console. The frontend uses:

```text
packet_buffer=1024 max_payload=256 read_chunk=512 drain_limit=4096
```

USB mode exits with `launch_ready`, `packet_error`, `transport_error` or
`abort`. `dev usb abort` is idempotent cleanup and stops the USB device; it does
not restore a previous USB function. A later App owns any USB reinitialization.

The safe host default remains 256-byte writes with a 1 ms inter-chunk delay.
Larger writes and zero delay are explicit throughput tests. A failed transfer
must preserve `exit=`, packet/transport result and USB dropped/overflow counters.

## Store And Image Semantics

Store v1 keeps one byte-range layout for QSPI and eMMC. QSPI installs at offset
zero. eMMC uses a fixed 16 MiB raw development slot at the tail of the exposed
block window; it is not a filesystem, product partition, slot manager or
manifest service. The eMMC adapter performs 512-byte read-modify-write when a
Store range is not block aligned.

Installation always uses `received_image_read -> app_store_install_image` with
readback verification. List, stage and run use
`AppStoreReader -> app_store_stage_named_image -> staged AppImageSource`.
Monitor code must not duplicate erase/write/verify or image lookup semantics.

Store entry format is encoded in `AppStoreEntry.flags & 0xF`:

- `0`: ELF
- `1`: ModuleX

ELF is the primary resident format. ModuleX is a second image format, not a
second App model: its entry remains `charm_app_main(api, argc, argv)`. ModuleX
v1 is materialized-only, supports local/global/external symbols plus `abs_addr`
and `rel32` relocation, and rejects non-zero BSS and XIP flags.

## App Stages And Diagnostics

`stage` reads the verified payload and creates an `AppImage`. `probe` validates
and dry-loads ELF metadata. `prepare` also validates the App ABI and argv but
stops before entry. `run` loads into the fixed execution region, prepares cache,
and calls `charm_app_main` through `AppRuntime`.

`dev app status` provides the canonical run record:

```text
dev: app record source=<received|qspi|emmc> format=<elf|modulex> name=<app>
  command=<stage|probe|prepare|run> argv=<argc> load=<addr> entry=<addr>
  span=<bytes> segments=<n> run_stage=<idle|lookup|load|abi|argv|start|exit>
  run_code=<code> exited=<0|1> exit=<code>
  caps_console=<bytes> caps_present=<n> caps_input=<n>
```

ELF capacity diagnostics are fixed to the H747 run region:

```text
dev: app run-region name=ram_d1_app_elf base=0x24070000 size=65536 align=16 linked_elf_base=0x24070000
dev: app capacity needed=<load_span> free=<bytes> fits=<0|1> region=65536 probe=<code>
```

An over-limit ELF must fail at probe/load with `load_buffer_too_small`; it must
not jump, partially start or silently switch to SDRAM execution. ModuleX status
adds `modulex`, `validate`, `dep`, `entry_off`, `span` and `relocated` fields.

The current `CharmAppApi` backend is diagnostic-first: console and time are
real; display and input are in-memory counters; storage and AFE are unsupported.
This target does not own Player UI or real display/touch policy.

## Recommended Validation

Run commands below from `Examples/project/h747-lab`.

Canonical off-board evidence, without flashing or opening serial/USB:

```powershell
powershell -ExecutionPolicy Bypass -File ../../app_abi/elf_samples/build_resident_platform_artifacts.ps1 -Validate
powershell -ExecutionPolicy Bypass -File tools/capture-resident-platform-evidence-bundle.ps1
```

Add the QEMU ELF virtual-board evidence:

```powershell
powershell -ExecutionPolicy Bypass -File tools/capture-resident-platform-evidence-bundle.ps1 -QemuElf -SkipH747Build
```

Build the resident firmware with the existing preset:

```powershell
cmake --build --preset build-h747-lab-dev-loader-debug -- -j1
```

Board regressions:

```powershell
# received, QSPI and eMMC ELF
powershell -ExecutionPolicy Bypass -File tools/capture-dev-loader-usb-cdc-elf-platform-smoke.ps1

# mixed ELF/ModuleX Store on QSPI and eMMC
powershell -ExecutionPolicy Bypass -File tools/capture-dev-loader-usb-cdc-appstore-platform-matrix-smoke.ps1

# full evidence bundle plus board matrix
powershell -ExecutionPolicy Bypass -File tools/capture-resident-platform-evidence-bundle.ps1 -BoardMatrix -UsbPort COMxx

# reuse already-installed stores without download/install
powershell -ExecutionPolicy Bypass -File tools/capture-dev-loader-installed-store-matrix-smoke.ps1
```

Focused helpers remain available for raw UART, USB app-run, transfer repeat and
throughput sweep under `tools/`. Prefer the evidence bundle or matrix entry over
copying token logic into a new script.

## Board Evidence And Failure Classification

The current resident build has validated:

- raw UART and USB CDC packetstreams reaching `launch_ready` with matching CRC
  and no dropped/overflow bytes under the safe USB defaults;
- zero-delay USB writes at 256, 512 and 1024-byte host chunk sizes;
- SDRAM2 receive/stage arenas after applying `storage_stage_a` before memory
  smoke; earlier fallback was initialization order, not a partial SDRAM bank;
- mixed Store install/list/run on QSPI and eMMC;
- received, QSPI and eMMC ELF Apps entering the same AppRuntime and exiting zero;
- received and stored `modulex_hello_app` relocation and exit zero;
- eMMC wide mode at `bus=8`, `clkcr=0x00008006`, `sta=0x00000000`, with
  1-bit/div16 retained as fallback.

Classify failures before changing upper layers:

- USB `setup=0 reset=0 connect=0` on both CDC and comparison firmware indicates
  the physical host/device path, commonly a missing device cable.
- `transport_error/buffer_too_small` or growing drop/overflow counters indicate
  byte-source pacing/buffering, not Store, ELF or AppRuntime.
- UART overrun/fallback growth indicates RX frontend loss.
- `launch_ready` with Store failure belongs to install/media validation.
- `lookup/load/abi/argv/start/exit` identifies the AppRuntime failure stage.
- pyOCD success requires process exit code zero; CMSIS-DAP AP warnings alone do
  not prove firmware failure.

## Boundaries

- No product boot policy, signature, rollback, slot selection or crash recovery.
- No direct raw jump; every App enters through `AppImage` and `AppRuntime`.
- No composite USB requirement and no automatic USB state restoration.
- No SDRAM ELF execution, MPU sandbox, process isolation or scheduler contract.
- No Player UI, display, touch or product storage policy in this monitor.

The H747 role split and migration status are tracked in
[`h747_lab_dynamic_boundary_roadmap.md`](../../docs/h747_lab_dynamic_boundary_roadmap.md).
