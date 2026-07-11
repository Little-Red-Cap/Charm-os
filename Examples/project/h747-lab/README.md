# H747 Lab

`h747-lab` is the official Charm experiment platform for the DIY STM32H747 board.
It is an independent project entry, following the same self-contained model as
the external `Charm-dap` and `Nocturne` projects.
Configure and build it from this directory.

This project does not hook itself into the repository root `CMakeLists.txt`.
The root of this project owns its CMake entry, presets, board package, services,
profiles, and firmware targets.

## First Targets

- `h747_lab_diag_shell`: the default board evidence shell for serial, PMIC,
  SDRAM1, SDRAM2, and QSPI proof.
- `h747_lab_display_demo`: the minimal HX8394D red-screen baseline using the
  already-verified DSI video-mode + LTDC background-only path.
- `h747_lab_display_raster_demo`: the first capability-world raster baseline
  using SDRAM1 framebuffer, LTDC layer fetch, and the same DSI panel path.
- `h747_lab_input_probe`: the first touch/input evidence target. It probes the
  GT970/GT9xx path over I2C4 and samples the two hardware encoders without
  coupling that hardware debug surface to Player.
- `h747_lab_player`: the first Player-on-H747 shell. It uses the raster
  display/input world boundary and proves the Player app/profile can be
  assembled without depending on the old Windows Player project.
- `h747_lab_player_md3`: the first real MD3 Player-on-H747 target. It reuses
  `Examples/project/player/app-common` and `app-vivid-MaterialDesign3`, binds
  them to the raster framebuffer service, disables host-only storage/fonts/cover
  decode, and carves an explicit SDRAM1 render/runtime pool after the raster
  service framebuffer pool instead of relying on linker-placed SDRAM objects.
- `h747_lab_storage_firmware_runtime`: the first dedicated eMMC-to-USB MSC
  evidence target. It keeps Player out of the loop, reuses `services/storage`
  for the real eMMC block path, and exports the medium through a board-local
  USB MSC device shell with serial status evidence.
- `h747_lab_posix_lab`: the POSIX compatibility shell. It assembles the Charm
  POSIX modules, RAMFS, and embedded ELF samples into a board-local evidence
  target for the C/POSIX process line.
- `h747_lab_app_lab`: the first dynamic App ABI resident monitor and the
  current dynamic-boundary mainline. It loads
  embedded App ELF images and enters `charm_app_main(api, argc, argv)` with a
  C-compatible capability table for console, time, display, input, optional
  storage, reserved AFE, and return-code recovery.
- `h747_lab_dev_loader`: the resident development-loader skeleton. It validates
  receive/verify/launch-ready staging for downloaded images, but stays
  diagnostic-first and dry-run until the App ABI line is stable.

Both targets share `board/h747_diy`, `runtime`, and `services`, but each target
has its own profile and app binding.

For the current role split between `app_lab`, `posix_lab`, and `dev_loader`,
read:

- `docs/h747_lab_dynamic_boundary_roadmap.md`

## Build

By default, the H747 toolchain file prefers
`D:/Toolchains/Arm GNU Toolchain arm-none-eabi/latest` when it exists, then
falls back to `15.2 rel1`, then to `arm-none-eabi-*` from `PATH`. To force a
specific local toolchain, configure with:

```powershell
cmake --preset h747-lab-debug -DH747_LAB_ARM_GNU_TOOLCHAIN_ROOT="D:/Toolchains/Arm GNU Toolchain arm-none-eabi/latest"
```

If an existing `cmake-build-*` directory was already configured with another
Arm toolchain, use `cmake --fresh` with the same preset. CMake caches compiler
and binutils paths, so changing only `H747_LAB_ARM_GNU_TOOLCHAIN_ROOT` may leave
the old linker and archive tools in place.

```powershell
cmake --preset h747-lab-debug
cmake --build --preset build-h747-lab-diag-shell-debug
cmake --build --preset build-h747-lab-display-demo-debug
cmake --build --preset build-h747-lab-display-raster-demo-debug
cmake --build --preset build-h747-lab-input-probe-debug
cmake --build --preset build-h747-lab-player-debug
cmake --build --preset build-h747-lab-player-md3-debug
cmake --build --preset build-h747-lab-storage-firmware-runtime-debug
cmake --build --preset build-h747-lab-posix-lab-debug
cmake --build --preset build-h747-lab-app-lab-debug
cmake --build --preset build-h747-lab-dev-loader-debug
```

Configure now runs a small H747 BSP doctor before generating Ninja build files.
If the selected `DRAFT_ROOT` is missing required CubeMX-generated files such as
`fmc.h/.c`, `quadspi.h/.c`, or `spi.h/.c`, CMake stops early and prints the BSP
root, missing files, affected target class, and repair hint. Treat that as a BSP
source-set problem, not an ELF, AppRuntime, packetstream, or Store regression.
`h747_lab_dev_loader` currently requires the SDRAM/FMC, QSPI, eMMC/SDMMC, and
default SPI-init boundary to be coherent before board ELF validation can run.

The alternate HX8394D table can be built with:

```powershell
cmake --preset h747-lab-display-github4lane-debug
cmake --build --preset build-h747-lab-display-demo-github4lane-debug
```

Build products are written under `cmake-build-*` inside this directory.
Run one H747 firmware build at a time inside the same binary directory. GCC
C++ module sidecar files are target-local, but parallel `cmake --build` sessions
against one Ninja directory can still race on build metadata.

The host/mock display raster path has a separate native entry:

```powershell
cmake -S host -B cmake-build-host-debug
cmake --build cmake-build-host-debug
.\cmake-build-host-debug\Debug\h747_lab_host_display_raster_demo.exe
.\cmake-build-host-debug\Debug\h747_lab_host_player.exe
ctest --test-dir cmake-build-host-debug -C Debug --output-on-failure
```

The host CTest set covers display raster CI, Player CI, and profile/evidence
selection CI. The host executables write `.ppm` visual evidence next to the
executable, so generated files stay under `cmake-build-*` instead of the source
tree.

## Current Board Evidence

The current DIY H747 memory baseline is recorded in
`docs/h747_lab_memory_evidence.md`.
The current raster display baseline is recorded in
`docs/h747_lab_raster_evidence.md`.
The dynamic App ABI board smoke protocol is recorded in
`docs/h747_lab_app_lab_smoke.md`.
The dynamic-boundary role split is recorded in
`docs/h747_lab_dynamic_boundary_roadmap.md`.

- PMIC communication uses the `i2c1_gpio_swapped` software I2C transport.
- SDRAM1 is verified as `is42s32800g_32m`, 32 MiB at `0xC0000000`.
- SDRAM2 is verified as `is42s32800g_32m`, 32 MiB at `0xD0000000`.
- Both SDRAM banks pass `locate`, `addr`, `lane`, `repeat`, `probe`, and
  segmented `verify`.
- QSPI reports JEDEC `EF/40/19` after DCDC1 is explicitly set to 3.3 V.
- `display_raster_demo` proves a double-buffered ARGB8888 SDRAM1 framebuffer
  path to LTDC/DSI with stable full-screen color switching.

## Layout

- `board/h747_diy`: board facts and STM32 HAL/CubeMX adaptation.
- `capabilities`: candidate Charm capability concepts trialed by this project.
- `runtime`: single `main -> foundation -> init.graph -> profile -> app loop`.
- `services`: reusable board services consumed by apps.
- `apps`: app logic only; apps do not touch HAL handles directly.
- `profiles`: explicit target assembly for board, runtime, services, and app.
- `docs`: contracts and migration notes for this formal line.

Service directories may keep a narrow C ABI where it protects verified HAL
bring-up facts, but apps should consume the typed C++ facade headers such as
`console_service.hpp`, `power_service.hpp`, `memory_service.hpp`, and
`display_service.hpp`.

Portable app/domain code should move toward `charm::cap` concepts from
`capabilities/`. These concepts are source-level contracts, not an ELF ABI; a
future hot-load boundary should map the same semantics into a stable capability
table or hostcall table.

`display_raster_demo` is the first app shaped this way: its domain code targets
a `RasterDisplayWorld`, while host/mock and H747 provide different worlds.
`input_probe` is the first input truth target; Player should consume the same
`InputFrame` / `InputWorld` facts instead of learning about GT970, I2C4, or TIM
encoder details.

`storage_firmware_runtime` is now the dedicated eMMC/USB MSC evidence line. It
focuses on exposing the real eMMC block path over USB MSC without pulling
Player into the storage-export role. Broader QSPI/eMMC firmware image storage,
resident monitor, bootloader, and ELF capability-table work still continue in
their own lines. The first resident monitor proof lives in `h747_lab_app_lab`:
Bootloader remains responsible for loading firmware/runtime, while the runtime
monitor owns dynamic App image source, ELF load buffer, capability table, app
start, and app exit reporting.

`h747_lab_posix_lab` stays as the POSIX compatibility line, while
`h747_lab_dev_loader` is currently a post-mainline development acceleration
experiment rather than a prerequisite for the resident App ABI path.

Each app owns an `apps/<name>/app.cmake` manifest. Each firmware target is
declared by a `profiles/<name>/profile.cmake` manifest that binds the board,
runtime, services, and app.

Old `Examples/project/player` and `boards/stm32h747-player` are reference
assets only. Verified facts may be migrated here, but this project must not
build by directly depending on those old project directories.
