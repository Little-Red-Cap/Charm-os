# Player Playback Engine Smoke

Host-only semantic smoke for the shared Player playback core.

This smoke verifies the current `player::PlaybackEngine` + `audio::AudioPlayer`
control path and the lightweight `player::PlaybackSession` track-control layer
without H747 BSP, SDL, Vivid UI, Store/FAT, or backend contracts.
It is a transition test for making Player a complete playback system, not a
public API or capability contract.

Covered behavior:

- no-player, no-track, and not-ready-track failures
- start on a real WAV fixture
- duration extraction from `AudioPlayer`
- pause, resume, stop, toggle
- seek while paused
- `AudioPlayer::tick()` processing after queued commands
- session-level track collection
- next/previous track switching while preserving playback state
- filtered playback queue projection
- paused-state track selection
- play mode state cycling
- sequential/repeat-one/shuffle auto-advance behavior

Build and run:

```powershell
cmake -S Examples/system/player_playback_engine_smoke -B Examples/system/player_playback_engine_smoke/cmake-build-player-playback-engine-smoke -G Ninja
cmake --build Examples/system/player_playback_engine_smoke/cmake-build-player-playback-engine-smoke
ctest --test-dir Examples/system/player_playback_engine_smoke/cmake-build-player-playback-engine-smoke --output-on-failure
```
