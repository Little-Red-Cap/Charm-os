# Player / Vivid Portability Map

This note is a small map for keeping the Player demo beautiful while making it easier to move away from the Windows preview shell. It complements `player_portability_boundary.md`; that file records current gates, while this one records ownership and cleanup direction.

## Reference Shape

- PixelPlayer is useful as a product reference: presentation/data/theme boundaries, album-art color extraction, reusable player components, and dynamic visual feedback.
- Auxio is useful as a restraint reference: a music player can feel polished without turning every screen into a feature pile.
- LVGL is useful as an embedded UI reference: hardware independence, explicit feature gates, and low-memory behavior must be ordinary design inputs.
- Slint Embedded is useful as a backend split reference: simulator/host and embedded backends should share app semantics without sharing host conveniences.

These are references, not contracts. Charm should borrow the pressure points, not the framework shape.

## Ownership Map

- Profile / records layer: chooses product, board, scenario, host profile, and resource defaults. In the current Windows preview this is still CMake-based and intentionally not a YAML records system.
- Product config layer: exposes stable resource constants such as default font path, font sizes, and host preview VHD path. Page code should not hardcode these.
- Semantic provider layer: supplies storage, cover, theme, time, and diagnostics capabilities through small app/controller-facing seams.
- Page controller layer: owns page state, animation decisions, and player semantics. It should not know SDL, argv shape, Win32 fallback paths, or file decoder policy.
- Vivid layer: owns reusable UI mechanisms such as surfaces, text roles, snapshots, transitions, popup/sheet primitives, and render evidence.
- Host shell layer: owns SDL lifecycle, event pump, screenshots, UI-CI runner, host logging, and file-backed preview conveniences.

## Current Host Shell Split

The Windows `main.cpp` is kept as a thin assembly entry. Host-only helpers are grouped by responsibility:

- `main.host_preview.inc`: preview argv parsing, host feature logging, app config resource binding, and preview-only Library setup hooks.
- `main.font_probe.inc`: host screenshot/font diagnostics.
- `main.screenshot.inc`: host screenshot capture and export.
- `main.ui_ci.inc`: host UI-CI runner and regression probes.
- `main.host_loop.inc`: SDL event dispatch, run-loop steps, and render presentation.

The includes stay in the same anonymous namespace so this cleanup does not create a new public API or change link boundaries.

## Vivid Extraction Candidates

- Dynamic surface and text roles: keep them app-proven first, then promote the stable role model into Vivid tokens.
- Default cover art: keep Player-owned artwork generation for now; later expose a generic placeholder/image preset only if another app needs it.
- Cover theme sampling: keep the fixed-budget Player sampler as product logic; only move color role application machinery into Vivid.
- Popup / sheet primitives: Library popup work should pressure-test focus, scrim, sheet geometry, and action rows before Vivid gets a shared primitive.
- Page transition snapshots: keep Player-specific mini-to-now playing choreography local, but promote admission, budget, and fallback policies through Vivid evidence docs.

## Portability Audit

- Dynamic containers: the Material Design 3 controller and some Library helpers still use `std::string` / `std::vector` for preview-rich state and text composition.
- Cover pipeline: host cover decode is gated, but embedded-image extraction and theme sampling still allocate temporary buffers when enabled.
- Font pipeline: file-backed FreeType font loading is host/resource-profile behavior; portable builds rely on built-in fonts until a board font resource contract exists.
- Time and diagnostics: week/date stamping is behind `player.time_utils`, but playback logs, screenshots, and UI-CI are host shell responsibilities.
- Storage: host VHD defaults are profile-driven; portable storage should arrive as a board/provider capability rather than a page-level assumption.

## Portable UI Probe Contract

`portability_probe` is not a hardware simulation. It is the minimum host-side proof that the UI does not secretly require Windows preview conveniences:

- `host_cover_decode=0`, so real cover decode is unavailable and default/generated cover art must keep the UI complete.
- `host_file_fonts=0`, so file font paths are unavailable and built-in fonts must keep layout readable.
- Host storage may remain enabled so the app can still exercise real library/player flows.
- `--font-disable-system-fallback` should still pass `--ui-ci`.
- The expected success line remains `done ok=1 failed=0`.

If this probe fails, the first question should be "which provider or resource boundary leaked?", not "which board format should we emulate?"
