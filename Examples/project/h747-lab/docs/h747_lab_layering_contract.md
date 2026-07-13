# H747 Lab Layering Contract

> status: `supporting`
>
> authority: H747 Lab source tree, profile CMake and explicit firmware targets

`h747-lab` is an independent project entry for the DIY STM32H747 board. It is
configured from `Examples/project/h747-lab`, is not a root default target and
keeps build output under `cmake-build-*`.

## Build Contract

- `CMakeLists.txt` selects one or more named profiles.
- `profiles/<name>/profile.cmake` declares target, board, runtime, app and
  services.
- `apps/<name>/app.cmake` owns app source selection.
- `cmake/h747_lab_sources.cmake` owns shared board/service source inventories.
- `cmake/h747_lab_target.cmake` materializes explicit firmware targets.
- in-source builds are rejected.
- CubeMX/HAL and linker inputs are explicit cache paths; missing required input
  is a configure/build prerequisite failure, not an App or runtime failure.

Do not copy target lists into this document. Current profiles and targets come
from CMake configure output and `CMakePresets.json`.

## Source Ownership

| Directory | Ownership |
|---|---|
| `board/h747_diy` | board facts, startup, IRQ, clock, HAL/CubeMX adaptation and low-level handles |
| `capabilities` | project-local source concepts and value types, without HAL dependencies |
| `services` | peripheral lifecycle and reusable typed facades |
| `apps` | scenario behavior and app-specific adaptation |
| `profiles` | target composition and service selection |
| `runtime` | firmware startup and init-graph execution |
| `tools` | flash, capture, artifact and board-diagnostic orchestration |

Dependency direction is app -> narrow capability/service facade -> board/HAL
adapter. Apps must not reach through a service facade to mutate vendor handles,
IRQ state or peripheral ownership. A verified low-level service may keep a C
boundary for HAL integration while exposing a typed C++ facade to consumers.

## Profile Boundary

Each profile provides the variables consumed by the H747 profile helper:

```text
H747_LAB_PROFILE_TARGET
H747_LAB_PROFILE_BOARD
H747_LAB_PROFILE_RUNTIME
H747_LAB_PROFILE_APP
H747_LAB_PROFILE_SERVICES
```

Profiles select composition; they do not create a global runtime `Profile`
object or enter the App ABI. Service source lists remain owned by CMake/source,
not duplicated in README tables.

## Dynamic Images

- `dev_loader` is the resident App platform mainline.
- `app_lab` is the embedded-image App ABI baseline.
- `posix_lab` is the POSIX/C ELF compatibility line.

Their current role split is defined in
[`h747_lab_dynamic_boundary_roadmap.md`](h747_lab_dynamic_boundary_roadmap.md).
Resident Apps cross `CharmAppApi`, not project C++ concepts or HAL types.

## Evidence Ownership

- memory and external-bus facts belong in
  [`h747_lab_memory_evidence.md`](h747_lab_memory_evidence.md);
- raster display facts belong in
  [`h747_lab_raster_evidence.md`](h747_lab_raster_evidence.md);
- App Lab board flow belongs in
  [`h747_lab_app_lab_smoke.md`](h747_lab_app_lab_smoke.md);
- Player-specific evidence remains with the Player target and is not repeated in
  this project layering contract.

Host, QEMU, build-only and real-board results remain distinct evidence domains.
A successful profile configure does not prove peripheral or App behavior.

## Migration Rules

- migrate verified board facts or adapter behavior, not old project directories;
- keep external CubeMX/vendor source ownership explicit during directory moves;
- preserve target/profile boundaries before renaming folders;
- do not promote project-local `World`, `Provider`, `Profile` or backend names
  into Charm Core without the constitutional admission evidence;
- update source/CMake paths and their diagnostics in the same migration change.

Host-proof admission rules are documented in
[`h747_lab_spine_migration_boundary.md`](h747_lab_spine_migration_boundary.md).
