# H747 Lab Player

This app is the first Player-on-H747 portability shell. It is intentionally
smaller than the Windows Player demo: the first goal is a clean app/profile
boundary, not feature parity.

## Boundary

- `player_model.hpp` owns the MCU-friendly view model and runtime tick state.
- `player_input.hpp` normalizes app-local text commands into `InputFrame`
  events before they reach `PlayerRuntime`.
- `player_domain.hpp` and `player_domain.inl` render from `PlayerViewModel`
  into a `RasterDisplayInputWorld`.
- `player.cpp` adapts the H747 board world, performs low-frequency resource
  probes, maps service snapshots into `PlayerBoardSnapshot`, routes UART
  commands through the same input path, and prints display evidence.
- `host/player_host.cpp` runs the same domain code against a mock world and
  writes `player_host.ppm` under `cmake-build-*`. Its `--ci` mode also asserts
  `PlayerBoardSnapshot -> PlayerViewModel` state mapping.

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

The domain path does not depend on the old Windows Player project, file fonts,
cover decoding, dynamic allocation, exceptions, RTTI, or STM32 HAL handles.
Future storage/audio/Vivid work should feed `PlayerViewModel` first, then
promote stable pieces into shared Charm/Vivid layers only after they survive
both host and H747 builds.
