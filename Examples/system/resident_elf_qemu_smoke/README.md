# Resident ELF QEMU Smoke

This is the first virtual-board backend smoke for the resident ELF platform.
It does not emulate H747 peripherals. It runs a small Cortex-M7 firmware on
QEMU `mps2-an500`, generates App ELFs linked to the QEMU run region, loads them
through `app_elf_load_image()`, and executes them through `AppRuntime`. It also
stages `hello_app` and `player_min` through the received-image boundary, replays
`hello_app` and `player_min` through the dev_loader packetstream byte transport to `launch_ready`,
builds a minimal
in-memory Store v1 image, and stages `hello_app`, `player_min`,
`argv_app`, `bss_app`, `data_app`, and `large_fit_app` back out of that Store before running
them through the same staged runtime adapter. `argv_app` validates that AppRuntime builds the
same `argc/argv` list for direct and Store-backed QEMU ELF runs. The
packetstream path also builds one `hello_app` stream with 257-byte packet payloads, then replays
that same immutable stream through fresh `ByteTransportRuntime` and `PacketRuntime` instances
with 1, 27, 113, 256, and 512-byte ingress chunks. Each replay must reach `launch_ready`, retain
the verified CRC, read the complete received image, and reproduce every payload byte. This is a
raw byte-stream fragmentation contract check; it does not model USB CDC throughput or H747 USB
peripherals.
`prepare:argv_app` case validates the prepare-only path: ELF load and argv
materialization complete, but the App entry is not called and no capability is
touched. `console_error_app` validates that `console.write(nullptr, nonzero)`
is rejected as `INVALID_ARGUMENT` without counting the failed write in both
direct and Store-backed paths. The
`data_app` validates that the ELF loader initializes `.data` from the image on
each direct and Store-backed run, while still zero-filling `.bss`. The
`argv_overflow_app` negative case validates that an AppRuntime `argv` failure
does not enter the App or touch capabilities. `abi_mismatch_app` validates the
same no-entry/no-capability rule for invalid `CharmAppApi` metadata. `exit_app`
validates the QEMU `app.exit` capability in both direct and Store-backed paths.
`exit_error_app` validates that `app.exit(7)` is a capability notification and
does not override the `charm_app_main` return value recorded by AppRuntime.
`exit_negative_app` validates signed `app.exit` diagnostics: `app.exit(-3)` is
printed as a signed code while AppRuntime still records the App return value.
`return_negative_app` validates signed AppRuntime exit diagnostics:
`charm_app_main` can return `-5`, and direct/Store-backed run records preserve
that signed value.
`unsupported_caps_app` validates from App code
that unsupported QEMU storage/AFE calls are explicit `UNSUPPORTED` stubs.
`afe_error_app` validates the AFE unsupported boundary independently from
storage: configure/read return `UNSUPPORTED`, preserve the caller buffer, and
only increment the AFE capability counters in both direct and Store-backed paths.
`storage_app` validates a smoke-local read-only virtual storage capability in
both direct and Store-backed paths. `storage_catalog_app` validates multiple
read-only virtual files and simultaneous file descriptors in both paths.
`storage_close_error_app` validates storage fd lifetime: a duplicate close is
`UNSUPPORTED`, the fd slot is reusable, and the reopened file starts at offset
zero.
`storage_error_app` validates that an invalid read buffer on a valid fd is
reported as `INVALID_ARGUMENT` without advancing the file cursor in both paths.
`storage_fd_exhaustion_app` validates the fixed QEMU storage fd table: opening
all four slots makes the next open fail as `IO_ERROR`, and closing one slot lets
the next open reuse that fd at offset zero.
`storage_open_error_app` validates that `open(nullptr)` is `INVALID_ARGUMENT`,
unsupported open flags/mode are rejected without allocating an fd slot, and the
next valid open still returns fd `3` at offset zero in both paths.
`storage_write_error_app` validates that the QEMU storage backend is read-only:
`write` is rejected as `UNSUPPORTED` without changing the file cursor or fd
lifetime in both paths.
`storage_zero_io_app` validates zero-length storage I/O semantics: a valid
zero-length read succeeds without advancing the file cursor, and a zero-length
write is still rejected by the read-only backend without advancing the cursor.
`display_describe_error_app` validates that `display.describe(nullptr)` is
rejected as `INVALID_ARGUMENT` without counting a successful describe or
emitting display evidence.
`display_error_app` validates that an invalid `display.present` byte count is
rejected as `INVALID_ARGUMENT` in both direct and Store-backed paths without
emitting a frame signature, frame dump, or GUI timeline frame.
`display_null_present_app` validates the same no-frame-evidence rule for a null
framebuffer pointer with the otherwise correct frame byte count.
`display_sequence_app` validates multi-frame display present accounting in both
direct and Store-backed paths. `input_error_app` validates that
`input.poll(nullptr)` is rejected as `INVALID_ARGUMENT` in both direct and
Store-backed paths without emitting an input trace event. The `time_app`
validates from App code that QEMU
virtual time advances by a
deterministic 17 ms step. `time_sequence_app` validates that each App run starts
from the deterministic QEMU virtual time base and advances through the same
four-sample sequence in both direct and Store-backed paths. `large_fit_app` validates a near-limit successful ELF
load with a 60 KiB BSS-backed span inside the 64 KiB QEMU run region through
direct, received-image, packetstream-received, and Store-backed entries. The
Store also contains `too_large_store_app` as a
padded negative stage-cache case; it must fail before loader/AppRuntime. The
separate `missing_app` lookup case must also fail before loader/AppRuntime. The
separate `received_too_large_app` case validates received-image stage-cache
overflow before loader/AppRuntime. The `packetstream_crc_mismatch` negative case
validates that a corrupted packetstream stops at packet verify with no
`received_image_read()`, no App staging, no AppRuntime entry, and no capability
calls. `packetstream_bad_elf_magic_app` validates the opposite boundary: the
packetstream reaches `launch_ready`, `received_image_read()` and staging both
succeed, and only the ELF loader rejects the payload as `bad_magic`.
`entry_outside_segment_app` and `rwx_segment_app` mutate a known-good ELF in
memory and prove that the QEMU resident domain rejects unsafe ELF structure at
the loader boundary. `bad_header_app`, `bad_class_app`, `bad_endian_app`,
`bad_ident_version_app`, `bad_type_app`, `bad_machine_app`, `bad_version_app`,
`bad_ehsize_app`, `bad_phentsize_app`, `bad_program_header_app`,
`truncated_payload_app`, `no_load_segment_app`, and `overlapping_segments_app`
extend the same mutation matrix to ELF header ABI, program-header,
payload-boundary, load-segment, and segment-overlap failures.
`wrong_link_base_app` keeps the ELF structurally valid but shifts its `PT_LOAD`
virtual addresses away from the QEMU domain run region; probe still reports
`ok`, but the QEMU load backend must reject it before AppRuntime reaches start.
`too_large_app` remains the ELF load-span negative case. The
`unaligned_load_buffer_app` case validates the runtime-domain arena contract:
if the QEMU backend supplies an unaligned ELF load buffer, the loader must stop
at `load_buffer_unaligned` before AppRuntime reaches start. `bad_elf_magic_app`
validates malformed ELF rejection at AppRuntime load stage without entering the
App or touching capabilities.

The purpose is to validate the architecture seam:

```text
App ELF
-> AppImage(format=elf)
-> ELF loader
-> AppRuntime
-> CharmAppApi virtual capability backend

Store v1 bytes
-> app_store_stage_named_image()
-> AppImage(format=elf)
-> ELF loader
-> AppRuntime
-> CharmAppApi virtual capability backend

Received ELF bytes
-> app_received_image_stage()
-> AppImage(format=elf)
-> ELF loader
-> AppRuntime
-> CharmAppApi virtual capability backend

Packetstream ELF bytes
-> ByteTransportRuntime
-> PacketRuntime
-> BinaryReceiveRuntime launch_ready
-> received_image_read()
-> app_received_image_stage()
-> AppImage(format=elf)
-> ELF loader
-> AppRuntime
-> CharmAppApi virtual capability backend

Corrupted Packetstream ELF bytes
-> ByteTransportRuntime
-> PacketRuntime verify failure
-> BinaryReceiveRuntime failed/crc_mismatch
-> no received_image_read()
-> no AppRuntime
-> no capability calls

Malformed ELF bytes in valid Packetstream
-> ByteTransportRuntime
-> PacketRuntime
-> BinaryReceiveRuntime launch_ready
-> received_image_read()
-> app_received_image_stage()
-> AppImage(format=elf)
-> ELF loader bad_magic
-> AppRuntime load-stage failure
-> no capability calls

App ELF with BSS
-> AppImage(format=elf)
-> ELF loader zero-fill
-> AppRuntime
-> CharmAppApi virtual capability backend

App ELF with initialized data
-> AppImage(format=elf)
-> ELF loader copies .data and zero-fills .bss
-> AppRuntime
-> CharmAppApi virtual capability backend

Near-limit App ELF with BSS
-> AppImage(format=elf)
-> ELF loader zero-fill
-> AppRuntime
-> capacity needed/free/fits evidence

Oversized App ELF
-> AppImage(format=elf)
-> ELF loader
-> AppRuntime load-stage failure

Malformed App ELF
-> AppImage(format=elf)
-> ELF loader bad_magic
-> AppRuntime load-stage failure
-> no capability calls

Unsafe App ELF structure
-> AppImage(format=elf)
-> ELF loader bad_class / bad_endian / bad_program_header / truncated_payload
   / bad_ident_version / bad_type / bad_machine / bad_version / bad_ehsize
   / bad_phentsize / no_load_segment / entry_outside_segment
   / overlapping_segments / rwx_segment
-> AppRuntime load-stage failure
-> no capability calls

Prepare-only App ELF
-> AppImage(format=elf)
-> ELF loader
-> AppRuntime prepare
-> no App entry/capability calls

Oversized received payload
-> app_received_image_stage()
-> received stage-cache failure
-> no ELF loader/AppRuntime/capability calls

Invalid CharmAppApi metadata
-> ELF loader ok
-> AppRuntime ABI-stage failure
-> no App entry/capability calls
```

The QEMU run region is `0x20080000..0x20090000`. This is intentionally separate
from the H747 dev_loader run region `0x24070000..0x24080000`; each runtime
domain owns its platform memory map.

`qemu_virtual_backend.hpp/.cpp` is the smoke-local virtual capability backend.
It owns the CMSDK UART log sink and the current virtual `CharmAppApi`
implementation for console, time, display, input, read-only storage, and
app.exit counters. Unsupported storage calls and all AFE calls are explicit
`CHARM_APP_STATUS_UNSUPPORTED` stubs, not null function pointers. Per-run
capability diagnostics include `afe=configure/read` so AFE probes can be checked
without conflating them with storage counters. The main smoke code should stay
focused on Store staging, ELF loading, AppRuntime, and diagnostics. This split is
not a public Backends API and does not introduce a second App model.
The smoke logs `resident-elf-qemu: backend-capabilities ...`,
`resident-elf-qemu: backend-scope ...`, and
`resident-elf-qemu: backend-contract ...` directly from `qemu_virtual_backend`;
`domain-summary.json` records the boundary declaration under `backend_scope`
and the virtual capability details under `backend_contract`. The contract is the
virtual-board evidence contract for this smoke: runtime domain `virtual_m7`,
virtual capabilities `console,time,display,input,storage,app_exit`, read-only
storage, and unsupported AFE. It is not a product backend registry.
The `backend-scope` line is a log-level boundary declaration: QEMU proves the
resident ELF loader/AppRuntime/CharmAppApi/source semantics, but does not prove
H747 USB CDC, QSPI, eMMC, FMC SDRAM, HAL init, MPU/cache, or pinmux behavior.
The runtime also logs `resident-elf-qemu: backend-self-check ...` from the same
backend implementation. That line checks the generated `CharmAppApi` function
table, display/input/storage constants, unsupported AFE boundary, and
`app.exit` notification semantics against the declared virtual backend contract
before any App is run. `domain-summary.json` records the same result as
`backend_self_check`, and validate-only/exported backend-contract checks reject
missing or failed self-check evidence.
The runtime also logs `resident-elf-qemu: backend-reset-self-check ...` after
dirtying the smoke-local counters, virtual time cursor, input cursor, display
counters, and storage fd table, then resetting the backend. `domain-summary.json`
records that result as `backend_reset_self_check`. This proves QEMU virtual
backend reset determinism before the App matrix starts; it does not prove H747
peripheral reset or board power sequencing.
`domain-summary.json` also records a derived `backend_readiness` section. That
section is intentionally redundant with lower-level coverage: it must report
`status=ready` and true gates for ELF loader, AppRuntime, received bytes,
packetstream, Store, equivalent sources, GUI evidence, storage, input, and the
unsupported-capability boundary.
`backend_capability_matrix` indexes the smoke-local capability evidence by
capability name: provider kind, policy, evidence artifacts, and representative
runs. It is a regression/audit index for the QEMU virtual backend, not a public
Backends API or profile binding schema.
The runtime log and machine-readable summary both pin smoke-local backend
semantics: deterministic time starts at `1000 ms` and advances by `17 ms`,
display is a 16x16 ARGB8888 framebuffer with frame signature/dump/PPM evidence,
input is a four-sample wrapping deterministic sequence, storage is a three-file
read-only virtual media with fd base `3` and four fd slots, and `app.exit` is a
notification counter that does not override the App return value.

The QEMU-local Store path is exposed through a smoke-local virtual memory media
reader owned by `qemu_virtual_backend`. Its counters are logged as
`resident-elf-qemu: store-media ...` and recorded in `domain-summary.json` so
Store-backed runs prove they crossed the runtime-domain backend seam. This is
still not QSPI, eMMC, a block device, or a filesystem.

The virtual storage backend exposes a small deterministic read-only file table:
`/virtual/readme.txt`, `/virtual/alpha.txt`, and `/virtual/beta.bin`. This is a
capability semantics proof with multiple file descriptors, not a filesystem,
block device, or App Store media.
The smoke writes `storage-trace.json` and compares it with
`storage-trace.golden.json` so open/read/close ordering, fd assignment, byte
counts, EOF behavior, invalid open/read handling, read-only write rejection, and
unsupported storage stubs are regression-gated.

The virtual display mode is fixed at 16x16 ARGB8888 with a 64-byte stride and
1024-byte frame payload. `display.describe` logs that mode explicitly, and
`domain-summary.json` records it as the QEMU display contract. The virtual
display backend logs both byte count and a deterministic byte-sum checksum for
presented frames. The same checksum is also included in the per-run capability
counters so direct and Store-backed runs can prove which App presented the
frame. A stronger FNV-1a-style frame hash and frame index are also logged, and
cumulative checksum/hash fields are recorded for multi-frame smokes. This is
intentionally a smoke-level content proof, not a framebuffer protocol or GUI
backend API. Invalid frame sizes are logged as `code=invalid_argument` but are
not emitted as frame signatures, frame dumps, PPM images, or GUI timeline
frames.

The virtual input backend exposes a deterministic four-step event sequence and
logs each returned state. `player_min` still sees the first event:
encoder1 delta `1`, pointer `(3,5)`, max `(15,15)`, detected `1`, down `0`.
`input_sequence_app` consumes all four events and verifies the sequence checksum.
`input_wrap_app` consumes six events and verifies that the fifth and sixth polls
wrap back to the first two virtual input states.
The smoke writes `input-trace.json` and compares it with
`input-trace.golden.json` so input ordering is a regression gate, not just a
loose log token. Invalid input polls are logged as `code=invalid_argument` but
are not emitted as input trace events.

## Run

```powershell
..\run-resident-elf-qemu-smoke.ps1
```

The wrapper owns the supported command-line surface and forwards to
`resident_elf_qemu_smoke/run_qemu_ci.ps1`; keep the wrapper default wait budget
aligned with the direct script default at `-TimeoutSec 15`. The local
`run_qemu_ci.ps1` script remains the direct implementation entry for lower-level
debugging. Both `-SelfTest` and `-DryRun` print the effective `timeout_sec` and
`tail_lines` values so standalone QEMU runs expose the same run-budget evidence
as the resident platform evidence bundle.

Use `..\run-resident-elf-qemu-smoke.ps1 -Doctor` as the supported host
environment check before occupying the QEMU run path. It resolves CMake, QEMU,
Arm GCC, the host compiler, QEMU source inputs, golden evidence files, output
paths, and confirms that the local QEMU build advertises `mps2-an500`; it does
not build, launch firmware, or validate runtime evidence. The doctor also prints
`qemu_version`, `cmake_version`, `cc_version`, `host_compiler_version`,
`evidence_dir`, `qemu_log`, `qemu_err_log`, `evidence_lock`,
`firmware_elf_load_base`, `elf_base`, `backend_scope_proves`, and
`backend_scope_does_not_prove` so environment drift, ELF domain layout drift,
QEMU evidence scope, and evidence routing are visible in captured logs. Use
`..\run-resident-elf-qemu-smoke.ps1 -DryRun` as the cheapest supported command
expansion check: it resolves the QEMU, compiler, generated artifact, evidence,
and run-budget paths without building or launching QEMU. Use
`..\run-resident-elf-qemu-smoke.ps1 -SelfTest` for a fast preflight that
checks the local CMake, QEMU, Arm GCC, sample source, linker script, and QEMU
`ELF_BASE` assumptions without building or launching QEMU. It also exercises
the domain-summary source-matrix assertions with synthetic direct, received,
packetstream, Store, prepare, and failure entries so those gates fail before a
full QEMU run is needed, and mutates a golden domain summary to confirm bad App
model or packetstream failure-boundary fields are rejected. Use
`-ValidateEvidenceBundle` to revalidate the existing
`qemu-ci.log`, frame signatures, frame dumps, PPM frames, input trace, storage
trace, domain summary, backend contract, and checked-in golden files without
rebuilding or launching QEMU. If `backend-contract.json` is missing from an old
evidence directory, validation derives it from the already validated
`domain-summary.json` and validates the derived file before passing. The default
full-run wait budget is 15 seconds; pass
`-TimeoutSec <seconds>` when using slower hosts or temporary extra diagnostics.
Pass `-EvidenceDir <dir>` to place the generated QEMU evidence set under an
independent directory. That directory owns `qemu-ci.log`, `qemu-ci.err.log`,
`qemu-evidence.lock`, trace JSON, frame dumps, PPM frames, and
`domain-summary.json`; full QEMU runs also write the derived
`backend-contract.json`, and validate-only runs can recreate it from
`domain-summary.json` when absent. Checked-in golden files still come from
`resident_elf_qemu_smoke`. For fully independent QEMU runs, also pass distinct
`-BuildDir` and `-AppOutDir` values so generated firmware, generated App ELFs,
and evidence files do not share writable paths.

Without explicit path arguments, the smoke reuses one ignored working root:
`cmake-build-resident-elf-qemu`. Its `apps` and `evidence` children contain all
generated App ELF and QEMU evidence output, keeping source directories and
`Examples/app_abi/elf_samples/out-qemu` free of routine run artifacts.
Use
`-ValidateLog resident_elf_qemu_smoke/qemu-ci.log` to classify an existing QEMU
log without rebuilding or launching QEMU. Use
`-ValidateFrameSignatures resident_elf_qemu_smoke/frame-signatures.json` to
validate an existing frame signature capture. The default full smoke also
compares the generated frame signatures with
`frame-signatures.golden.json`; pass `-SkipGoldenFrameSignatures` only for
temporary experiments where visual-output drift is expected. Use
`-ValidateFrameDumps resident_elf_qemu_smoke/frame-dumps.json` to validate the
full hex framebuffer dump capture. The default full smoke also compares the
generated frame dumps with `frame-dumps.golden.json`; pass
`-SkipGoldenFrameDumps` only for temporary experiments where pixel-level output
drift is expected. Use
`-ValidateFramePpm resident_elf_qemu_smoke/frame-ppm` to validate the generated
visual PPM frame files and their manifest. Use
`-ValidateInputTrace resident_elf_qemu_smoke/input-trace.json` to validate the
deterministic virtual input sequence. The default full smoke also compares the
generated input trace with `input-trace.golden.json`; pass
`-SkipGoldenInputTrace` only for temporary experiments where input behavior is
expected to drift. Use
`-ValidateStorageTrace resident_elf_qemu_smoke/storage-trace.json` to validate
the deterministic virtual storage sequence. The default full smoke also
compares the generated storage trace with `storage-trace.golden.json`; pass
`-SkipGoldenStorageTrace` only for temporary experiments where storage behavior
is expected to drift.
Use
`-ValidateDomainSummary resident_elf_qemu_smoke/domain-summary.json` to validate
the machine-readable `virtual_m7` runtime-domain summary. The default full smoke
also compares the generated summary with `domain-summary.golden.json` after
canonicalizing environment-specific paths; pass `-SkipGoldenDomainSummary` only
for temporary experiments where the backend contract or coverage matrix is
expected to drift. Use
`-ValidateBackendContract resident_elf_qemu_smoke/backend-contract.json` to
validate only the extracted virtual backend contract evidence. This file uses
schema `charm.resident_elf_qemu.backend_contract.v1` and is a narrow copy of the
backend scope, backend contract, backend self-check, backend readiness,
capability matrix, and runtime-domain profile from `domain-summary.json`; use it
when backend identity, boundary, or capability semantics need a cheap
single-file gate without treating the full run matrix as the changed artifact.
Use
`-CompareDomainSummary expected.json -ActualDomainSummary actual.json`,
`-CompareStorageTrace expected.json -ActualStorageTrace actual.json`, or
`-CompareFrameDumps expected.json -ActualFrameDumps actual.json` to compare two
captures after canonicalizing environment-specific fields such as log paths.

The script builds QEMU-specific App ELF artifacts with `ELF_BASE=0x20080000`,
uses the shared `app-abi-store-pack` tool to build a QEMU-local Store v1 image
containing those ELFs, embeds the generated bytes into the firmware, builds the
firmware, launches QEMU, and checks a fixed token set. `domain-summary.json`
also records the generated QEMU artifact directory, App ELF count, Store-backed
App count, generated include count, and representative size/CRC facts for
`appstore.bin`, `hello_app`, `player_min`, `large_fit_app`, `too_large_app`, and
`too_large_store_app`. This makes the virtual backend evidence identify the
actual ELF/Store inputs it executed instead of only recording the runtime
result.

The full smoke and `-ValidateEvidenceBundle` both consume the same evidence files
(`qemu-ci.log`, trace JSON, frame dumps, PPM frames, `domain-summary.json`, and
the derived `backend-contract.json`) inside the selected evidence directory. `run_qemu_ci.ps1`
serializes those two whole-evidence paths with `qemu-evidence.lock` in the same
directory so a full QEMU run cannot rewrite the log while an evidence-bundle
validation is reading it. Single-file validators, doctor, selftest, and dry-run
do not take this lock. Use independent evidence directories for parallel CI
jobs or local experiments that should not overwrite the canonical checked-out
evidence files.

The QEMU App artifact list is owned by `Get-QemuAppSpecs` in
`run_qemu_ci.ps1`. The same specs drive source selection, generated `.elf.inc`
requirements, and Store pack arguments. `CMakeLists.txt` only consumes the
script-provided `CHARM_QEMU_REQUIRED_INC_FILES` list and checks that those
generated includes exist, so adding a QEMU ELF fixture does not require
maintaining a second CMake-side artifact list.

The full smoke checks these representative tokens:

- `resident-elf-qemu: stage-cache bytes=16384`
- `resident-elf-qemu: backend-contract time=deterministic_tick start_ms=1000 step_ms=17 reset_per_run=1`
- `resident-elf-qemu: backend-contract display=framebuffer width=16 height=16 stride=64 format=argb8888 frame_bytes=1024 evidence=frame_signatures,frame_dumps,frame_ppm,gui_timeline`
- `resident-elf-qemu: backend-contract input=deterministic_sequence sample_count=4 pointer_max=15,15 wraps=1 evidence=input_trace,gui_timeline`
- `resident-elf-qemu: backend-contract storage=virtual_readonly_files file_count=3 fd_base=3 fd_slots=4 write_policy=unsupported evidence=storage_trace`
- `resident-elf-qemu: backend-contract app_exit=notification_counter overrides_return=0`
- `resident-elf-qemu: backend-self-check api=1 display=1 input=1 storage=1 afe=1 app_exit=1 result=ok`
- `resident-elf-qemu: backend-reset-self-check counters=1 display=1 time=1 input=1 storage=1 result=ok`
- `resident-elf-qemu: store entries=31 bytes=`
- `resident-elf-qemu: store-media kind=memory bytes=`
- `resident-elf-qemu: unsupported storage_open=1 storage_read=1 storage_write=1 storage_close=1 afe_configure=1 afe_read=1 storage_count=1/1/1/1 afe_count=1/1`
- `resident-elf-qemu: app hello_app stage=exit code=ok exit=0`
- `resident-elf-qemu: capacity hello_app needed=270 free=65266 fits=1 region=65536 probe=ok`
- `resident-elf-qemu: caps hello_app console=78 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: received stage name=hello_app code=ok format=elf bytes=5132`
- `resident-elf-qemu: app received:hello_app stage=exit code=ok exit=0`
- `resident-elf-qemu: capacity received:hello_app needed=270 free=65266 fits=1 region=65536 probe=ok`
- `resident-elf-qemu: caps received:hello_app console=78 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: store stage name=hello_app code=ok format=elf size=5132`
- `resident-elf-qemu: app store:hello_app stage=exit code=ok exit=0`
- `resident-elf-qemu: capacity store:hello_app needed=270 free=65266 fits=1 region=65536 probe=ok`
- `resident-elf-qemu: caps store:hello_app console=78 time=0 describe=0 present=0 input=0 exit=0`
- `argv_app: argc=4 checksum=2052`
- `resident-elf-qemu: app argv_app stage=exit code=ok exit=0`
- `resident-elf-qemu: capacity argv_app needed=396 free=65140 fits=1 region=65536 probe=ok`
- `resident-elf-qemu: caps argv_app console=31 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: store stage name=argv_app code=ok format=elf size=5392`
- `resident-elf-qemu: app store:argv_app stage=exit code=ok exit=0`
- `resident-elf-qemu: capacity store:argv_app needed=396 free=65140 fits=1 region=65536 probe=ok`
- `resident-elf-qemu: caps store:argv_app console=31 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: prepare prepare:argv_app stage=start code=ok ready=1 argc=4`
- `resident-elf-qemu: capacity prepare:argv_app needed=396 free=65140 fits=1 region=65536 probe=ok`
- `resident-elf-qemu: caps prepare:argv_app console=0 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=0/0/0/0 storage_bytes=0`
- `resident-elf-qemu: app argv_overflow_app stage=argv code=argv_overflow exit=0`
- `resident-elf-qemu: capacity argv_overflow_app needed=396 free=65140 fits=1 region=65536 probe=ok`
- `resident-elf-qemu: caps argv_overflow_app console=0 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=0/0/0/0 storage_bytes=0`
- `resident-elf-qemu: app abi_mismatch_app stage=abi code=abi_mismatch exit=0`
- `resident-elf-qemu: capacity abi_mismatch_app needed=270 free=65266 fits=1 region=65536 probe=ok`
- `resident-elf-qemu: caps abi_mismatch_app console=0 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=0/0/0/0 storage_bytes=0`
- `resident-elf-qemu: store stage name=missing_app code=image_not_found expected=image_not_found image_size=0`
- `resident-elf-qemu: caps missing_app console=0 time=0 describe=0 present=0 input=0 exit=0`
- `afe_error_app: unsupported afe preserved buffer`
- `resident-elf-qemu: app afe_error_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps afe_error_app console=48 time=0 describe=0 present=0 input=0 exit=0`
- `storage=0/0/0/0 storage_bytes=0 afe=1/1`
- `resident-elf-qemu: store stage name=afe_error_app code=ok format=elf`
- `resident-elf-qemu: app store:afe_error_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps store:afe_error_app console=48 time=0 describe=0 present=0 input=0 exit=0`
- `bss_app: zero-fill ok`
- `resident-elf-qemu: app bss_app stage=exit code=ok exit=0`
- `resident-elf-qemu: capacity bss_app needed=513 free=65023 fits=1 region=65536 probe=ok`
- `resident-elf-qemu: caps bss_app console=22 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: store stage name=bss_app code=ok format=elf`
- `resident-elf-qemu: app store:bss_app stage=exit code=ok exit=0`
- `resident-elf-qemu: capacity store:bss_app needed=513 free=65023 fits=1 region=65536 probe=ok`
- `resident-elf-qemu: caps store:bss_app console=22 time=0 describe=0 present=0 input=0 exit=0`
- `console_error_app: null write rejected`
- `resident-elf-qemu: app console_error_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps console_error_app console=39 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: app store:console_error_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps store:console_error_app console=39 time=0 describe=0 present=0 input=0 exit=0`
- `data_app: data-init ok checksum=50`
- `resident-elf-qemu: app data_app stage=exit code=ok exit=0`
- `resident-elf-qemu: capacity data_app needed=529 free=65007 fits=1 region=65536 probe=ok`
- `resident-elf-qemu: caps data_app console=35 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: store stage name=data_app code=ok format=elf`
- `resident-elf-qemu: app store:data_app stage=exit code=ok exit=0`
- `resident-elf-qemu: capacity store:data_app needed=529 free=65007 fits=1 region=65536 probe=ok`
- `resident-elf-qemu: caps store:data_app console=35 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: app.exit code=7`
- `resident-elf-qemu: app exit_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps exit_app console=0 time=0 describe=0 present=0 input=0 exit=1`
- `resident-elf-qemu: app store:exit_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps store:exit_app console=0 time=0 describe=0 present=0 input=0 exit=1`
- `resident-elf-qemu: app exit_error_app stage=exit code=ok exit=42`
- `resident-elf-qemu: caps exit_error_app console=0 time=0 describe=0 present=0 input=0 exit=1`
- `resident-elf-qemu: app store:exit_error_app stage=exit code=ok exit=42`
- `resident-elf-qemu: caps store:exit_error_app console=0 time=0 describe=0 present=0 input=0 exit=1`
- `resident-elf-qemu: app.exit code=-3`
- `resident-elf-qemu: app exit_negative_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps exit_negative_app console=0 time=0 describe=0 present=0 input=0 exit=1`
- `resident-elf-qemu: app store:exit_negative_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps store:exit_negative_app console=0 time=0 describe=0 present=0 input=0 exit=1`
- `resident-elf-qemu: app return_negative_app stage=exit code=ok exit=-5`
- `resident-elf-qemu: caps return_negative_app console=0 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: app store:return_negative_app stage=exit code=ok exit=-5`
- `resident-elf-qemu: caps store:return_negative_app console=0 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: app unsupported_caps_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps unsupported_caps_app console=0 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: app store:unsupported_caps_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps store:unsupported_caps_app console=0 time=0 describe=0 present=0 input=0 exit=0`
- `storage_app: bytes=27 checksum=2441`
- `resident-elf-qemu: app storage_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps storage_app console=36 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=1/5/0/1 storage_bytes=27`
- `resident-elf-qemu: app store:storage_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps store:storage_app console=36 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=1/5/0/1 storage_bytes=27`
- `storage_catalog_app: files=2 bytes=31 checksum=2845`
- `resident-elf-qemu: storage open path=/virtual/alpha.txt code=ok fd=3 size=15`
- `resident-elf-qemu: storage read fd=4 code=ok requested=5 count=1 offset=15 remaining=0`
- `resident-elf-qemu: app storage_catalog_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps storage_catalog_app console=52 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=2/9/0/2 storage_bytes=31`
- `resident-elf-qemu: app store:storage_catalog_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps store:storage_catalog_app console=52 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=2/9/0/2 storage_bytes=31`
- `storage_close_error_app: close reuse ok`
- `resident-elf-qemu: storage close fd=3 code=unsupported`
- `resident-elf-qemu: app storage_close_error_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps storage_close_error_app console=40 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=2/1/0/3 storage_bytes=1`
- `resident-elf-qemu: app store:storage_close_error_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps store:storage_close_error_app console=40 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=2/1/0/3 storage_bytes=1`
- `storage_error_app: invalid read preserved cursor`
- `resident-elf-qemu: storage read fd=3 code=invalid_argument requested=4 count=0 offset=0 remaining=27`
- `resident-elf-qemu: app storage_error_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps storage_error_app console=49 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=1/2/0/1 storage_bytes=4`
- `resident-elf-qemu: app store:storage_error_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps store:storage_error_app console=49 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=1/2/0/1 storage_bytes=4`
- `storage_fd_exhaustion_app: fd slots exhausted and reused`
- `resident-elf-qemu: storage open path=/virtual/readme.txt code=io_error fd=-1`
- `resident-elf-qemu: app storage_fd_exhaustion_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps storage_fd_exhaustion_app console=57 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=6/1/0/5 storage_bytes=1`
- `resident-elf-qemu: app store:storage_fd_exhaustion_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps store:storage_fd_exhaustion_app console=57 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=6/1/0/5 storage_bytes=1`
- `storage_open_error_app: open errors preserve fd`
- `resident-elf-qemu: storage open path=<null> code=invalid_argument fd=-1`
- `resident-elf-qemu: storage open path=/virtual/readme.txt code=unsupported fd=-1`
- `resident-elf-qemu: app storage_open_error_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps storage_open_error_app console=48 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=4/1/0/1 storage_bytes=1`
- `resident-elf-qemu: app store:storage_open_error_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps store:storage_open_error_app console=48 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=4/1/0/1 storage_bytes=1`
- `storage_write_error_app: readonly write preserved cursor`
- `resident-elf-qemu: storage write fd=3 code=unsupported requested=1 count=0 offset=1 remaining=26`
- `resident-elf-qemu: app storage_write_error_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps storage_write_error_app console=57 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=1/2/1/1 storage_bytes=2`
- `resident-elf-qemu: app store:storage_write_error_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps store:storage_write_error_app console=57 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=1/2/1/1 storage_bytes=2`
- `storage_zero_io_app: zero io preserved cursor`
- `resident-elf-qemu: storage read fd=3 code=ok requested=0 count=0 offset=0 remaining=27`
- `resident-elf-qemu: storage write fd=3 code=unsupported requested=0 count=0 offset=1 remaining=26`
- `resident-elf-qemu: app storage_zero_io_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps storage_zero_io_app console=46 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=1/3/1/1 storage_bytes=2`
- `resident-elf-qemu: app store:storage_zero_io_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps store:storage_zero_io_app console=46 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=1/3/1/1 storage_bytes=2`
- `display_describe_error_app: null describe rejected`
- `resident-elf-qemu: app display_describe_error_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps display_describe_error_app console=51 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: app store:display_describe_error_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps store:display_describe_error_app console=51 time=0 describe=0 present=0 input=0 exit=0`
- `display_error_app: invalid present rejected`
- `resident-elf-qemu: display present bytes=1020 code=invalid_argument expected=1024`
- `resident-elf-qemu: app display_error_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps display_error_app console=44 time=0 describe=1 present=1 input=0 exit=0 display_checksum=0 display_checksum_total=0`
- `display_hash=0x00000000 display_hash_total=0x00000000 display_frame=0`
- `resident-elf-qemu: app store:display_error_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps store:display_error_app console=44 time=0 describe=1 present=1 input=0 exit=0 display_checksum=0 display_checksum_total=0`
- `display_null_present_app: null present rejected`
- `resident-elf-qemu: display present bytes=1024 code=invalid_argument expected=1024`
- `resident-elf-qemu: app display_null_present_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps display_null_present_app console=48 time=0 describe=1 present=1 input=0 exit=0 display_checksum=0 display_checksum_total=0`
- `resident-elf-qemu: app store:display_null_present_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps store:display_null_present_app console=48 time=0 describe=1 present=1 input=0 exit=0 display_checksum=0 display_checksum_total=0`
- `display_sequence_app: frames=2 checksum=3072`
- `resident-elf-qemu: display present bytes=1024 checksum=1024`
- `resident-elf-qemu: display present bytes=1024 checksum=1024 hash=0x373fb1c5 frame=1`
- `resident-elf-qemu: display present bytes=1024 checksum=2048`
- `resident-elf-qemu: display present bytes=1024 checksum=2048 hash=0xa9b09dc5 frame=2`
- `resident-elf-qemu: app display_sequence_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps display_sequence_app console=45 time=0 describe=1 present=2 input=0 exit=0 display_checksum=2048 display_checksum_total=3072`
- `display_hash=0xa9b09dc5 display_hash_total=0x9e8f2c00 display_frame=2`
- `resident-elf-qemu: app store:display_sequence_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps store:display_sequence_app console=45 time=0 describe=1 present=2 input=0 exit=0 display_checksum=2048 display_checksum_total=3072`
- `input_error_app: null poll rejected`
- `resident-elf-qemu: input poll code=invalid_argument`
- `resident-elf-qemu: app input_error_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps input_error_app console=36 time=0 describe=0 present=0 input=1 exit=0`
- `input_checksum=0 input_last=0,0,0`
- `resident-elf-qemu: app store:input_error_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps store:input_error_app console=36 time=0 describe=0 present=0 input=1 exit=0`
- `input_sequence_app: polls=4 checksum=114`
- `resident-elf-qemu: input poll encoder1=0 pointer=6,8 max=15,15 detected=1 down=0`
- `resident-elf-qemu: app input_sequence_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps input_sequence_app console=41 time=0 describe=0 present=0 input=4 exit=0`
- `input_checksum=114 input_last=6,8,0`
- `resident-elf-qemu: app store:input_sequence_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps store:input_sequence_app console=41 time=0 describe=0 present=0 input=4 exit=0`
- `input_wrap_app: polls=6 checksum=169`
- `resident-elf-qemu: input poll encoder1=0 pointer=4,6 max=15,15 detected=1 down=1`
- `resident-elf-qemu: app input_wrap_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps input_wrap_app console=37 time=0 describe=0 present=0 input=6 exit=0`
- `input_checksum=169 input_last=4,6,1`
- `resident-elf-qemu: app store:input_wrap_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps store:input_wrap_app console=37 time=0 describe=0 present=0 input=6 exit=0`
- `resident-elf-qemu: app time_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps time_app console=0 time=2 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: app store:time_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps store:time_app console=0 time=2 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: app time_sequence_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps time_sequence_app console=0 time=4 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: app store:time_sequence_app stage=exit code=ok exit=0`
- `resident-elf-qemu: caps store:time_sequence_app console=0 time=4 describe=0 present=0 input=0 exit=0`
- `large_fit_app: near-limit ok`
- `resident-elf-qemu: app large_fit_app stage=exit code=ok exit=0`
- `resident-elf-qemu: capacity large_fit_app needed=61696 free=3840 fits=1 region=65536 probe=ok`
- `resident-elf-qemu: caps large_fit_app console=29 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: received stage name=large_fit_app code=ok format=elf`
- `resident-elf-qemu: app received:large_fit_app stage=exit code=ok exit=0`
- `resident-elf-qemu: capacity received:large_fit_app needed=61696 free=3840 fits=1 region=65536 probe=ok`
- `resident-elf-qemu: caps received:large_fit_app console=29 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: packetstream stage name=large_fit_app transport=ok packet=ok stage=launch_ready code=ok payload=5168`
- `resident-elf-qemu: packetstream read name=large_fit_app code=ok bytes=5168`
- `resident-elf-qemu: packetstream app-stage name=large_fit_app code=ok format=elf bytes=5168`
- `resident-elf-qemu: app packetstream:large_fit_app stage=exit code=ok exit=0`
- `resident-elf-qemu: capacity packetstream:large_fit_app needed=61696 free=3840 fits=1 region=65536 probe=ok`
- `resident-elf-qemu: caps packetstream:large_fit_app console=29 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: store stage name=large_fit_app code=ok format=elf`
- `resident-elf-qemu: app store:large_fit_app stage=exit code=ok exit=0`
- `resident-elf-qemu: capacity store:large_fit_app needed=61696 free=3840 fits=1 region=65536 probe=ok`
- `resident-elf-qemu: caps store:large_fit_app console=29 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: packetstream stage name=packetstream_crc_mismatch transport=packet_failed packet=receive_failed stage=failed code=crc_mismatch payload=5132`
- `resident-elf-qemu: caps packetstream_crc_mismatch console=0 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: received stage name=received_too_large_app code=buffer_too_small expected=buffer_too_small bytes=0 image_size=16385`
- `resident-elf-qemu: caps received_too_large_app console=0 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: store stage name=too_large_store_app code=image_too_large expected=image_too_large image_size=0`
- `resident-elf-qemu: caps too_large_store_app console=0 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: load bad_elf_magic_app format=elf probe=bad_magic`
- `resident-elf-qemu: capacity bad_elf_magic_app needed=0 free=65536 fits=1 region=65536 probe=bad_magic`
- `resident-elf-qemu: app bad_elf_magic_app stage=load code=load_failed exit=0`
- `resident-elf-qemu: caps bad_elf_magic_app console=0 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: packetstream stage name=packetstream_bad_elf_magic_app transport=ok packet=ok stage=launch_ready code=ok payload=64`
- `resident-elf-qemu: packetstream read name=packetstream_bad_elf_magic_app code=ok bytes=64`
- `resident-elf-qemu: packetstream app-stage name=packetstream_bad_elf_magic_app code=ok format=elf bytes=64`
- `resident-elf-qemu: load packetstream:packetstream_bad_elf_magic_app format=elf probe=bad_magic`
- `resident-elf-qemu: capacity packetstream:packetstream_bad_elf_magic_app needed=0 free=65536 fits=1 region=65536 probe=bad_magic`
- `resident-elf-qemu: app packetstream:packetstream_bad_elf_magic_app stage=load code=load_failed exit=0`
- `resident-elf-qemu: caps packetstream:packetstream_bad_elf_magic_app console=0 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: load bad_header_app format=elf probe=bad_header`
- `resident-elf-qemu: capacity bad_header_app needed=0 free=65536 fits=1 region=65536 probe=bad_header`
- `resident-elf-qemu: app bad_header_app stage=load code=load_failed exit=0`
- `resident-elf-qemu: caps bad_header_app console=0 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: load bad_class_app format=elf probe=bad_class`
- `resident-elf-qemu: capacity bad_class_app needed=0 free=65536 fits=1 region=65536 probe=bad_class`
- `resident-elf-qemu: app bad_class_app stage=load code=load_failed exit=0`
- `resident-elf-qemu: caps bad_class_app console=0 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: load bad_endian_app format=elf probe=bad_endian`
- `resident-elf-qemu: capacity bad_endian_app needed=0 free=65536 fits=1 region=65536 probe=bad_endian`
- `resident-elf-qemu: app bad_endian_app stage=load code=load_failed exit=0`
- `resident-elf-qemu: caps bad_endian_app console=0 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: load bad_ident_version_app format=elf probe=bad_header`
- `resident-elf-qemu: capacity bad_ident_version_app needed=0 free=65536 fits=1 region=65536 probe=bad_header`
- `resident-elf-qemu: app bad_ident_version_app stage=load code=load_failed exit=0`
- `resident-elf-qemu: caps bad_ident_version_app console=0 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: load bad_type_app format=elf probe=bad_header`
- `resident-elf-qemu: capacity bad_type_app needed=0 free=65536 fits=1 region=65536 probe=bad_header`
- `resident-elf-qemu: app bad_type_app stage=load code=load_failed exit=0`
- `resident-elf-qemu: caps bad_type_app console=0 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: load bad_machine_app format=elf probe=bad_header`
- `resident-elf-qemu: capacity bad_machine_app needed=0 free=65536 fits=1 region=65536 probe=bad_header`
- `resident-elf-qemu: app bad_machine_app stage=load code=load_failed exit=0`
- `resident-elf-qemu: caps bad_machine_app console=0 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: load bad_version_app format=elf probe=bad_header`
- `resident-elf-qemu: capacity bad_version_app needed=0 free=65536 fits=1 region=65536 probe=bad_header`
- `resident-elf-qemu: app bad_version_app stage=load code=load_failed exit=0`
- `resident-elf-qemu: caps bad_version_app console=0 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: load bad_ehsize_app format=elf probe=bad_header`
- `resident-elf-qemu: capacity bad_ehsize_app needed=0 free=65536 fits=1 region=65536 probe=bad_header`
- `resident-elf-qemu: app bad_ehsize_app stage=load code=load_failed exit=0`
- `resident-elf-qemu: caps bad_ehsize_app console=0 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: load bad_phentsize_app format=elf probe=bad_program_header`
- `resident-elf-qemu: capacity bad_phentsize_app needed=0 free=65536 fits=1 region=65536 probe=bad_program_header`
- `resident-elf-qemu: app bad_phentsize_app stage=load code=load_failed exit=0`
- `resident-elf-qemu: caps bad_phentsize_app console=0 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: load bad_program_header_app format=elf probe=bad_program_header`
- `resident-elf-qemu: capacity bad_program_header_app needed=0 free=65536 fits=1 region=65536 probe=bad_program_header`
- `resident-elf-qemu: app bad_program_header_app stage=load code=load_failed exit=0`
- `resident-elf-qemu: caps bad_program_header_app console=0 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: load truncated_payload_app format=elf probe=truncated_payload`
- `resident-elf-qemu: capacity truncated_payload_app needed=0 free=65536 fits=1 region=65536 probe=truncated_payload`
- `resident-elf-qemu: app truncated_payload_app stage=load code=load_failed exit=0`
- `resident-elf-qemu: caps truncated_payload_app console=0 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: load no_load_segment_app format=elf probe=no_load_segment`
- `resident-elf-qemu: capacity no_load_segment_app needed=0 free=65536 fits=1 region=65536 probe=no_load_segment`
- `resident-elf-qemu: app no_load_segment_app stage=load code=load_failed exit=0`
- `resident-elf-qemu: caps no_load_segment_app console=0 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: load entry_outside_segment_app format=elf probe=entry_outside_segment`
- `resident-elf-qemu: capacity entry_outside_segment_app needed=0 free=65536 fits=1 region=65536 probe=entry_outside_segment`
- `resident-elf-qemu: app entry_outside_segment_app stage=load code=load_failed exit=0`
- `resident-elf-qemu: caps entry_outside_segment_app console=0 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: load overlapping_segments_app format=elf probe=overlapping_segments`
- `resident-elf-qemu: capacity overlapping_segments_app needed=0 free=65536 fits=1 region=65536 probe=overlapping_segments`
- `resident-elf-qemu: app overlapping_segments_app stage=load code=load_failed exit=0`
- `resident-elf-qemu: caps overlapping_segments_app console=0 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: load rwx_segment_app format=elf probe=rwx_segment`
- `resident-elf-qemu: capacity rwx_segment_app needed=0 free=65536 fits=1 region=65536 probe=rwx_segment`
- `resident-elf-qemu: app rwx_segment_app stage=load code=load_failed exit=0`
- `resident-elf-qemu: caps rwx_segment_app console=0 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: load wrong_link_base_app format=elf probe=ok link_base=0x20081000 expected_base=0x20080000`
- `base_match=0`
- `resident-elf-qemu: capacity wrong_link_base_app needed=270 free=65266 fits=1 region=65536 probe=ok`
- `resident-elf-qemu: app wrong_link_base_app stage=load code=load_failed exit=0`
- `resident-elf-qemu: caps wrong_link_base_app console=0 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: load too_large_app format=elf probe=load_buffer_too_small`
- `resident-elf-qemu: capacity too_large_app needed=82176 free=0 fits=0 region=65536 probe=load_buffer_too_small`
- `resident-elf-qemu: app too_large_app stage=load code=load_failed exit=0`
- `resident-elf-qemu: caps too_large_app console=0 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: load unaligned_load_buffer_app format=elf probe=load_buffer_unaligned`
- `resident-elf-qemu: capacity unaligned_load_buffer_app needed=0 free=65536 fits=1 region=65536 probe=load_buffer_unaligned`
- `resident-elf-qemu: app unaligned_load_buffer_app stage=load code=load_failed exit=0`
- `resident-elf-qemu: caps unaligned_load_buffer_app console=0 time=0 describe=0 present=0 input=0 exit=0`
- `resident-elf-qemu: input poll encoder1=1 pointer=3,5 max=15,15 detected=1 down=0`
- `resident-elf-qemu: display present bytes=1024`
- `resident-elf-qemu: display present bytes=1024 checksum=174720`
- `resident-elf-qemu: display present bytes=1024 checksum=174720 hash=0xfac53a05 frame=1`
- `resident-elf-qemu: app player_min stage=exit code=ok exit=0`
- `resident-elf-qemu: capacity player_min needed=1280 free=64256 fits=1 region=65536 probe=ok`
- `resident-elf-qemu: caps player_min console=32 time=1 describe=1 present=1 input=1 exit=0 display_checksum=174720`
- `display_hash=0xfac53a05 display_hash_total=0xfac53a05 display_frame=1`
- `resident-elf-qemu: received stage name=player_min code=ok format=elf bytes=5168`
- `resident-elf-qemu: app received:player_min stage=exit code=ok exit=0`
- `resident-elf-qemu: capacity received:player_min needed=1280 free=64256 fits=1 region=65536 probe=ok`
- `resident-elf-qemu: caps received:player_min console=32 time=1 describe=1 present=1 input=1 exit=0 display_checksum=174720`
- `resident-elf-qemu: packetstream stage name=player_min transport=ok packet=ok stage=launch_ready code=ok payload=5168`
- `resident-elf-qemu: packetstream read name=player_min code=ok bytes=5168`
- `resident-elf-qemu: packetstream app-stage name=player_min code=ok format=elf bytes=5168`
- `resident-elf-qemu: app packetstream:player_min stage=exit code=ok exit=0`
- `resident-elf-qemu: capacity packetstream:player_min needed=1280 free=64256 fits=1 region=65536 probe=ok`
- `resident-elf-qemu: caps packetstream:player_min console=32 time=1 describe=1 present=1 input=1 exit=0 display_checksum=174720`
- `resident-elf-qemu: store stage name=player_min code=ok format=elf`
- `resident-elf-qemu: display present bytes=1024 checksum=174720`
- `resident-elf-qemu: app store:player_min stage=exit code=ok exit=0`
- `resident-elf-qemu: capacity store:player_min needed=1280 free=64256 fits=1 region=65536 probe=ok`
- `resident-elf-qemu: caps store:player_min console=32 time=1 describe=1 present=1 input=1 exit=0 display_checksum=174720`
- `resident-elf-qemu: ok`

On success, the script also writes `frame-signatures.json` next to the QEMU log.
That JSON captures every `display.present` signature as `bytes`, `checksum`,
`hash`, and `frame`, and groups pending frames by the App run that produced
them, such as `display_sequence_app`, `store:display_sequence_app`,
`player_min`, `received:player_min`, `packetstream:player_min`, and
`store:player_min`. `frame-signatures.golden.json` is the checked-in expected
signature set for this smoke. It is a host-side GUI regression gate, not a
product manifest or framebuffer file format. The default QEMU run validates and
golden-compares this JSON after writing it.

The script also writes `input-trace.json`. That JSON groups virtual input events
by App run: `input_sequence_app`, `store:input_sequence_app`,
`input_wrap_app`, `store:input_wrap_app`, `player_min`,
`received:player_min`, `packetstream:player_min`, and `store:player_min`. It is
deterministic capability evidence for GUI/input regression triage, not an input
backend API.

The script also writes `storage-trace.json`. That JSON groups virtual storage
events by App run: `storage_app`, `store:storage_app`, `storage_catalog_app`,
`store:storage_catalog_app`, `storage_close_error_app`,
`store:storage_close_error_app`, `storage_error_app`,
`store:storage_error_app`, `storage_fd_exhaustion_app`,
`store:storage_fd_exhaustion_app`, `storage_open_error_app`,
`store:storage_open_error_app`, `storage_write_error_app`,
`store:storage_write_error_app`, `storage_zero_io_app`,
`store:storage_zero_io_app`, `unsupported_caps_app`, and
`store:unsupported_caps_app`. It records `open`,
`read`, `write`, and `close` events with path, fd, status, requested byte count, returned
byte count, offset, and remaining bytes. The `storage_error_app` trace keeps the
invalid read at offset `0`, then proves the next valid read also starts at
offset `0`. The `storage_close_error_app` trace proves that a duplicate close is
reported as `unsupported`, then reopening the same file reuses fd `3` and reads
from offset `0`. The `storage_open_error_app` trace proves that failed open calls
do not consume fd slots before the next valid open returns fd `3`.
The `storage_fd_exhaustion_app` trace proves the fixed four-slot fd table reports
`io_error` when full, then reuses a closed fd slot and restarts the reopened file
at offset `0`.
The `storage_write_error_app` trace proves that a rejected read-only write is
reported at the current offset and does not advance the cursor before the next
read.
The `storage_zero_io_app` trace proves that zero-length reads return `0` and do
not advance the cursor, while zero-length writes remain rejected by the read-only
backend and also preserve the cursor.
`storage-trace.golden.json` is the checked-in expected storage
sequence. It is deterministic capability evidence for storage regression triage,
not a filesystem format, block-device trace, or App Store metadata.

The script also writes `frame-dumps.json`, which contains the same run grouping
plus the full 16x16 ARGB8888 frame bytes encoded as hex. This remains a
smoke-level capture format for deterministic regression comparison, not a public
GUI backend API. `frame-dumps.golden.json` is the checked-in expected pixel dump
set. Frame dump comparison ignores absolute log paths and compares the run/frame
structure plus frame bytes.

The script also materializes those frame dumps into `frame-ppm/`. Each generated
PPM is a tiny 16x16 RGB888 image derived from the QEMU ARGB8888 payload, and
`frame-ppm/manifest.json` records the run name, hash, source frame index, and
file name. This is deliberately a host evidence artifact for quick visual
inspection of the virtual display output; it is not a GUI backend API and is not
embedded in any App Store payload.

The script also writes `domain-summary.json` with schema
`charm.resident_elf_qemu.domain_summary.v1`. This is the compact machine-readable
proof that the QEMU run is a `virtual_m7` runtime domain with run region
`0x20080000..0x20090000`, 16 KiB stage cache, fixed packetstream
storage/transport/stream/received buffers, Store v1 staging, direct/received/
packetstream/Store ELF runs, prepare-only coverage, capability coverage, and the
expected negative cases. Its `run_budget` section records the effective QEMU
timeout and failure-tail settings for auditability; golden comparison
canonicalizes those local runner values so temporary timeout tuning does not
look like a backend semantic change. Its `backend_scope` section records the
exact virtual-board scope boundary: it proves ELF loader, AppRuntime,
`CharmAppApi`, capability backend, received-image, packetstream, and Store v1
semantics, but it does not prove H747 USB CDC, QSPI, eMMC, FMC SDRAM, HAL init,
MPU/cache, or pinmux behavior. Its `backend_contract` section records the smoke-local
virtual capabilities, unsupported AFE boundary, deterministic time tick,
framebuffer display evidence, deterministic input sequence, read-only virtual
storage media, and `app.exit` return-value semantics. Its `backend_readiness`
section is the derived readiness summary that confirms the same run proved the
ELF loader, AppRuntime, received/packetstream/Store ingress, source equivalence,
GUI, storage, input, unsupported-boundary, and runtime-reset gates. Its
`backend_capability_matrix` section maps each smoke-local capability to its
virtual provider, policy, evidence source, and representative run set. Its
`runtime_domain_profile` section names the smoke as a `virtual_m7` virtual-board
domain and mirrors the same `backend_scope`; validation rejects profile/scope
mismatches so archived evidence cannot silently widen QEMU's claimed boundary. Its
`elf_run_evidence_matrix` section indexes every ELF load by run name and binds
source (`direct`, `received`, `packetstream`, `store`, or `prepare`), source
stage/code, ELF load probe/capacity, AppRuntime stage/code/exit, and readiness
into one record. This is the QEMU-local evidence index for the resident ELF
main chain; it is not a public backend/profile schema. Its `failure_taxonomy`
section derives a compact negative-path index from `coverage.negative_cases`:
transport (`packetstream_verify`), stage (`received_stage`/`store_stage`), load,
and runtime (`argv`/`abi`) failures must keep their expected counts and cases.
Its `runtime_reset_determinism` section records the QEMU-local reset evidence
that each App run starts from a cleared run region, reset capability counters,
deterministic time/input/display/storage state, zero-filled BSS,
reinitialized data, and equivalent direct/received/packetstream/Store source
runs. This proves the virtual runtime domain reset contract, not H747 peripheral
reset or board power sequencing.
Its `display` section
records the fixed virtual display mode so GUI evidence drift is caught before
frame hashes are interpreted. Its evidence section records frame signature, full
dump, visual PPM, input trace, and storage trace counts. Its `coverage.loads` section records the ELF loader/probe/capacity
facts for each loaded image: format, probe code, ELF link base, expected domain
base, materialized entry, original entry vaddr, span, segment count,
needed/free bytes, fit result, base match, and run region. It is an off-board evidence
artifact, not a product manifest and not Store metadata. Its
`store.media` section records the QEMU-local virtual Store media kind, byte
size, read calls, read bytes, and read failures. Its
`coverage.packetstreams` section records the packetstream payload size, stream
size, packet/dispatch count, launch-ready state, readback bytes, app-stage bytes,
and CRC agreement for the QEMU-local packetstream receive path. It records both
transport-level failures, such as CRC mismatch with empty read/stage fields, and
image-level failures, such as a malformed ELF payload that reaches `launch_ready`
but fails later in the AppRuntime load stage.

Its `artifacts` section records the QEMU-local generated ELF, Store, and
generated include artifacts used for the run. Artifact paths are canonicalized
for golden comparison, while sizes and CRCs remain exact gates so stale
`out-qemu` inputs are caught before QEMU evidence is archived. The same section
also records `store_manifest`: a parsed Store v1 header and entry list with
entry name, offset, size, flags, format bits, and payload CRC. Domain-summary
validation cross-checks every Store entry against the generated ELF artifacts so
wrong Store flags, stale payload bytes, or size drift fail before QEMU evidence
is treated as valid.

Its `coverage.source_matrix` section summarizes the resident-platform entry
coverage per App. For example, `hello_app`, `large_fit_app`, and `player_min`
must all prove equivalent direct, received, packetstream, and Store-backed ELF
runs. Validation now also compares those representative Apps' ELF load metadata
across the four sources: format, probe result, link base, expected base, entry,
entry vaddr, load span, segment count, capacity needed/free, fit result, run
region, and capacity probe must match the direct run. `argv_app` also proves the
prepare-only path; negative entries retain the source where the stable failure
stage was observed.
Its `coverage.gui_timeline` section merges display frames and input polls by App
run name. `player_min`, `received:player_min`, `packetstream:player_min`, and
`store:player_min` must each prove `frames=1 inputs=1` with the same frame hash
and last input event. `display_sequence_app` and `input_sequence_app` remain
single-capability controls for display-only and input-only behavior, while
`input_wrap_app` gates deterministic input sequence wraparound. This makes
GUI evidence a per-run timeline check rather than two unrelated trace files.
Validation treats this summary as a stable QEMU backend contract: expected
backend contract, coverage counts for runs, stages, loads, packetstreams,
source-matrix entries, GUI timeline entries, negative cases, and evidence traces
are exact gates. If a future change adds or removes coverage, update the smoke
and this evidence contract intentionally. `domain-summary.golden.json` is the
checked-in canonical contract snapshot; its environment-specific evidence paths
are reduced to file names, and the generated App artifact directory is reduced
to a stable token before comparison.

## Boundary

This smoke is a Charm virtual backend, not an H747 simulator. USB CDC, QSPI,
eMMC, FMC SDRAM, HAL initialization, and board pin/resource conflicts remain
real-board concerns. QEMU should first cover loader/runtime/capability semantics
and later grow virtual display, input, and block media backends behind the same
`CharmAppApi` and `AppImage` contracts.

The H747 resident evidence bundle can include this virtual-board proof with the
explicit `-QemuElf` switch:

```powershell
Examples/project/h747-lab/tools/capture-resident-platform-evidence-bundle.ps1 -QemuElf
```

The bundle forwards the same QEMU run budget as the standalone wrapper. Use
`-QemuElfTimeoutSec <seconds>` and `-QemuElfTailLines <lines>` to tune the QEMU
wait timeout and failure log tail without bypassing the supported wrapper
surface. Use `-QemuElfEvidenceDir <dir>` when the bundle should write or
validate QEMU evidence outside the canonical `resident_elf_qemu_smoke`
directory; the summary records that absolute path as `qemu_elf_evidence_dir`.
Use `-QemuElfBuildDir <dir>` and `-QemuElfAppOutDir <dir>` when the bundle
should also isolate the generated QEMU firmware build tree and generated App
ELFs. The bundle passes absolute paths to the QEMU wrapper and records them as
`qemu_elf_build_dir` and `qemu_elf_app_out_dir`.
Add `-QemuElfDoctor` when the bundle should run the QEMU host-environment
doctor before artifact generation, host smokes, a full QEMU run, or
existing-evidence validation:

```powershell
Examples/project/h747-lab/tools/capture-resident-platform-evidence-bundle.ps1 -QemuElf -QemuElfDoctor
```

This is explicit rather than default so `-QemuElfValidateOnly` can still be used
on hosts that only need to validate an already captured evidence set.

When BSP/CubeMX source-list migration is blocking the H747 build-only step, the
same bundle can be used as an off-board ELF/QEMU gate with:

```powershell
Examples/project/h747-lab/tools/capture-resident-platform-evidence-bundle.ps1 -QemuElf -SkipH747Build
```

To reuse an existing QEMU capture in the same bundle without rebuilding or
launching QEMU, add `-QemuElfValidateOnly`:

```powershell
Examples/project/h747-lab/tools/capture-resident-platform-evidence-bundle.ps1 -QemuElf -QemuElfValidateOnly -SkipH747Build
```

That mode still validates `qemu-ci.log`, traces, PPM frames,
`domain-summary.json`, the backend contract, and golden comparisons before
writing the `qemu_elf_*` summary tokens with
`qemu_elf_mode=validate_existing_evidence`. If the selected evidence directory
does not contain the derived `backend-contract.json`, validation derives a
temporary contract from `domain-summary.json` and removes it after validation.
The bundle summary then records
`qemu_elf_backend_contract_file=derived_from_domain_summary:<domain-summary.json>`.

The switch is opt-in so the default H747 evidence bundle keeps its existing
meaning. QEMU evidence is off-board semantic evidence for the ELF/AppRuntime
chain; it does not replace real-board USB, Store, SDRAM, eMMC, or HAL evidence.
When `-QemuElf` is enabled, the bundle summary expands `domain-summary.json`
into `qemu_elf_mode`, `qemu_elf_doctor`, `qemu_elf_doctor_versions`,
`qemu_elf_doctor_layout`, `qemu_elf_doctor_scope`, `qemu_elf_scope_match`,
`qemu_elf_domain`, `qemu_elf_app_model`, `qemu_elf_backend`,
`qemu_elf_backend_self_check`, `qemu_elf_backend_reset_self_check`,
`qemu_elf_backend_readiness`,
`qemu_elf_runtime_reset`,
`qemu_elf_capability_matrix`,
`qemu_elf_backend_scope`,
`qemu_elf_runtime_domain_profile`,
`qemu_elf_run_evidence_matrix`,
`qemu_elf_backend_contract`, `qemu_elf_run_budget`,
`qemu_elf_run_budget_match`, `qemu_elf_memory`, `qemu_elf_store`,
`qemu_elf_store_manifest`, `qemu_elf_display`,
`qemu_elf_domain_summary_golden`, `qemu_elf_frame_signatures`,
`qemu_elf_backend_contract_file`,
`qemu_elf_frame_signatures_golden`, `qemu_elf_frame_signature_gate`,
`qemu_elf_frame_dumps`, `qemu_elf_frame_dumps_golden`,
`qemu_elf_frame_dump_gate`, `qemu_elf_input_trace`,
`qemu_elf_input_trace_golden`, `qemu_elf_input_trace_gate`,
`qemu_elf_storage_trace`, `qemu_elf_storage_trace_golden`,
`qemu_elf_storage_trace_gate`, `qemu_elf_frame_ppm`,
`qemu_elf_frame_ppm_gate`, `qemu_elf_evidence`,
`qemu_elf_gui_contract`, `qemu_elf_storage_contract`, `qemu_elf_capacity`,
`qemu_elf_artifacts`, `qemu_elf_loads`,
`qemu_elf_equivalent_sources`,
`qemu_elf_packetstreams`, `qemu_elf_failure_taxonomy`,
`qemu_elf_failure_transport`, `qemu_elf_failure_stage`,
`qemu_elf_failure_load`, `qemu_elf_failure_runtime`,
`qemu_elf_domain_golden`, `qemu_elf_sources`, `qemu_elf_coverage`, and
`qemu_elf_player_min_gui` tokens so archived logs expose whether QEMU was
rebuilt and launched (`build_and_run`) or reused from an existing capture
(`validate_existing_evidence`), plus the fixed App model
(`format=elf:model=CharmAppApi`), virtual backend identity, detailed backend
contract, backend self-check, backend reset self-check, doctor/runtime scope matching (`qemu_elf_scope_match=1` when checked), whether the contract came from a persistent
`backend-contract.json` or from `domain-summary.json`, backend readiness
status/gates, capability matrix, recorded QEMU run budget, whether the requested bundle budget matches
the captured `domain-summary.json` budget, runtime-reset determinism, runtime-domain scope boundary, ELF run evidence matrix, ELF load memory boundary,
packetstream buffer boundary (`storage/transport/stream/received`), Store media,
QEMU trace capture/golden paths and gate status for frame signatures, full
frame dumps, input trace, and storage trace, frame PPM visual artifact
validation status, frame/input/storage evidence counts, GUI display/input
contract summary, virtual read-only storage contract summary,
parsed Store v1 entry flags/size/CRC self-consistency, near-limit/over-limit
ELF capacity, QEMU-local artifact counts and representative ELF/Store CRCs,
representative ELF loader
entry/span/segment/fits facts, direct/received/packetstream/Store equivalent
ELF load facts for `hello_app`, `large_fit_app`, and `player_min`,
packetstream payload/CRC and failure-boundary facts, domain-summary golden
comparison, transport/stage/load/runtime failure taxonomy plus per-category
failure summaries, and the
direct/received/packetstream/store source matrix plus full coverage counts
(`runs/stages/loads/packetstreams/source_matrix/gui_timeline/prepare/
capabilities/negative_cases`) without opening the JSON file.
