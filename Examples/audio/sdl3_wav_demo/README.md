# SDL3 WAV Demo

This example links through the shared `Charm-audio` module graph.
It supports both SDL3 audio playback and a pull-simulator mode that does not
require an audio device.

## Build

```powershell
cmake -S Examples/audio/sdl3_wav_demo -B cmake-build-sdl3_wav_demo -G Ninja
cmake --build cmake-build-sdl3_wav_demo
```

## Run

```powershell
# Normal playback (SDL audio)
.\cmake-build-sdl3_wav_demo\sdl3-wav-demo.exe .\Examples\audio\sdl3_wav_demo\sample.flac
```

```powershell
# Pull simulator (no SDL audio output).
.\cmake-build-sdl3_wav_demo\sdl3-wav-demo.exe --pull-sim --pull-jitter-ms=5 --pull-jitter-seed=1 --pull-jitter-pattern=burst --tone=440 --seconds=10
```

## Smoke

From the repository root:

```powershell
.\scripts\audio_sdl3_wav_demo_smoke.ps1 -Clean -SummaryPath out\audio-sdl3-smoke\summary.json
```

The smoke script configures and builds `cmake-build-audio-sdl3-smoke`, then
runs both `--tone --seconds 1` and `sample.flac --seconds 1` with
`SDL_AUDIODRIVER=dummy`. Its target boundary check requires the Ninja target
list to contain `Charm-audio` and `sdl3-wav-demo`, and to exclude
`Charm-runtime` and `Charm-media`.

For CI or local gates, use the wrapper with the same default build directory:

```powershell
.\scripts\ci_audio_sdl3_wav_demo_smoke.ps1 -Clean
```

## Notes

- The demo does not maintain its own module source list; CMake routes through
  the shared `Charm-audio` target instead.
- `--pull-sim` forces tone mode and runs `AudioPullSimulator` to exercise
  pull timing, underrun, and water-level behavior without hardware.
- `--pull-jitter-ms` injects random callback jitter (0..N ms) for stress testing.
- `--pull-jitter-seed` makes the jitter sequence repeatable.
- `--pull-jitter-pattern` selects `uniform` or `burst`.
