# Charm Dev Loader Prototype

This directory holds the board-free resident dev-loader prototype shared by
host smokes and H747 monitor wiring.

The first-generation contract is intentionally small:

- `Session` owns the image receive state machine.
- `Storage` is a callback-backed byte sink/source for RAM or future media.
- `BinaryReceiveRuntime` owns the transport-neutral begin/write/verify path.
- `CommandRuntime` maps text commands onto the same `Session` transitions.
- `PacketRuntime` maps transport packet v0 onto the same binary receive path.
- `Packet stream` helpers build and replay raw packet v0 byte streams before a
  real USB or serial transport exists.
- `Packet console` helpers convert raw packetstream bytes into H747 console
  `dev packet ingest <hex>` command lines for line-mode board smoke.
- `ByteTransportRuntime` buffers arbitrary incoming bytes and dispatches full
  packet v0 frames into `PacketRuntime`.
- `ByteTransportRuntime::reset()` only clears buffered bytes; frontends that
  start a new packetstream session from sequence 0 must call
  `reset_session()`.
- `hex_decode_bytes` provides the console-friendly packet ingest path used by
  H747 `dev_loader`.
- `received_image_read` exposes a read-only view of a `launch_ready` received
  image for AppRuntime handoff experiments.
- H747 `dev app stage/probe/prepare/run` consumes that received-image view and
  the App ABI ELF load/AppRuntime helper chain.
- `dev launch dry-run` remains the transport-neutral receive-session marker;
  App execution is an explicit monitor command, not a raw jump.

Supported command-layer verbs:

- `dev status`
- `dev begin <size> [crc_hex]`
- `dev fill <hex_byte> <count>`
- `dev verify`
- `dev launch dry-run`
- `dev abort`

The prototype does not implement board reset, a product bootloader jump path,
or product update policy. H747 frontends should keep console/UART/USB parsing
thin and reuse this command/session path instead of creating a second download
model.

Board-free validation entry points:

- `Examples/system/dev_loader_session_smoke`
- `Examples/system/dev_loader_command_smoke`
- `Examples/system/dev_loader_binary_receive_smoke`
- `Examples/system/dev_loader_packet_smoke`
- `Examples/system/dev_loader_packet_stream_smoke`
- `Examples/system/dev_loader_packet_console_smoke`
- `Examples/system/dev_loader_raw_uart_smoke`
- `Examples/system/dev_loader_byte_transport_smoke`
- `Examples/system/dev_loader_hex_ingest_smoke`
- `Examples/system/dev_loader_store_receive_smoke`
- `Examples/system/dev_loader_app_handoff_smoke`
- `Examples/system/dev_loader_received_elf_smoke`
- `Examples/system/dev_loader_stage_probe_smoke`

`dev_loader_store_receive_smoke` is the current bridge proof between App Store
v1 install/staging and the transport-neutral receive path. It does not add a
product manifest, USB transfer, or real jump; it proves that bytes installed to
a flash-like medium and staged from the same external program-store shape can be
fed into the RAM receive state machine.

`dev_loader_packet_smoke` freezes the command-independent packet semantics for
future USB/serial/host transports. It does not define a USB framing layer,
retry/window policy, or product launch behavior.

`dev_loader_packet_stream_smoke` and `dev-loader-packet-stream` add the sending
side of that same semantic path. The stream format is repeated little-endian
`PacketHeader` bytes plus payload bytes:

```text
begin -> data* -> verify -> optional launch_dry_run
```

This is still transport-neutral. Future USB or serial frontends should move this
byte stream and feed decoded packets into `PacketRuntime`; they must not invent
a second begin/data/verify state machine.

`dev_loader_packet_console_smoke` and `dev-loader-packet-console` add the
line-mode H747 console adapter. The tool converts a `.packetstream` file into
multiple `dev packet ingest <hex>` lines with a default of 48 packetstream bytes
per line. It does not add `reset`, `status`, or `abort`; monitor state control
stays explicit.

`dev_loader_byte_transport_smoke` proves the frontend boundary for real byte
transports. USB CDC, serial, or a host bridge may deliver arbitrary chunk sizes;
the adapter buffers bytes, decodes complete packet v0 frames, and dispatches
them into the same `PacketRuntime`.

`dev_loader_hex_ingest_smoke` proves the H747 console frontend shape. It decodes
hex text into bytes, feeds the byte transport in small chunks, and reaches the
same launch-ready dry-run state without raw serial mode.

`dev_loader_raw_uart_smoke` proves the H747 raw UART frontend semantics without
opening a serial port. It simulates `dev raw begin`, arbitrary raw byte chunks,
automatic exit at `launch_ready`, packet failure exit, packet v0 `abort`, and a
second download that restarts from packet sequence 0. USB CDC or USB bulk should
reuse this same byte-ingest boundary.

The H747 `dev usb begin` frontend is the first USB byte-source adapter for that
same boundary. It is intentionally mode-exclusive: the resident monitor owns USB
only while receiving packetstream bytes, stops/disconnects the USB device after
`launch_ready` or abort, and does not promise to restore a previous USB
function. Apps that need USB later must initialize their own backend.

`dev_loader_app_handoff_smoke` is the first received-image to AppRuntime bridge.
It reads a `launch_ready` payload back from storage, stages it as an `AppImage`,
and runs a fake `charm_app_main` through `AppRuntime`. It still does not execute
H747 ELF, jump to RAM, or add USB transport.

`dev_loader_received_elf_smoke` pushes the same bridge to real App ELF bytes.
It receives `hello_app.elf` and `player_min.elf` through packetstream semantics
and verifies ELF load metadata through the App ABI ELF loader backend. It also
passes the loaded ELF image through `AppRuntime::prepare()` so the
`lookup/load/abi/argv/start` chain is covered without executing Arm code on
host.

`dev_loader_stage_probe_smoke` matches the H747 `dev app stage/probe` frontend:
read the `launch_ready` payload, stage it as `AppImage(format=elf)`, call the
ELF dry load path, and materialize the would-be `LoadedAppImage` entry address
for diagnostics only. It deliberately stops before `AppRuntime` or
`charm_app_main`.

The H747 `dev app run` frontend is the board-side closure of the same chain:

```text
packetstream/raw UART -> launch_ready payload
  -> received_image_read
  -> app_received_image_stage(format=elf)
  -> app_elf_load_image
  -> AppRuntime::run()
  -> charm_app_main(api, argc, argv)
```

The current H747 App ELF samples are linked for `ELF_BASE = 0x24070000`; the
resident monitor therefore loads executable App ELF bytes into
`0x24070000..0x24080000`. Future USB, QSPI/eMMC, or ModuleX work must preserve
the same `AppImage + AppRuntime + CharmAppApi` handoff shape.
