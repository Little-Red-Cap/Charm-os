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

## Host-Only Dependencies

- SDL3 windowing, event pump, renderer, and screenshot flow live in `Examples/project/player/win`.
- Host feature defaults are composed by `PLAYER_HOST_PROFILE`; explicit `CHARM_PLAYER_HOST_*` cache values remain valid overrides for local experiments.
- Windows preview resource defaults are composed by `product_player_host_resources.cmake`; common code reads them through `player.product_config`.
- Win32/GDI fallback font caching is gated by `CHARM_PLAYER_HOST_UI && CHARM_PLAYER_PC_FONT_CACHE && _WIN32`.
- Host VHD storage defaults are gated by `CHARM_PLAYER_HOST_STORAGE`.
- Product resource defaults live in `player.product_config`; host entry points should select or override them instead of hardcoding resource paths.
- `AppConfig` stores resource paths in fixed-capacity slots so the app-level config boundary does not require dynamic strings.
- Embedded and file-backed cover decoding is gated by `CHARM_PLAYER_HOST_COVER_DECODE`; portable targets can keep using generated/default covers or later provide pre-decoded resource images.
- The Windows host shell prints `[player.features]` at startup so a preview build and a portability-probe build can be distinguished from logs. A probe with `host_cover_decode=0` is expected to skip real cover decoding.
- `player.cover` exposes a small `CoverProviderFn` slot. The default provider is the host decoder when `CHARM_PLAYER_HOST_COVER_DECODE=1`; portable targets can install a pre-decoded/resource-backed provider without changing page controllers.
- When no cover can be decoded, Now Playing and the mini bar use generated default cover art instead of leaving image slots empty.
- Cover-theme sampling is capped at a fixed 128x128 working set before palette extraction, so Player-side theme sampling has an explicit memory budget.
- FreeType/VFS file font binding is gated by `CHARM_PLAYER_HOST_FILE_FONTS`; portable targets fall back to built-in fonts until a board resource-font contract exists.
- Playback and filesystem diagnostics are gated by explicit Player feature macros, not `_WIN32`.
- FreeType file-backed font loading is a host/product resource path until Vivid has a board resource contract.
- Calendar/week stamping is routed through `player.time_utils` so page controllers do not carry platform time branches.

## Portability Blockers To Retire

- Dynamic `std::vector` / `std::string` state in the Material Design 3 controller and cover pipeline.
- Cover extraction and theme sampling allocate temporary buffers for embedded images.
- Library popup and preview text helpers still build temporary `std::string` values.
- Page screenshots and UI CI helpers belong to the host shell, not to the portable Player core.

## Next Cleanup Order

1. Move host-only diagnostics and screenshot helpers behind explicit host macros.
2. Replace controller-owned dynamic track/list caches with fixed-capacity storage.
3. Add a portable pre-decoded image/resource cover provider beside the existing host decoder path.
4. Replace host C library time fallback with a board clock/RTC adapter when the portable Player target appears.
