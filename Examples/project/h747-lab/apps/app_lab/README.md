# H747 App Lab

`app_lab` is the first resident monitor for the dynamic Charm App ABI on H747.
It is separate from `posix_lab`: POSIX remains the compatibility path for
C/POSIX programs, while `app_lab` validates the primary Player/Scope-style ABI:

```c
int charm_app_main(const CharmAppApi* api, int argc, char** argv);
```

## Monitor Commands

- `app list`: list embedded App ELF images.
- `app store install`: install the embedded `appstore.bin` into QSPI offset `0`.
- `app run <name> [args...]`: load an embedded App ELF and enter
  `charm_app_main`.
- `app run-path <path> [args...]`: reserved file-backed path. It currently
  supports `qspi:<name>` / `qspi:@<offset>:<size>` for the first QSPI
  Nor App image store and otherwise reports stable `not_supported` until a full
  storage-backed executable loader is connected.
- `app status`: print the staged monitor diagnostics report in this fixed order:
  monitor -> source/store -> install -> last result -> elf_load -> display/input.
- `app smoke`: run the first embedded smoke set.

## First Embedded Apps

- `hello_app`: proves C ABI entry, console capability, `argc/argv`, and return
  code recovery.
- `player_min`: proves display mode query, bounded raster present, time query,
  input poll, console logging, and return code recovery.

## Boundaries

- Bootloader still loads firmware/runtime, not App ELF directly.
- The resident runtime owns image source, ELF load buffer, cache maintenance,
  capability table, app start, and app exit reporting.
- The monitor consumes the reusable prototype runtime in `Examples/app_abi`;
  H747-specific code only binds embedded ELF images, load memory/cache handling,
  and board capabilities.
- The ABI is C-compatible. It must not expose C++ classes, vtables, exceptions,
  RTTI, or source-level concepts across the dynamic image boundary.
- ModuleX must later reuse the same `ProgramImage + CharmAppApi +
  charm_app_main` model rather than creating a second app model.
- `app_lab` is the current H747 dynamic-boundary mainline. `posix_lab` remains
  the POSIX compatibility line and does not define this ABI.

## Current Limits

- Embedded/fixed-memory App ELF is the primary and official path.
- QSPI Nor is the first installable program-image source. `app_lab` can install
  the embedded `appstore.bin` into QSPI, then stage App ELF bytes into a
  runtime-owned RAM cache and enter the same ELF loader and App ABI runtime
  path as embedded images.
- QSPI App store parsing uses the reusable `Examples/app_abi` store reader
  and staging prototype. H747 only provides the QSPI `read(offset, bytes)`
  backend and a RAM cache; lookup/read/cache-to-`AppImage` behavior stays in
  the shared App ABI helper.
- `app status` is the official diagnostics-first report for this target.
  Its stable fields are:
  `last_request`, `last_app`, `last_source`, `last_stage`, `last_code`,
  `last_exited`, `last_exit`, `backend`, `elf_load`, and the QSPI store/install
  facts.
- QSPI install uses the shared `Examples/app_abi` install semantics rather than
  monitor-local write rules. The current first milestone fixes the target store
  at QSPI offset `0`.
- File-backed filesystem App ELF is intentionally still a stable stub.
- Generic file-backed `app run-path <path>` is intentionally frozen at stable
  `not_supported` until a real storage-backed executable loader exists.
- `player_min` submits a small bounded raster payload. Full-frame rendering
  should come from a runtime/backend-owned buffer strategy, not a large dynamic
  App `.bss`.
- This target initializes display/input services, so its RAM_D1 footprint tracks
  display/runtime experiments rather than the minimal monitor targets.

## Build And Board Bring-Up

```powershell
cmake --preset h747-lab-debug -DH747_LAB_ARM_GNU_TOOLCHAIN_ROOT="D:/Toolchains/Arm GNU Toolchain arm-none-eabi/latest"
cmake --build --preset build-h747-lab-app-lab-debug
```

The expected firmware artifacts are:

- `cmake-build-h747-lab-debug/h747_lab_app_lab.elf`
- `cmake-build-h747-lab-debug/h747_lab_app_lab.bin`

The short-term board flashing path uses the binary artifact at internal Flash
base `0x08000000`:

```powershell
powershell -ExecutionPolicy Bypass -File tools/flash-app-lab-pyocd.ps1
```

The wrapper defaults to `h747_lab_app_lab.bin` with `pyocd load --format bin -a
0x08000000`. ELF flashing is kept as an explicit debug fallback via `-Elf`, not
as the default board smoke path.

The board-side smoke entry is `app smoke` at the `app-lab>` prompt over
`COM16 / 115200 8N1`.

QSPI App store paths are:

- `app store install`
- `app run-path qspi:<name> [args...]`
- `app run-path qspi:@<hex_offset>:<hex_size> [args...]`

The named store starts at QSPI offset `0` with this little-endian prototype
layout:

```c
struct QspiAppStoreHeader {
    uint32_t magic;       // 0x50415043, "CPAP"
    uint16_t version;     // 1
    uint16_t header_size; // sizeof(QspiAppStoreHeader)
    uint32_t entry_count;
    uint32_t entry_size;  // sizeof(QspiAppStoreEntry)
};

struct QspiAppStoreEntry {
    char name[32];
    uint32_t offset;
    uint32_t size;
    uint32_t flags;
};
```

This is not a filesystem or final manifest. It is the smallest durable program
store shape needed to validate Nor Flash as an App image source.

Board smoke protocol and reusable scripts are documented in
`docs/h747_lab_app_lab_smoke.md`.

Current board-free hardening note: this target is build-verified only in the
current round. The latest board-free pass focuses on App Store pack/staging and
Dev Loader receive semantics. Do not use the board, serial port, or reset line
while parallel touchscreen bring-up is running.

The latest board-side evidence is narrower than a full fresh closure:

- the current `app status` diagnostics layout and tokens were observed once on
  the H747 board with correct output
- `capture-app-lab-smoke.ps1` now separates reset failure from token failure and
  adds `-ValidateLog <path>` for host-only token checking
- the reset/capture wrapper still needs hardening because the pyOCD probe path
  is not yet stable enough to treat every run as a board-closure proof

Local probe facts from the previous board run:
`STM32CubeProgrammer_CLI` v2.17.0 does not enumerate the current Charm
CMSIS-DAP probe, while pyOCD/OpenOCD can connect but did not complete a fully
reliable internal Flash program cycle.

For the current role split against `posix_lab` and `dev_loader`, read:

- `../docs/h747_lab_dynamic_boundary_roadmap.md`
