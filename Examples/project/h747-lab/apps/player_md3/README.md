# H747 Lab Player MD3

This app is the first real MD3 Player-on-H747 bridge. It instantiates the
shared `Examples/project/player/app-vivid-MaterialDesign3` controller/pages
through `PlayerRuntime<PlayerController, PlayerPage>` and presents the result
through the H747 raster framebuffer service.

The app-local bridge owns only board/runtime assembly:

- initialize after `power`, `memory`, and `display_raster` services are ready;
- carve an explicit SDRAM1 render/runtime pool after the raster framebuffer
  pool;
- bind `PlayerPlatform` to that external render surface;
- drive the shared `PlayerRuntime` through `PlayerRuntimeShell`;
- render the real MD3 scene into `PlayerRasterDisplaySink`;
- keep host-only storage, cover decode, and file fonts disabled.

This target is not a replacement design and must not return to a hand-drawn
mock/probe path. The source of truth for visual behavior remains
`Examples/project/player/app-vivid-MaterialDesign3`; H747 only supplies board
providers and fallback seams until storage, cover, font, and input providers are
connected.
