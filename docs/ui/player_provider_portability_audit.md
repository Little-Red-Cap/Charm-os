# Player Provider Portability Audit v0

This audit records where the Player demo already has a provider boundary, where it still depends on the Windows preview host, and which dynamic allocations are product semantics versus replaceable preview implementation details.

It is intentionally narrower than a full embedded port plan. The goal is to make the next cleanup slice obvious without changing Player UI behavior.

## Scope

- Audited code: `Examples/project/player/app-common`, `Examples/project/player/app-vivid-MaterialDesign3`, and the Windows host shell under `Examples/project/player/win`.
- User-owned Library popup work in `player.controller.library.inc` and `player.ui_builder.library.inc` was treated as read-only context.
- Hardware projects, `.claude/`, `artifacts/`, YAML records, public Vivid APIs, and board-specific validation are out of scope.
- The current profiles remain `preview_full` and `portability_probe`; this document does not change build behavior.

## Boundary Vocabulary

- Provider boundary: app/controller code calls a small semantic seam instead of knowing the host backend.
- Host gate: compile-time or profile-selected switch that keeps a Windows preview convenience from becoming product API.
- Product semantic state: dynamic state that describes the player experience and may need a fixed-capacity version later.
- Replaceable implementation state: dynamic or file-backed state that belongs to host preview, scanning, diagnostics, or a temporary adapter.

## Summary Matrix

| Area | Current boundary | Host gate | Dynamic allocation points | Next priority |
| --- | --- | --- | --- | --- |
| Cover | `player.cover` exposes `CoverProviderFn`; page/controller code can use generated/default cover art when decode is unavailable. | `CHARM_PLAYER_HOST_COVER_DECODE`; profile log prints `host_cover_decode`. | Host decode buffers, embedded image extraction, palette/theme sampling work buffers. | Add a pre-decoded/resource-backed cover provider contract. |
| Theme | Player-owned cover sampling feeds dynamic surface/text roles; Vivid owns reusable role application candidates. | Indirectly tied to cover decode availability. | Fixed-budget 128x128 sampling workspace, palette candidates, controller theme state. | Keep sampling in Player; consider moving role application rules into Vivid after more app pressure. |
| Font | Product config carries font paths and sizes; host preview binds file fonts only when enabled. | `CHARM_PLAYER_HOST_FILE_FONTS`, `CHARM_PLAYER_PC_FONT_CACHE`; profile log prints `host_file_fonts`. | FreeType/VFS font buffers, Win32/GDI glyph cache vectors, preview argv strings. | Define a board font resource/provider contract before replacing file fonts. |
| Display/Input | `PlayerDisplaySurface` and `PlayerDisplaySink` separate Player rendering from SDL; SDL input mapping is host-local. | Windows preview uses SDL adapter; memory sink proves external framebuffer output. | Preview SDL texture/window state; UI-CI external buffer is fixed-size static storage. | Add Win32/Linux/board SDRAM adapters as peers without changing Player UI. |
| Time | `player.time_utils` keeps date/week formatting out of page code. | No dedicated board clock gate yet; Windows host binds the runtime time source. | Mostly fixed strings today; playback uses runtime clock timestamps. | Add a time/clock provider adapter when a portable Player target appears. |
| Storage | `player.storage` and `PlayerApp` isolate scan/mount flow; product config carries host VHD default. | `CHARM_PLAYER_HOST_STORAGE`; profile/resource records select VHD defaults. | Track lists, scan queues, stats history, path buffers, filesystem traversal state. | Introduce board storage capability/provider instead of page-level storage assumptions. |
| Diagnostics | Host shell owns screenshot, font probe, UI-CI, and preview logging includes. | `CHARM_PLAYER_PLAYBACK_LOG`, `CHARM_PLAYER_FS_LOG`, host shell only screenshot/UI-CI paths. | Screenshot paths, UI-CI probe state, font probe output strings, playback log formatting. | Keep host-only; split `main.ui_ci.inc` by evidence group later. |
| UI-CI | Windows host runner proves preview and portability-probe contracts without becoming app semantics. | Host executable path plus `--ui-ci`; probe uses disabled cover decode/file fonts. | Case-local probe state and temporary strings. | Group by frame budget, layer, navigation, transition, and list evidence. |

## Cover

Current state:

- `player.cover` already has a small provider seam through `CoverProviderFn`.
- `player.cover` now checks an optional `CoverResourceProviderFn` before host decode, so board/resource builds can return a pre-decoded `CoverResourceView`.
- The Windows preview can install the host decoder when `CHARM_PLAYER_HOST_COVER_DECODE=1`.
- `portability_probe` disables host decode, so generated/default cover art must keep Now Playing and the mini bar complete.
- The UI path should not assume that embedded album art can always be decoded at runtime.

Dynamic allocation and portability notes:

- Host-side decode and embedded-image extraction still need temporary image buffers.
- Resource-backed covers still copy into `CoverImage::argb` in v0; fixed-capacity ownership is a later portable-profile concern.
- Cover-theme sampling uses a fixed 128x128 working set before palette extraction, which is good because the memory budget is visible.
- Some theme and transition state remains in the MD3 controller; this is product semantic state, not a host dependency by itself.

Recommended next slice:

- Add real board/resource implementations for the new pre-decoded cover provider seam.
- Keep host decode as one provider implementation, not the default mental model.
- Continue using generated/default cover art as the fallback contract.

## Theme

Current state:

- Album-art color sampling is Player product logic because it decides the music experience.
- Surface/text role application is a Vivid extraction candidate because it is reusable UI machinery.
- Mini bar and Now Playing should remain visually tied to the same semantic theme roles, but not to the same host decoder.

Dynamic allocation and portability notes:

- The sampling workspace is bounded, but palette extraction and controller theme state still deserve fixed-capacity review later.
- Dynamic role state is acceptable for the Windows preview but should remain isolated from low-level render/Vivid contracts.

Recommended next slice:

- Keep cover sampling local to Player.
- Document a stable `CoverTheme` or similar product-level result before moving any role machinery into Vivid.
- Only promote role application to Vivid after another page or app needs the same rule.

## Font

Current state:

- `player.product_config` owns default font path and size constants.
- `AppConfig` carries a `FontResourceConfig` record with fixed-capacity paths, font sizes, and an explicit `file_backed` bit.
- Host file fonts are enabled only through `CHARM_PLAYER_HOST_FILE_FONTS`.
- Win32/GDI fallback glyph caching is guarded by `CHARM_PLAYER_HOST_UI && CHARM_PLAYER_PC_FONT_CACHE && _WIN32`.
- `portability_probe --font-disable-system-fallback` is the current proof that built-in fonts can keep the UI readable.

Dynamic allocation and portability notes:

- `player.font_cache` uses `std::vector` for host glyph and bitmap caches. This is host preview implementation state.
- FreeType/VFS font loading remains file-backed and should not be treated as a board resource contract.
- Page code still asks for rich font variants; that is visual product intent, not necessarily a host dependency.

Recommended next slice:

- Define a board font resource/provider contract with built-in packages and optional external resource packs.
- Keep file paths as host/profile resource defaults.
- Avoid spreading file font assumptions into page builders or controllers.

## Display and Input

Current state:

- `player.display` defines `PlayerDisplaySurface`, `PlayerDisplaySink`, pixel format, dirty region, and ownership metadata.
- `PlayerPlatform` binds a supplied surface and renders Vivid Scene output into it through `RuntimeCanvas`.
- The Windows preview owns its default display buffer in the host shell, then presents it through an SDL display sink.
- `MemoryDisplaySink` is the current SDRAM-style seam: UI-CI renders a frame into an external buffer and verifies present metadata.
- SDL input event decoding lives in a host-local adapter include; Player app code consumes `RawInputEvent` and `UiKey`.

Dynamic allocation and portability notes:

- The display HAL contract itself does not require dynamic allocation, exceptions, RTTI, SDL, Win32, or Linux APIs.
- Rich Vivid scene/controller state is still not MCU-strict, but the final display output can now target an externally owned framebuffer.
- SDL texture/window state remains host-only preview implementation state.

Recommended next slice:

- Add a Win32 display sink to prove the SDL dependency can be replaced on desktop without touching Player UI.
- Add a Linux framebuffer/DRM sink when a Linux host target appears.
- Add a board SDRAM/LTDC sink that maps `present` to cache clean, dirty flush, DMA2D copy, or LTDC buffer flip.

## Time

Current state:

- Calendar/week labels are routed through `player.time_utils`.
- The Windows host binds the runtime time source during host bootstrap.
- Playback state uses runtime clock timestamps for elapsed/progress behavior.

Dynamic allocation and portability notes:

- Time labels are mostly fixed-string friendly today.
- There is no dedicated Player clock provider yet, so a board target would still need a clean adapter point.

Recommended next slice:

- Add a small time/clock provider only when the portable Player target needs it.
- Keep page code consuming formatted labels or semantic time values rather than platform time APIs.

## Storage

Current state:

- `player.storage`, `player.media_scan`, and `PlayerApp` keep scan/mount flow out of individual pages.
- Host VHD defaults are profile/resource-record data, not page constants.
- `CHARM_PLAYER_HOST_STORAGE` marks Windows host storage defaults.

Dynamic allocation and portability notes:

- Track lists, stats history, scan traversal, and path composition are still the largest dynamic/product-state area.
- Some storage utilities already use `FixedString` and fixed vectors, which is the right direction.
- Rich Library behavior is currently user-owned work and should not be reshaped by this audit.

Recommended next slice:

- Introduce a board storage provider/capability when there is a non-Windows storage target.
- Later replace controller-owned dynamic track/list caches with fixed-capacity storage where it affects the portable profile.
- Do not force the Windows preview VHD shape onto boards.

## Diagnostics

Current state:

- Startup feature logging prints profile, storage, cover decode, file fonts, and playback log state.
- Screenshot capture, GIF/PPM export, font probe, and UI-CI live in the Windows host shell includes.
- Playback and filesystem logs are behind explicit Player feature macros instead of raw `_WIN32` checks.

Dynamic allocation and portability notes:

- Diagnostics use host strings and file paths freely, which is acceptable while they stay host-only.
- `main.ui_ci.inc` is large, but it is evidence infrastructure rather than app semantics.

Recommended next slice:

- Keep diagnostics behind host shell and explicit gates.
- Split `main.ui_ci.inc` by evidence family when it becomes hard to review.
- Avoid adding screenshot or probe assumptions to controllers.

## Dynamic Memory Classification

Product semantic state that may later need fixed-capacity equivalents:

- MD3 controller state for page selection, transition choreography, now-playing data, dynamic themes, and rich display text.
- Storage/library track lists and stats history.
- Cover-theme results and animation state.

Replaceable preview or adapter implementation state:

- Win32/GDI font cache glyph vectors and bitmap buffers.
- FreeType/VFS file font loading buffers.
- Host cover decode buffers and embedded-image extraction buffers.
- Screenshot paths, GIF/PPM export state, and UI-CI case-local scratch data.
- Preview argv parsing strings and host-only diagnostics text.

Already moving in the portable direction:

- `FixedString` app config and path helpers.
- `FontResourceConfig` keeps file-backed font resource data behind the app config boundary instead of scattering TTF fields through host bootstrap.
- `CoverResourceProviderFn` gives portable builds a pre-decoded cover path before host decode.
- `PlayerDisplaySurface` lets board code provide the final framebuffer memory instead of forcing Player to own or SDL-present it.
- `MemoryDisplaySink` gives CI a SDRAM-style external-buffer proof without needing board hardware.
- Explicit host feature gates in `player.host_features`.
- CMake host profiles that make preview/probe differences visible.
- `CoverProviderFn` and generated/default cover fallback.

## Next Slice Candidates

1. Display sink adapters: add Win32/Linux/SDRAM implementations behind the same `PlayerDisplaySink` contract.
2. Font provider: define board font resource ownership and keep file fonts as a host/profile implementation.
3. Time/diagnostics provider: keep formatting and logging out of page code, but avoid over-abstracting before a board target exists.
4. UI-CI grouping: split `main.ui_ci.inc` by evidence family without changing behavior.
5. Controller dynamic state slimming: replace only the dynamic state that blocks `portability_probe` or a concrete board profile.

## Probe Contract Reminder

The portability probe is not a hardware simulation. Its question is narrower and more useful:

- Does the UI still run when host cover decode is disabled?
- Does it remain readable when file fonts and system fallback are disabled?
- Do generated/default covers and built-in fonts preserve the product shape?
- Do host-only diagnostics stay outside app semantics?
- Can a frame render into an externally owned memory surface without SDL present?

If the probe fails, debug the provider/resource boundary first.
