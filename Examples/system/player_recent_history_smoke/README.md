# Player Recent History Smoke

Host-only semantic smoke for Player recently played history.

This verifies the app-local product behavior behind the Home `Recently Played`
card:

- a real playback path can be upserted into recent history;
- replaying the same path updates timestamp and increments play count;
- newest entries sort first and old entries are evicted at fixed capacity;
- the text file format parses and serializes without involving VFS.

This smoke is not a public storage contract, backend API, or resource model.
It is a narrow Player behavior check for the current complete-player work.

Build and run:

```powershell
cmake -S Examples/system/player_recent_history_smoke -B Examples/system/player_recent_history_smoke/cmake-build-player-recent-history-smoke -G Ninja
cmake --build Examples/system/player_recent_history_smoke/cmake-build-player-recent-history-smoke
ctest --test-dir Examples/system/player_recent_history_smoke/cmake-build-player-recent-history-smoke --output-on-failure
```
