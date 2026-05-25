# H747 Lab

`h747-lab` is the official Charm experiment platform for the DIY STM32H747 board.
It is an independent project entry, like `Examples/project/daplink` and
`Examples/project/player-usb-audio`: configure and build from this directory.

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
- `h747_lab_player`: the first Player-on-H747 shell. It uses the raster
  display world boundary and proves the Player app/profile can be assembled
  without depending on the old Windows Player project.
- `h747_lab_posix_lab`: the first POSIX-on-H747 shell. It assembles the Charm
  POSIX modules, RAMFS, and embedded ELF samples into a board-local evidence
  target.

Both targets share `board/h747_diy`, `runtime`, and `services`, but each target
has its own profile and app binding.

## Build

```powershell
cmake --preset h747-lab-debug
cmake --build --preset build-h747-lab-diag-shell-debug
cmake --build --preset build-h747-lab-display-demo-debug
cmake --build --preset build-h747-lab-display-raster-demo-debug
cmake --build --preset build-h747-lab-player-debug
cmake --build --preset build-h747-lab-posix-lab-debug
```

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
.\cmake-build-host-debug\h747_lab_host_display_raster_demo.exe
.\cmake-build-host-debug\h747_lab_host_player.exe
ctest --test-dir cmake-build-host-debug -C Debug --output-on-failure
```

The host executables write `.ppm` visual evidence next to the executable, so
generated files stay under `cmake-build-*` instead of the source tree.

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

Each app owns an `apps/<name>/app.cmake` manifest. Each firmware target is
declared by a `profiles/<name>/profile.cmake` manifest that binds the board,
runtime, services, and app.

Old `Examples/project/player` and `boards/stm32h747-player` are reference
assets only. Verified facts may be migrated here, but this project must not
build by directly depending on those old project directories.
