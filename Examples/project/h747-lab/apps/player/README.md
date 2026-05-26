# H747 Lab Player

This app is the first Player-on-H747 portability shell. It is intentionally
smaller than the Windows MD3 Player: the current goal is to land the platform
boundary first, then move the real product UI across that boundary.

The source of truth for the polished Player product experience remains
`Examples/project/player/app-vivid-MaterialDesign3`. This app must not be used
as a replacement design or a "looks-like" port.

## Boundary

- `player_model.hpp` owns the MCU-friendly view model and runtime tick state.
- `player_input.hpp` normalizes app-local text commands into `InputFrame`
  events before they reach `PlayerRuntime`.
- `player_domain.hpp` and `player_domain.inl` render from `PlayerViewModel`
  into a `RasterDisplayInputWorld`.
- `player.cpp` adapts the H747 board world, performs low-frequency resource
  probes, maps service snapshots into `PlayerBoardSnapshot`, routes UART
  commands through the same input path, and prints display evidence.
- `player_display_hal.hpp` adapts the H747 raster framebuffer into the shared
  `player.display` `PlayerDisplaySurface`/`PlayerDisplaySink` contract.
- `host/player_host.cpp` runs the same domain code against a mock world and
  writes `player_host.ppm` under `cmake-build-*`. Its `--ci` mode also asserts
  `PlayerBoardSnapshot -> PlayerViewModel` state mapping.

## Current Migration Status

The current H747 build proves that the board can expose a Player display
surface/sink without SDL, Win32, file fonts, or host cover decode. It does not
yet prove that the full MD3 Player controller and pages run on H747.

The next product migration step is to keep extracting the existing MD3 Player
state/render/input contracts until the H747 app can consume the same product UI
path with board-local display, input, storage, font, and cover providers.

## UART commands

The board Player shell currently accepts:

- `status`
- `toggle`
- `next`
- `prev`
- `seek+`
- `seek-`

`status` is a diagnostic fast path. The other commands are normalized into the
same `InputFrame` path used by encoder/touch sampling, so future physical input
sources should extend the input service instead of adding new PlayerRuntime
entry points.

The current shell path does not depend on SDL, file fonts, cover decoding,
dynamic allocation, exceptions, RTTI, or STM32 HAL handles. Future
storage/audio/Vivid work should keep platform capabilities behind explicit
providers, then promote stable pieces into shared Charm/Vivid layers only after
they survive both host and H747 builds.
