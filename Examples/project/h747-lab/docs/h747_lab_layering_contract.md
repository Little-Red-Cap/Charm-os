# H747 Lab Layering Contract

`h747-lab` is an independent Charm project entry for the DIY STM32H747 board.
It is not a root-level `add_subdirectory` target and it does not reuse old
project directories as build dependencies.

## Build Contract

- Configure from `Examples/project/h747-lab`.
- Build explicit firmware targets:
  - `h747_lab_diag_shell`
  - `h747_lab_display_demo`
  - `h747_lab_display_raster_demo`
  - `h747_lab_player`
  - `h747_lab_posix_lab`
- Use `cmake-build-*` directories owned by this project.
- Keep root `Charm-os/CMakeLists.txt` free of H747 Lab default wiring.
- The project root lists profiles only; app/service binding lives in
  `profiles/<name>/profile.cmake`.
- Each app declares its own sources through `apps/<name>/app.cmake`.

## Layer Contract

- `board/h747_diy` owns board facts, CubeMX/HAL adaptation, clock, IRQ, startup,
  linker-facing assumptions, and low-level handles.
- `capabilities` owns candidate source-level contracts such as stream and
  display concepts. It must not depend on board, HAL, or service internals.
- `runtime` owns the single startup path and init.graph execution.
- `services` owns reusable capabilities such as console, power, memory, and
  display.
- `apps` owns scenario behavior only.
- `profiles` binds one target to one app and its required services.
- Verified low-level services may keep a C ABI at the HAL boundary, but app
  code should consume typed C++ facade headers from the same service directory.
- Apps and reusable domain code should prefer `charm::cap` concept contracts
  when the same behavior must run on H747, PC, or mock backends.

Profiles must declare:

- `H747_LAB_PROFILE_TARGET`
- `H747_LAB_PROFILE_BOARD`
- `H747_LAB_PROFILE_RUNTIME`
- `H747_LAB_PROFILE_APP`
- `H747_LAB_PROFILE_SERVICES`

## Migration Contract

Verified facts from `boards/stm32h747-player` may be migrated, especially:

- swapped software I2C PMIC transport
- SDRAM1/SDRAM2 profile-driven smoke, segmented verify, spot, bus, alias, and
  timing evidence commands
- QSPI JEDEC/read evidence
- HX8394D minimal DSI red-screen path

The old board project remains a reference source, not a dependency.

Current memory evidence note:

- `qspi probe` has produced valid JEDEC evidence (`EF/40/19`) with DCDC1 at
  3.3 V.
- SDRAM1 and SDRAM2 currently both initialize at the FMC/HAL level but fail
  first-word readback in the same way across conservative and ST-style timing
  presets (`0x13579BDF` read as `0x13579BC6` in smoke). New evidence should use
  `sdramX alias`, `sdramX waitbus`, plus the expanded CAS/read-pipe/mode-register
  `sdramX timing` sweep before changing display raster code.
- `memory mpu normal` is a diagnostic-only command for checking whether Cortex-M7
  MPU attributes are involved. It does not change the default boot policy until
  hardware evidence proves it should.
- Identical failures on both banks should be treated as a shared FMC/data-lane,
  byte-lane, control-line, or board-layout blocker before assuming an app,
  capability, or raster-demo bug.
- `display_demo` remains the red-screen display baseline. Do not move raster
  framebuffer work forward until SDRAM evidence is clean.

## Capability Contract

The first trial contracts are `Stream` and `Display`, documented in
`docs/h747_lab_capability_contract.md`.

`display_raster_demo` is the first world-shaped app: domain code consumes
`RasterDisplayWorld`; H747 and host/mock provide separate world implementations.
`player` deliberately follows that same boundary so UI/player work can be shaped
and reviewed without touching DSI/LTDC internals. `posix_lab` is the first board
target for Charm POSIX/ELF evidence and should stay isolated from display and
player policy.

These concepts are compile-time collaboration boundaries. They are not a
dynamic ELF/plugin ABI. If H747 Lab later grows a resident monitor, the ELF side
must receive an explicit capability table or hostcall table that mirrors the
same semantics without depending on C++ template or name-mangling behavior.
