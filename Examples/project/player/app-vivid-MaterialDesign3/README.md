# Player UI - Material Design 3

This directory contains the Material Design 3 / PixelPlayer-inspired Vivid UI variant for the Player demo.

The goal is not only to make a nicer player skin. This variant is also a real-project pressure test for Vivid:

- validate page composition, text roles, style helpers, and motion patterns in a concrete app
- identify patterns that are stable enough to move into `Modules/ui/vivid`
- keep host-only conveniences visible so the UI can be made portable later

## Directory Role

This UI variant owns the visual language and page composition for the Vivid Player demo. It should not own platform bring-up, filesystem backends, audio device glue, or board-specific display code.

The main modules are:

- `player.ui`
- `player.ui_builder`
- `player.controller`

The Windows SDL3 shell lives under `Examples/project/player/win` and must remain the host-preview entrypoint.

## Collaboration Rules

- Prefer page-local implementation first; promote helpers to Vivid only after repeated use proves the abstraction.
- Keep shared helpers small and semantic. Do not create a second layout/style system beside existing Vivid helpers.
- Treat Player as a Vivid pressure test, not as a place to hide framework-level behavior.
- Do not mix host-only code into portable UI/controller paths without an explicit product or host macro.

## Portability Boundary

This variant is not MCU-strict yet. It still uses dynamic containers, host-side font/resource loading, and image decoding paths that need cleanup before a board build.

Use `docs/ui/player_portability_boundary.md` as the current boundary note for:

- Windows preview shell responsibilities
- `player_md3` real-board bridge responsibilities
- `player` probe / portability-shell responsibilities
- the shared runtime seam that both shells must keep using

## Current Focus Files

- `player.ui.cppm`
- `player.ui_builder.cppm`
- `player.ui_builder.shared.inc`
- `player.ui_builder.home.inc`
- `player.ui_builder.now_playing.inc`
- `player.ui_builder.library.inc`
- `player.controller.cppm`
- `design_notes.md`

When changing page visuals, update `design_notes.md` if the requirement or acceptance rule changes.
