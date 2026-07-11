# Player Controller Playback Smoke

Host-only smoke for the Player controller playback bridge.

This is not a backend contract or public API. It verifies one product-level seam:
`PlayerController -> PlaybackSession -> PlaybackEngine -> AudioPlayer -> VFS WAV source`
and the return path from `AudioPlayer` state back into `PlayerController::tick_player()`.

The smoke uses a local RamFs mount and a pumped null audio sink, so it does not require
SDL, host audio hardware, H747, or a VHD image. The pumped sink consumes the player
FIFO through the normal fill callback, which lets the smoke prove natural end-of-track
advance without injecting controller state by hand.

Build and run:

```powershell
cmake -S Examples/system/player_controller_playback_smoke -B Examples/system/player_controller_playback_smoke/cmake-build-player-controller-playback-smoke -G Ninja
cmake --build Examples/system/player_controller_playback_smoke/cmake-build-player-controller-playback-smoke
ctest --test-dir Examples/system/player_controller_playback_smoke/cmake-build-player-controller-playback-smoke --output-on-failure
```
