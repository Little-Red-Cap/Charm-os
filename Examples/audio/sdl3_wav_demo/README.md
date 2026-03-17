# SDL3 WAV Demo

This example supports a pull-simulator mode that does not require SDL audio.

## Build

```bash
cmake -S Examples/audio/sdl3_wav_demo -B Examples/audio/sdl3_wav_demo/build -G Ninja
cmake --build Examples/audio/sdl3_wav_demo/build
```

## Run

```bash
# Normal playback (SDL audio)
Examples/audio/sdl3_wav_demo/build/sdl3-wav-demo sample.flac
```

```bash
# Pull simulator (no SDL audio output)
Examples/audio/sdl3_wav_demo/build/sdl3-wav-demo --pull-sim --pull-jitter-ms=5 --pull-jitter-seed=1 --pull-jitter-pattern=burst --tone=440 --seconds=10
```

## Notes

- `--pull-sim` forces tone mode and runs `AudioPullSimulator` to exercise
  pull timing, underrun, and water-level behavior without hardware.
- `--pull-jitter-ms` injects random callback jitter (0..N ms) for stress testing.
- `--pull-jitter-seed` makes the jitter sequence repeatable.
- `--pull-jitter-pattern` selects `uniform` or `burst`.
