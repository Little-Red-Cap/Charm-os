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

This variant is now shared by the Windows MD3 preview and the H747
`player_md3` PRODUCT target. Host-only conveniences such as file fonts,
dynamic cover decode, screenshots/UI CI, and layered transitions must stay
behind runtime policy, product profile, or host-only build gates.

H747 uses the same controller/pages with capability downgrade: built-in fonts,
resource/placeholder covers, `StaticCut`, no debug UI, and no host cover-theme
extraction. New visual work must not introduce a board-only UI fork.

## Current Focus Files

- `player.ui.cppm`
- `player.ui_builder.cppm`
- `player.ui_builder.shared.inc`
- `player.ui_builder.home_layout.inc`
- `player.ui_builder.now_playing_layout.inc`
- `player.ui_builder.library_layout.inc`
- `player.ui_builder.home.inc`
- `player.ui_builder.now_playing.inc`
- `player.ui_builder.library.inc`
- `player.controller.cppm`
- `design_notes.md`
- `maintainability_audit.md`

When changing page visuals, update `design_notes.md` if the requirement or acceptance rule changes.
When changing code structure or deciding whether a Player pattern should move into Vivid, update
`maintainability_audit.md`.

## Visual Recovery Gate

Now Playing is the first visual recovery slice after the portability pass. Any
visual change must keep these gates green:

- Windows `charm-player-win-vivid-md3` build and `--ui-ci`
- H747 `h747_lab_player_md3 -j 1` PRODUCT build and refreshed memory evidence
- no H747 regression of file-font/debug/FreeType/layered/snapshot/dynamic
  cover-theme gates

The frozen pre-recovery H747 baseline is `RAM_D1 46008 B`, `FLASH 712440 B`,
and `PlayerController 9552 B`. The latest Now Playing closure slice refreshed
the evidence at `RAM_D1 46016 B`, `FLASH 714688 B`, and
`PlayerController 9560 B`.

Windows `--ui-ci` now includes `now_playing_matrix_*` cases for structure, text
sync, cover fallback, play/pause, long text, and Library-to-Now-Playing sync.
It also includes `now_playing_seek_*` cases for progress rect/hitbox, dragging
preview, seek commit, cancel restore, no-duration fallback, and time-label sync.
The current main-experience closure cases are `now_playing_closure_*`, covering
layout stack, cover stage, title block, control hierarchy, and fallback +
no-duration stability. The Windows screenshot evidence is generated under the
build directory as `generated/ui-ci/now_playing_closure.ppm`.
