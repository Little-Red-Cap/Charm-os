# H747 Lab Layering Contract

`h747-lab` is an independent Charm project entry for the DIY STM32H747 board.
It is not a root-level `add_subdirectory` target and it does not reuse old
project directories as build dependencies.

Its current role in the broader Charm roadmap is defined by
`docs/architecture/rte_to_h747_platform_roadmap.md`: H747 Lab is the
real-board pressure field for the `RTE -> H747` platformization line, with
`Display + Player` as the first host/board shared app semantics slice.

If the current question is specifically "which Spine/RTE semantics should
survive into H747 worlds, resident monitor surfaces, or future ELF ABI
boundaries", continue with:

- `docs/h747_lab_spine_migration_boundary.md`

## Build Contract

- Configure from `Examples/project/h747-lab`.
- Build explicit firmware targets:
  - `h747_lab_diag_shell`
  - `h747_lab_display_demo`
  - `h747_lab_display_raster_demo`
  - `h747_lab_input_probe`
  - `h747_lab_player`
  - `h747_lab_player_md3`
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
- `services` owns reusable capabilities such as console, power, memory,
  display, and input. Input owns GT970/I2C4, TP_RST/TP_INT, TIM encoder, and
  button board facts; apps consume `InputFrame`/`InputWorld` facts instead.
- `apps` owns scenario behavior only.
- `profiles` binds one target to one app and its required services.
- Verified low-level services may keep a C ABI at the HAL boundary, but app
  code should consume typed C++ facade headers from the same service directory.
- Apps and reusable domain code should prefer `charm::cap` concept contracts
  when the same behavior must run on H747, PC, or mock backends.

For `h747_lab_player_md3`, this layering means:

- `services/display` owns raster framebuffer presentation and LTDC-facing
  facts.
- `services/input` owns GT970/GT9xx probing, I2C4 transport, encoder counters,
  and button GPIO facts, and exposes typed input snapshots through
  `input_service.hpp`.
- `apps/player_md3` owns only the board-to-Player adapter: SDRAM render/runtime
  pool selection, `PlayerDisplaySurface` binding, `PlayerRuntimeShell`
  lifecycle, translation from generic `InputFrame`/pointer/button observations
  into `PlayerInputEvent`, and serial diagnostics.
- `Examples/project/player/app-vivid-MaterialDesign3` remains the visual and
  controller source of truth. H747 Lab must not replace it with a separate
  board-only mock UI.

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
- GT970 best-effort touch probe and dual encoder evidence through
  `services/input`

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
`input_probe` is the first input truth target and owns GT970/encoder evidence.
`player` deliberately follows `RasterDisplayInputWorld` so UI/player work can be
shaped and reviewed without touching DSI/LTDC, I2C4, or TIM internals.
`player_md3` is the first real shared-Player vertical slice: it instantiates the
desktop MD3 `PlayerRuntime`, renders into an SDRAM-backed external surface,
presents through the raster display sink, and translates generic
touch/encoder facts from `input.service` into Player semantic input.
`posix_lab` is the first board target for Charm POSIX/ELF evidence and should
stay isolated from display and player policy.

## Player MD3 Board Evidence

`h747_lab_player_md3` is accepted through serial status evidence, not by
inspecting app internals. The important fields are:

- `real_md3=1 mock=0`
- `smoke=1/11111`
- `delta=<frames>/<presents>` with non-zero values on loop status lines
- `content=<bg>:<non_bg>@<min>-<max>` with non-zero content pixels
- `exec_fail=0`, `co=0`, and `to=0`
- `input=<polls>/<events>`, `t=<probe>/<ready>/<down>@<x>,<y>`, and
  `e=<touch>/<encoder>/<button>` for the input bridge

The console prompt `h747-player-md3>` is a bring-up adapter. Its `status`,
`touch probe`, navigation, transport, and mode commands may inject the same
Player semantic input path used by hardware samples, but they do not become a
controller or page dependency.

## Deferred Firmware And Storage Runtime

`storage_firmware_runtime` is reserved for the later resident-runtime direction.
It is not part of the current touch/input line.

Future work should split it into:

- eMMC / USB MSC block-device evidence
- QSPI/eMMC firmware image storage format
- resident monitor / bootloader / ELF capability table or hostcall table

Until that monitor exists, H747 Lab keeps using independent firmware targets for
parallel feature work.

These concepts are compile-time collaboration boundaries. They are not a
dynamic ELF/plugin ABI. If H747 Lab later grows a resident monitor, the ELF side
must receive an explicit capability table or hostcall table that mirrors the
same semantics without depending on C++ template or name-mangling behavior.
