# Charm Host SDL3

This directory implements the SDL3 Host provider. It is a backend implementation,
not a source of application semantics.

## Consumption

After the Charm runtime target exists, add this directory and link the provider:

```cmake
add_subdirectory("${CHARM_ROOT}/Backends/host/sdl3" charm-host-sdl3)
target_link_libraries(my_host_app PRIVATE Charm::host-sdl3)
```

Import `charm.backend.host.sdl3`, create one `Host`, call `open()`, bind
`host.clock()` to the existing run loop, and invoke `pump_events()` from its IO
phase. The render phase submits a neutral `raster::SurfaceView` through
`present()`.

The first frame establishes the logical raster extent used to map later pointer
coordinates from the Host window. `pump_events()` is the sole SDL queue owner;
consumers must not run another `SDL_PollEvent()` loop beside it.

Host v1 permits one active `Host` instance and one SDL window per process. A
second or external SDL window is rejected instead of competing for SDL's global
event queue. If an external window appears later, the pump returns
`foreign_window` before polling, so it does not consume that window's events.
The pump handles both global quit and window-close requests and still drains
quit events when no raw input sink is bound.

## Boundary

- SDL and native window types stay inside this backend.
- Input leaves the backend as `input::RawInputEvent`.
- Display input is the `Backends/contract/raster_display.hpp` candidate.
- Player commands, frame pacing, audio, storage, fonts, screenshots, and product
  configuration stay outside this backend.
