# H747 Lab Player

This app is the first Player-on-H747 portability shell. It is intentionally
smaller than the Windows Player demo: the first goal is a clean app/profile
boundary, not feature parity.

## Boundary

- `player_model.hpp` owns the MCU-friendly view model, command model, and
  runtime tick state.
- `player_domain.hpp` and `player_domain.inl` render from `PlayerViewModel`
  into a `RasterDisplayWorld`.
- `player.cpp` adapts the H747 board world, performs low-frequency resource
  probes, maps service snapshots into `PlayerBoardSnapshot`, and prints
  display evidence.
- `host/player_host.cpp` runs the same domain code against a mock world and
  writes `player_host.ppm` under `cmake-build-*`. Its `--ci` mode also asserts
  `PlayerBoardSnapshot -> PlayerViewModel` state mapping.

The domain path does not depend on the old Windows Player project, file fonts,
cover decoding, dynamic allocation, exceptions, RTTI, or STM32 HAL handles.
Future storage/audio/Vivid work should feed `PlayerViewModel` first, then
promote stable pieces into shared Charm/Vivid layers only after they survive
both host and H747 builds.
