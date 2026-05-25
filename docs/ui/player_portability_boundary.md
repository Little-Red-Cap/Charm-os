# Player Portability Boundary

This note records the current portability contract for the real Player product
path. It is the source of truth for what must stay in the shared Player runtime
and what is allowed to remain host- or board-local.

## Runtime Roles

- `Examples/project/player/app-vivid-MaterialDesign3`
  owns the real product UI, controller, cover theme, and page composition.
- `Examples/project/player/win`
  is the Windows preview shell. It may own SDL windowing, SDL present, SDL input
  translation, screenshot helpers, UI-CI flags, and host preview convenience.
- `Examples/project/h747-lab/apps/player_md3`
  is the real H747 MD3 bridge. It must instantiate the shared Player runtime and
  render the real MD3 UI, not a hand-drawn replacement.
- `Examples/project/h747-lab/apps/player`
  remains the smaller probe / portability shell. It is useful for CI and board
  capability bring-up, but it is not the product UI path.

## Shared Runtime Seam

The shared seam for both Windows preview and future board builds is:

- `player.display`
- `player.input`
- `player.runtime`
- `player.runtime_shell`

These layers may know about Player runtime semantics, display surfaces/sinks,
and normalized Player input events. They must not depend on SDL, Win32, HAL,
board console, or H747 services.

## Current Host-Only Assumptions

The following features remain host-gated and must not leak into the portable
runtime shell:

- SDL window lifecycle and present path
- SDL input event translation
- screenshot / GIF export
- host storage convenience paths
- host cover decode
- host file fonts / FreeType

The board side must keep using fallback seams until real providers are ready:

- empty or board-local storage config
- default cover / resource cover provider
- built-in font fallback
- board display sink / framebuffer surface
- board input provider

## Cleanup Order

When continuing portability work, prefer this order:

1. Keep the real Player runtime path shared.
2. Add or improve provider seams in `app-common`.
3. Bind those seams in Windows or board shells.
4. Only promote abstractions into wider Charm/Vivid layers after both shells
   survive the same product path.
