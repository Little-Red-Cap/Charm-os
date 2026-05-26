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

- 2026-05-23 hardware retest proved the earlier SDRAM `+0x20` alias/fault
  blocker is cleared. Keep the historical diagnostics because they remain useful
  if a future board reintroduces the same symptom.
- SDRAM1 and SDRAM2 both pass `locate`, `addr`, `lane`, `repeat`, `probe`, and
  segmented `verify` under the `is42s32800g_32m` profile.
- SDRAM1 is verified at `0xC0000000` with `size=0x02000000` and
  `ready=true verify=true`.
- SDRAM2 is verified at `0xD0000000` with `size=0x02000000` and
  `ready=true verify=true`.
- The two populated IS42S32800G-6BLI devices therefore provide 32 MiB per bank,
  64 MiB total, with current firmware evidence for read/write availability.
- `qspi probe` is expected to fail while DCDC1 remains at the default 1.5 V.
  After explicit `pmic enable dcdc1 1` plus `pmic set dcdc1 3300`, QSPI has
  produced valid JEDEC evidence (`EF/40/19`).
- `memory mpu normal` remains a diagnostic-only command for checking whether
  Cortex-M7 MPU attributes are involved. It does not change the default boot
  policy.
- If the old `+0x20` alias pattern returns, first inspect the shared FMC address
  path around external word address bit A3 / MCU PF3 before changing app,
  capability, raster-demo, or timing policy.
- `display_demo` remains the red-screen display baseline. Raster framebuffer
  work may now use SDRAM as a verified prerequisite, but it still needs its own
  LTDC layer/present evidence.

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
