# Player USB Audio

This project is the current working home for the STM32G431 USB audio bring-up.

Current build policy:

- keep this project self-contained, like the external `Charm-dap` project
- keep CubeMX-generated assets under `g431/`
- keep product entry code under `app/`
- treat the repository root as a shared module source, not as the primary project orchestrator

Current scope:

- USB CDC bring-up on the Charm USB stack
- ST USB Audio class bring-up for real USB speaker enumeration and streaming
- USB OUT packet data bridged into the local I2S DMA playback buffer
- keep display, encoder, and higher-level UI work out of scope for now

Current presets:

- `cmake --preset g431-debug`
- `cmake --build --preset g431-debug`
- `cmake --preset g431-uac-st-debug`
- `cmake --build --preset g431-uac-st-debug`

Notes:

- the ST Audio class is vendored locally under `g431/USB_DEVICE/Class/AUDIO/`
- this local copy keeps USB Audio packet sizing and descriptor details under project control
- for `44.1 kHz / 16-bit / stereo`, the class uses a max isochronous packet size so full-speed `176/180` byte frame variation can be accepted correctly
- the internal ST audio buffer depth is reduced to fit STM32G431 RAM more comfortably during bring-up

Build:

- `cmake --preset g431-debug`
- `cmake --build --preset g431-debug`
- if STM32Cube G4 is not installed in the default local path, configure with
  `-DPLAYER_USB_AUDIO_STM32CUBE_G4_ROOT=<path>` or export `STM32CUBE_G4_ROOT`
