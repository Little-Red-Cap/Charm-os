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
| Cover | `player.cover_resource` defines the resource contract, `player.runtime` installs a `PlayerCoverResourceProviderBinding`, and `player.cover` still owns host decode fallback plus generated/default cover art. | `CHARM_PLAYER_HOST_COVER_DECODE`; profile log prints `host_cover_decode`. | Host decode buffers, embedded image extraction, palette/theme sampling work buffers. | Add real board/resource provider implementations behind the binding contract. |
| Theme | Player-owned cover sampling feeds dynamic surface/text roles; Vivid owns reusable role application candidates. | Indirectly tied to cover decode availability. | Fixed-budget 128x128 sampling workspace, palette candidates, controller theme state. | Keep sampling in Player; consider moving role application rules into Vivid after more app pressure. |
| Audio spectrum | `audio.spectrum` owns analyzer state and the host FFT backend; `audio.player` only calls `enable/read/push/reset`. | `CHARM_AUDIO_ENABLE_SPECTRUM` and `CHARM_AUDIO_SPECTRUM_USE_HOST_FFT`; defaults follow `CHARM_TARGET_HAS_CXX_MATH`. | Host FFT uses fixed arrays plus `<complex>/<cmath>`; no-math targets compile the disabled backend with no analyzer buffers. | Add a CMSIS-DSP or fixed-point backend behind `SpectrumAnalyzer` instead of reintroducing FFT details into `audio.player`. |
| Font | `FontResourceConfig` carries explicit font source kind plus sizes; host preview binds file fonts only when enabled; board ports can pass package metadata through `PlayerBoardFontPackageView`. | `CHARM_PLAYER_HOST_FILE_FONTS`, `CHARM_PLAYER_PC_FONT_CACHE`; profile log prints `host_file_fonts`. | FreeType/VFS font buffers, Win32/GDI glyph cache vectors, preview argv strings. | Define the concrete board font package binary/layout behind `FontResourceKind::Package`. |
| Runtime | `player.runtime` owns product lifecycle; `player.board_runtime` owns the board construction helper; `player.runtime_probe` owns reusable memory-surface proof. `player.runtime.hqzy_cm7.player_ui_port_bridge` is the H747-local facts bridge for framebuffer, dirty region, touch, and clock seams. | Host shell selects preview/probe config and still owns SDL, argv, screenshots, and UI-CI entry. | Runtime owns `App` storage in v0; controller/platform/runtime storage is provided by the adapter. The H747 bridge uses fixed records and function pointers only. | Add a non-Windows board shell that calls `make_player_board_runtime()` with board-owned resources. |
| Display/Input | `PlayerDisplaySurface`, `PlayerDisplaySink`, board display callbacks, `PlayerBoardPortConfig`, and `render_player_frame()` separate Player rendering from SDL; `PlayerInputEvent` plus touch batching separates Player input from SDL/window/touch samples. `player.runtime.hqzy_cm7.player_ui_port_bridge` is the H747-local board-port seed for those facts and now has local `probe_port()` / `present_dirty()` helpers. | Windows preview uses SDL display and input adapters; memory, board-sink, board-port, and board-touch probes prove real Player UI external-framebuffer/input output. | Preview SDL texture/window state; UI-CI external buffer is fixed-size static storage; input HAL structs are fixed-size. H747 port probing adds no dynamic allocation. | Add a non-Windows board shell that wires SDRAM/LTDC and touch drivers through `PlayerBoardPortConfig`. |
| Time | `player.time_utils` keeps date/week formatting out of page code. | No dedicated board clock gate yet; Windows host binds the runtime time source. | Mostly fixed strings today; playback uses runtime clock timestamps. | Add a time/clock provider adapter when a portable Player target appears. |
| Storage | `player.storage` and `PlayerApp` isolate scan/mount flow; product config carries host VHD default. | `CHARM_PLAYER_HOST_STORAGE`; profile/resource records select VHD defaults. | Track lists, scan queues, stats history, path buffers, filesystem traversal state. | Introduce board storage capability/provider instead of page-level storage assumptions. |
| Diagnostics | Host shell owns screenshot, font probe, UI-CI, and preview logging includes. | `CHARM_PLAYER_PLAYBACK_LOG`, `CHARM_PLAYER_FS_LOG`, host shell only screenshot/UI-CI paths. | Screenshot paths, UI-CI probe state, font probe output strings, playback log formatting. | Keep host-only; split `main.ui_ci.inc` by evidence group later. |
| UI-CI | Windows host runner proves preview and portability-probe contracts without becoming app semantics. | Host executable path plus `--ui-ci`; probe uses disabled cover decode/file fonts. | Case-local probe state and temporary strings. | Group by frame budget, layer, navigation, transition, and list evidence. |

## Cover

Current state:

- `player.cover_resource` now owns the app-common resource contract: `CoverResourceRequest`, `CoverResourceView`, and `PlayerCoverResourceProviderBinding`.
- `player.runtime` installs the configured cover resource binding before UI/bootstrap work and restores the previous binding on shutdown, so provider scope is runtime-local instead of page-local.
- `player.board_port` carries the same binding so a future board shell can supply pre-decoded cover resources without teaching page/controller code about files or decoders.
- `player.cover` still checks the active resource binding before host decode, so board/resource builds can return a pre-decoded `CoverResourceView`.
- The Windows preview can install the host decoder when `CHARM_PLAYER_HOST_COVER_DECODE=1`.
- `portability_probe` disables host decode, so generated/default cover art must keep Now Playing and the mini bar complete.
- The UI path should not assume that embedded album art can always be decoded at runtime.

Dynamic allocation and portability notes:

- Host-side decode and embedded-image extraction still need temporary image buffers.
- Resource-backed covers still copy into `CoverImage::argb` in v0; fixed-capacity ownership is a later portable-profile concern.
- Cover-theme sampling uses a fixed 128x128 working set before palette extraction, which is good because the memory budget is visible.
- Some theme and transition state remains in the MD3 controller; this is product semantic state, not a host dependency by itself.

Recommended next slice:

- Add real board/resource implementations for the runtime/board-installed cover resource binding.
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

## Audio Spectrum

Current state:

- `audio.spectrum` owns the spectrum analyzer state, backend selection, FFT window, output bins, and ready/enabled flags.
- `audio.player` no longer imports `alg_fft`, `<complex>`, or `<cmath>` for spectrum rendering; it only forwards captured PCM to `SpectrumAnalyzer`.
- The current implemented backend is `host_fft`, selected by `CHARM_AUDIO_SPECTRUM_USE_HOST_FFT`.
- No-math targets compile the same `audio.spectrum` module with `SpectrumBackendKind::none`, so spectrum enablement becomes a no-op and playback remains independent of FFT availability.

Dynamic allocation and portability notes:

- The host FFT backend uses fixed-size arrays only, but it still depends on hosted math/library features.
- A future STM32H7 backend should use CMSIS-DSP or a fixed-point analyzer behind `SpectrumAnalyzer`; `audio.player` should not learn CMSIS headers or DSP workspace details.
- Board code can leave spectrum disabled without changing playback semantics.

Recommended next slice:

- Add a concrete `cmsis_dsp` backend only when the board build has CMSIS-DSP linked and its scratch/storage budget is explicit.
- Keep backend choice in media/profile config, not in Player page/controller code.

## Font

Current state:

- `player.product_config` owns default font path and size constants.
- `AppConfig` carries a `FontResourceConfig` record with fixed-capacity paths, font sizes, and explicit `FontResourceKind`.
- `PlayerBoardFontPackageView` gives board code a named place to pass package bytes/key/size metadata without pretending file paths exist.
- Host file fonts are enabled only through `CHARM_PLAYER_HOST_FILE_FONTS`.
- Win32/GDI fallback glyph caching is guarded by `CHARM_PLAYER_HOST_UI && CHARM_PLAYER_PC_FONT_CACHE && _WIN32`.
- `portability_probe --font-disable-system-fallback` is the current proof that built-in fonts can keep the UI readable.

Dynamic allocation and portability notes:

- `player.font_cache` uses `std::vector` for host glyph and bitmap caches. This is host preview implementation state.
- FreeType/VFS font loading remains file-backed and should not be treated as a board resource contract.
- Page code still asks for rich font variants; that is visual product intent, not necessarily a host dependency.

Recommended next slice:

- Define the concrete board font package binary/layout behind `FontResourceKind::Package`.
- Keep file paths as host/profile resource defaults.
- Avoid spreading file font assumptions into page builders or controllers.

## Display and Input

Current state:

- `player.display` defines `PlayerDisplaySurface`, `PlayerDisplaySink`, pixel format, dirty region, and ownership metadata.
- `PlayerPlatform` binds a supplied surface and renders Vivid Scene output into it through `RuntimeCanvas`.
- `render_player_frame()` is the shared frame lifecycle for host preview, UI-CI, and future board sinks.
- The Windows preview owns its default display buffer in the host shell, then presents it through an SDL display sink.
- `MemoryDisplaySink` is the current SDRAM-style seam: `player.runtime_probe` renders the real Player UI into an external buffer and verifies present metadata.
- `make_board_display_sink()` adds the board adapter seam for cache clean, dirty flush, and present/flip callbacks while preserving the same `PlayerDisplaySink` contract.
- `player.board_port` groups framebuffer, display callbacks, touch source, font package metadata, and audio defaults into `PlayerBoardPortConfig`.
- `make_player_board_port_bindings()` returns the `PlayerDisplaySurface`, `PlayerDisplaySink`, and `AppConfig` a board shell needs before constructing `PlayerPlatform` and `PlayerRuntime`.
- `player.board_runtime` adds `PlayerBoardRuntimeConfig` and `make_player_board_runtime()` so the board shell does not hand-roll `PlayerPlatform` / `PlayerRuntime` construction.
- `player.runtime.hqzy_cm7.player_ui_port_bridge` is deliberately lighter than `player.board_runtime`: it can be compiled into the current H747 USB/Ink profile, validate SDRAM framebuffer metadata, read the board clock seam, and exercise display callbacks without importing MD3/Vivid runtime modules.
- `--runtime-memory-smoke` is the Windows host adapter entry for the same probe before SDL initialization, so the display proof is not coupled to a host window.
- `player.input` defines the Player product input boundary: pointer, wheel, button, command, and a minimal `PlayerTouchSampleSource` seam.
- `read_player_touch_events()` batches board touch samples into fixed-capacity `PlayerInputEvent` output without dynamic allocation.
- SDL input event decoding lives in a host-local adapter include; it only translates SDL events to `PlayerInputEvent`.
- `player.app` is the single bridge from `PlayerInputEvent` to Vivid `RawInputEvent`, wheel dispatch, or controller command dispatch.
- Controller command handling lives behind `handle_input_command(PlayerInputCommand)`, so board keys, SDL keys, and future Win32/Linux input can share product semantics.
- UI-CI click, touch-style, and wheel cases now run through the Player input boundary instead of hand-building raw Vivid input.

Dynamic allocation and portability notes:

- The display HAL contract itself does not require dynamic allocation, exceptions, RTTI, SDL, Win32, or Linux APIs.
- Rich Vivid scene/controller state is still not MCU-strict, but the final display output can now target an externally owned framebuffer.
- SDL texture/window/event state remains host-only preview implementation state.
- The input HAL contract itself does not require dynamic allocation, exceptions, RTTI, SDL, Win32, Linux APIs, or a concrete touch driver.
- A board touch adapter only needs to sample `{down, x, y, id, ms}` and emit pointer events; cache, LTDC, DMA2D, and touch-controller handles stay in board code.

Recommended next slice:

- Add a non-Windows board shell that calls `make_player_board_runtime()` from board-owned resources.
- Then wire concrete STM32H7 SDRAM/LTDC callbacks and touch driver sampling into that shell.

## Runtime

Current state:

- `player.runtime` is the common product lifecycle shell for the Vivid Player path.
- Windows/SDL constructs `PlayerRuntime` with a host-owned `PlayerPlatform`, controller storage, clock, and `PlayerRuntimeConfig`.
- `bootstrap()` owns the product sequence: bind clock, construct `App`, initialize storage, bind player/controller/scene, build UI, apply storage view, and bootstrap the selected track.
- `dispatch_input()` is the only host loop path from `PlayerInputEvent` into `player.app` and then Vivid/controller semantics.
- `tick()` owns `App::tick()`, `controller.tick_player()`, and optional preview visual hooks without making spectrum rendering part of the board contract.
- `render()` owns the frame call into `render_player_frame()` so SDL, memory, and future board sinks share the same render choreography.
- `shutdown()` owns app/controller shutdown while SDL window/texture cleanup remains host-only.
- `player.runtime_probe` wraps one externally supplied runtime/storage/surface configuration and records bootstrap, render, present, dirty-region, nonzero-pixel, and root-binding evidence.

Dynamic allocation and portability notes:

- Runtime v0 still stores `App` in `std::optional`; this is a desktop-friendly ownership seam, not yet a fixed-allocation embedded profile.
- Controller, `PlayerPlatform`, and runtime storage are supplied from the adapter side, so a board profile can place them in static storage or a specific memory region.
- Runtime does not depend on SDL, Win32, Linux, screenshots, argv parsing, or the UI-CI runner.
- Runtime still imports the rich Player app/controller/display stack, so it is a lifecycle boundary, not an MCU-strict proof by itself.

Recommended next slice:

- Promote the no-window memory smoke shape into a non-Windows board shell using `PlayerBoardRuntimeConfig`, board-style clock, and board/provider config outside the Windows host executable.
- Keep screenshot, GIF, font probe, and preview argv helpers in the Windows host shell.
- Avoid adding virtual interfaces or heap-owning provider registries until a concrete board adapter needs them.

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
- `FontResourceConfig` keeps font source mode and file-backed resource data behind the app config boundary instead of scattering TTF fields through host bootstrap.
- `PlayerBoardPortConfig` keeps framebuffer, display callbacks, touch source, and package font metadata in one board adapter record instead of scattering them through page/controller code.
- `PlayerBoardRuntimeConfig` keeps runtime defaults near the board adapter and leaves page/controller code unaware of board assembly details.
- `CoverResourceProviderFn` gives portable builds a pre-decoded cover path before host decode.
- `PlayerDisplaySurface` lets board code provide the final framebuffer memory instead of forcing Player to own or SDL-present it.
- `render_player_frame()` keeps clear/transition/render/present choreography out of SDL-specific code.
- `MemoryDisplaySink` gives CI a SDRAM-style real-Player-UI external-buffer proof without needing board hardware.
- `PlayerInputEvent` gives SDL, Win32, Linux, and board touch drivers the same input seam.
- UI-CI now proves touch-style pointer dispatch and wheel dispatch through the Player input boundary.
- Explicit host feature gates in `player.host_features`.
- CMake host profiles that make preview/probe differences visible.
- `CoverProviderFn` and generated/default cover fallback.

## Next Slice Candidates

1. Board shell target: call `make_player_board_runtime()` outside the Windows executable; the current H747 `player_ui_port_bridge` is the local seed.
2. Font provider: define the package binary/layout and keep file fonts as a host/profile implementation.
3. Time/diagnostics provider: keep formatting and logging out of page code, but avoid over-abstracting before a board target exists.
4. Controller dynamic state slimming: replace only the dynamic state that blocks `portability_probe` or a concrete board profile.

## Probe Contract Reminder

The portability probe is not a hardware simulation. Its question is narrower and more useful:

- Does the UI still run when host cover decode is disabled?
- Does it remain readable when file fonts and system fallback are disabled?
- Do generated/default covers and built-in fonts preserve the product shape?
- Do host-only diagnostics stay outside app semantics?
- Can a frame render into an externally owned memory surface without SDL present?

If the probe fails, debug the provider/resource boundary first.
