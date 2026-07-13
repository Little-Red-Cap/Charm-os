# SDL3 WAV Demo

## 文档状态

- `status`: `supporting`
- `scope`: `Charm-audio` 的 SDL3 playback 与 pull-simulator fixture
- `source`: [`CMakeLists.txt`](CMakeLists.txt)、[`main.cpp`](main.cpp)

该示例通过共享 `Charm-audio` module graph 构建，不维护私有 module source list。SDL playback 需要
audio device；`--pull-sim` 使用相同 fill semantics，不输出真实音频。

## Build 与运行

手工构建与 smoke 统一复用仓库根 `cmake-build-audio-sdl3-smoke`：

```powershell
$build = 'cmake-build-audio-sdl3-smoke'
cmake -S Examples/audio/sdl3_wav_demo -B $build -G Ninja
cmake --build $build -- -j1

& "$build/sdl3-wav-demo.exe" Examples/audio/sdl3_wav_demo/sample.flac
& "$build/sdl3-wav-demo.exe" --pull-sim --tone=440 --seconds=10
```

Pull jitter 由 `--pull-jitter-ms`、`--pull-jitter-seed` 和
`--pull-jitter-pattern=uniform|burst` 控制；参数定义以 `main.cpp` 为准。

## Smoke

```powershell
./scripts/audio_sdl3_wav_demo_smoke.ps1
./scripts/ci_audio_sdl3_wav_demo_smoke.ps1
```

两个入口默认复用同一 build directory。只有需要丢弃缓存时显式传 `-Clean`；summary、jobs 与自定义
build path 通过脚本参数设置。

Smoke 使用 dummy SDL driver 验证 tone 与 sample fixture，并检查 target boundary。它证明 Host/pull
语义和 `Charm-audio` 接线，不证明真实 audio device、DMA/I2S 或板级时序。
