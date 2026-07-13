# Player MD3 Runtime Smoke

Canonical, host-only Player MD3 runtime evidence. The smoke builds the real MD3
controller, scene, Player runtime, and Player Port endpoint without SDL, Win32
APIs, H747, QEMU, audio hardware, or platform storage.

It verifies that an empty-library Player still bootstraps, accepts raw input,
updates, renders into a borrowed raster surface, and shuts down through the
Player Port lifecycle. Home, Library, and Now Playing are rendered in RGB565,
RGB888, and ARGB8888, giving Player-owned visual regression evidence without
requiring a Host screenshot API.

```powershell
cmake --preset player-md3-canonical-debug
cmake --build --preset build-player-md3-canonical-debug -- -j1
ctest --preset test-player-md3-canonical-debug
```

以上命令从 `Examples/project/player` 执行，并固定复用仓库根 `cmake-build-player`；不要为该 smoke
创建独立构建目录。

维护者只有在确认 UI 改动符合预期时，才可使用 `<format> --record` 打印新的三页面 digest，随后
显式更新测试内基线。`--record` 不属于 CTest，不能绕过 canonical 回归。
