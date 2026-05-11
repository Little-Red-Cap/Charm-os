# SDL3 WAV Demo

This example links through the shared `Charm-os` / `Charm-media` module graph.
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

## Notes

- The demo does not maintain its own module source list; CMake routes through
  the shared Charm targets instead.
- `--pull-sim` forces tone mode and runs `AudioPullSimulator` to exercise
  pull timing, underrun, and water-level behavior without hardware.
- `--pull-jitter-ms` injects random callback jitter (0..N ms) for stress testing.
- `--pull-jitter-seed` makes the jitter sequence repeatable.
- `--pull-jitter-pattern` selects `uniform` or `burst`.
