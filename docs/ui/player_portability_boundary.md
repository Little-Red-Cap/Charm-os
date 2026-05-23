# Player Portability Boundary

This note tracks the current portability boundary for the Player UI work. The goal is to keep the Player demo useful as a Vivid pressure test without letting host-only conveniences become implicit product APIs.

## Current Boundary

- `Examples/project/player/win` is the Windows SDL3 host shell.
- `PLAYER_SCENARIO` selects the Windows UI target (`ink`, `vivid`, or `vivid_md3`).
- `PLAYER_HOST_PROFILE` selects a Windows host feature record. `preview_full` is the default full preview; `portability_probe` disables host cover decoding and file fonts while keeping the UI runnable.
- `PLAYER_RESOURCE_*` and `PLAYER_HOST_STORAGE_VHD_PATH` describe Windows preview resource defaults and are forwarded into `player.product_config`.
- `CHARM_PLAYER_HOST_UI=1` marks host-preview code paths.
- `CHARM_PLAYER_HOST_STORAGE=1` marks host storage defaults such as the Windows VHD path.
- `CHARM_PLAYER_PC_FONT_CACHE=1` is only valid for the host shell and must not be treated as a portable font path.
- `CHARM_PLAYER_PLAYBACK_LOG=1` enables host playback diagnostics.
- `app-vivid-MaterialDesign3` is still a rich host-side UI variant. It is not yet MCU-strict because it still uses dynamic containers and host asset loading.

## Layering Direction

The Player portability boundary should follow the same broad shape as the DAPLink-style split:

- Product/profile records decide which features, providers, and board resources are composed.
- Common source modules provide stable capabilities without knowing the host preview shell.
- Host shells own preview-only windowing, diagnostics, screenshots, and file-backed convenience paths.
- Page controllers should depend on semantic providers, not on Windows, SDL, or file decoder details.
- The useful DAPLink reference is its port/profile/contract discipline, not its USB firmware details; Player ports should translate that discipline into display, input, storage, font, cover, and clock providers.

For the current ownership map, host shell split, Vivid extraction candidates, and portable UI probe contract, see `player_vivid_portability_map.md`.

## Host-Only Dependencies

- SDL3 windowing, event pump, renderer, and screenshot flow live in `Examples/project/player/win`.
- `player.runtime` owns the shared product lifecycle for Vivid Player targets: app construction, storage bootstrap, controller binding, input dispatch, ticking, frame rendering, and shutdown. Windows/SDL calls into this shell instead of open-coding product lifecycle steps.
- The Windows host shell still owns preview arguments, SDL initialization, screenshot capture, UI-CI entry points, and visual preview hooks such as the spectrum overlay.
- Player display output now goes through a small display HAL contract: app-common renders into `PlayerDisplaySurface`, and host/board code presents that surface through a `PlayerDisplaySink`.
- SDL is only the current Windows preview display adapter. A Win32, Linux fbdev/DRM, or STM32 SDRAM/LTDC adapter can present the same `pixels / width / height / stride / pixel_format` surface without changing Player UI code.
- `PlayerPlatform` binds an externally supplied surface; the Windows preview owns one default buffer in the host shell, while board code can pass an SDRAM framebuffer surface.
- `make_board_display_sink()` is the board adapter seam for SDRAM/LTDC-style present. Board code owns callback state and may map clean-cache, dirty flush, and present/flip to HAL, DMA2D, LTDC, or a no-op.
- `MemoryDisplaySink` is the portable/CI seam for SDRAM-style output. `player.runtime_probe` renders the real Player runtime into an external buffer and verifies present metadata.
- `--runtime-memory-smoke` is the Windows host adapter entry for that probe: it runs before SDL initialization, builds the real `PlayerRuntime`, renders MD3 Player into an external memory surface, and exits without opening a host window.
- SDL event decoding is isolated in a host input adapter include. The adapter now emits `PlayerInputEvent`; the Player app is the only place that bridges product input to Vivid raw input, wheel events, or controller commands.
- `read_player_touch_events()` is the board touch adapter seam: board code owns sampling state/source, while app-common converts fixed-capacity touch samples into `PlayerInputEvent` batches.
- Windows command-line preview flags are parsed into a host-local `PreviewOptions` structure; page controllers should not learn about argv shape.
- Host feature defaults are composed by `PLAYER_HOST_PROFILE`; explicit `CHARM_PLAYER_HOST_*` cache values remain valid overrides for local experiments.
- Windows preview resource defaults are composed by `product_player_host_resources.cmake`; common code reads them through `player.product_config`.
- Win32/GDI fallback font caching is gated by `CHARM_PLAYER_HOST_UI && CHARM_PLAYER_PC_FONT_CACHE && _WIN32`.
- Host VHD storage defaults are gated by `CHARM_PLAYER_HOST_STORAGE`.
- Product resource defaults live in `player.product_config`; host entry points should select or override them instead of hardcoding resource paths.
- `AppConfig` stores resource paths in fixed-capacity slots so the app-level config boundary does not require dynamic strings.
- `AppConfig` groups font resource data as `FontResourceConfig`; file-backed fonts must opt in through that record instead of leaking host TTF fields into page code.
- Embedded and file-backed cover decoding is gated by `CHARM_PLAYER_HOST_COVER_DECODE`; portable targets can keep using generated/default covers or later provide pre-decoded resource images.
- The Windows host shell prints `[player.features]` at startup so a preview build and a portability-probe build can be distinguished from logs. A probe with `host_cover_decode=0` is expected to skip real cover decoding.
- `player.cover` exposes `CoverResourceProviderFn` before the legacy `CoverProviderFn` host decoder. Portable targets can return pre-decoded/resource-backed views without changing page controllers; host decode remains gated by `CHARM_PLAYER_HOST_COVER_DECODE`.
- When no cover can be decoded, Now Playing and the mini bar use generated default cover art instead of leaving image slots empty.
- Cover-theme sampling is capped at a fixed 128x128 working set before palette extraction, so Player-side theme sampling has an explicit memory budget.
- FreeType/VFS file font binding is gated by `CHARM_PLAYER_HOST_FILE_FONTS`; portable targets fall back to built-in fonts until a board resource-font contract exists.
- Playback and filesystem diagnostics are gated by explicit Player feature macros, not `_WIN32`.
- FreeType file-backed font loading is a host/product resource path until Vivid has a board resource contract.
- Calendar/week stamping is routed through `player.time_utils` so page controllers do not carry platform time branches.

## Product Runtime Shell v0

`player.runtime` is the current portable product shell for the Vivid Player line:

- It composes `App`, `PlayerController`, and `PlayerPlatform` without depending on SDL, Win32, screenshots, or command-line parsing.
- It consumes a `PlayerRuntimeConfig<Page>` record for app config, storage config, start page, initial track, auto-start, and clear color.
- It exposes the same product actions a board shell needs: `bootstrap`, `tick`, `dispatch_input`, `render`, and `shutdown`.
- It still receives host-owned `PlayerPlatform` and controller storage. This keeps v0 small while allowing a future board shell to provide an SDRAM-backed `PlayerDisplaySurface` and board-owned controller storage.
- `player.runtime_probe` is the reusable memory proof around this shell. It receives externally owned clock, platform, controller storage, runtime storage, display sink, and runtime config, then runs one bootstrap/tick/render/shutdown cycle.
- The Windows host exposes `--runtime-memory-smoke` for a no-window runtime proof. It still lives in the host executable for convenience, but the construction path avoids SDL window, renderer, texture, event pump, screenshots, and overlay preview hooks.

## Display/Input HAL v0

The display boundary is intentionally small:

- App side: render into `PlayerDisplaySurface`.
- Adapter side: implement `PlayerDisplaySink::present(surface, dirty)`.
- Frame lifecycle: `render_player_frame()` owns clear, transition destination capture, scene render, and optional present so SDL preview, UI-CI, and board sinks do not duplicate render choreography.
- Surface contract: buffer pointer, dimensions, stride, pixel format, and ownership are explicit.
- Board SDRAM contract: board code supplies the framebuffer address and stride, then maps `present` to cache clean, dirty flush, DMA2D copy, LTDC front-buffer flip, or a no-op for single-buffer scanout.
- Board sink contract: `PlayerBoardDisplayCallbacks` are optional and ordered as clean-cache, flush-dirty, then present/flip. The sink clips dirty regions and records the final surface/dirty evidence for CI.
- Host preview contract: SDL creates a texture matching the Player surface format and only copies/presents the final surface.

The matching input boundary is also small:

- Host or board code samples hardware/window events.
- Adapter code converts samples to `PlayerInputEvent`.
- `PlayerInputEvent::Pointer` covers mouse and touch down/move/up/cancel samples.
- `PlayerInputEvent::Wheel` is bridged by `player.app`, so host adapters no longer dispatch directly to `Scene`.
- `PlayerInputEvent::Command` is delivered to the controller through `handle_input_command()`, where product actions map to page or playback semantics.
- Board touch integration only needs to provide `{down, x, y, id, ms}` samples through the `PlayerTouchSampleSource` shape; `read_player_touch_events()` turns those samples into a fixed-capacity event batch. It does not need to simulate SDL or know Vivid internals.
- `RawInputEvent` remains the Charm IO fact input below this boundary. It is no longer the Windows Player host adapter contract.

This means Win32, Linux, SDL, and STM32 are peers at the adapter layer. None of them should leak into Player page builders or controller semantics.

## Portability Blockers To Retire

- Dynamic `std::vector` / `std::string` state in the Material Design 3 controller and cover pipeline.
- Cover extraction and theme sampling allocate temporary buffers for embedded images.
- Library popup and preview text helpers still build temporary `std::string` values.
- Page screenshots and UI CI helpers belong to the host shell, not to the portable Player core.

## Next Cleanup Order

1. Define a board font provider/resource package path so file fonts stay a host implementation.
2. Split time/diagnostics providers only where a concrete board target needs them.
3. Group UI-CI evidence by subsystem once `main.ui_ci.inc` becomes hard to review.
4. Replace controller-owned dynamic track/list caches with fixed-capacity storage where they enter MCU-strict paths.
