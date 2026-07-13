# Player Port Runtime Smoke

Host-only semantic smoke for the Player-owned port lifecycle. It uses an
in-memory raster surface, a fake monotonic clock, and queued raw input. It does
not include SDL, Win32, H747, QEMU, storage, or audio.

The smoke verifies:

- `bootstrap -> input -> update -> render -> shutdown` ordering;
- bounded raw-input draining;
- run-loop supplied timestamps and frame delta;
- raster presentation through the Player consumer contract;
- idempotent shutdown and rejection of frames after shutdown.

推荐复用 Player 唯一构建目录：

```powershell
cmake --preset player-md3-canonical-debug
cmake --build --preset build-player-md3-canonical-debug -- -j1
ctest --preset test-player-md3-canonical-debug
```

以上命令从 `Examples/project/player` 执行，并固定使用仓库根 `cmake-build-player`。本目录的
独立 CMake 入口只供隔离诊断，不作为日常入口，避免产生平行构建目录。
