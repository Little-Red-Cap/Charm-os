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
- translate GT970 touch and dual encoder samples into `PlayerInputEvent`;
- render the real MD3 scene into `PlayerRasterDisplaySink`;
- keep host-only storage, cover decode, and file fonts disabled.

This target is not a replacement design and must not return to a hand-drawn
mock/probe path. The source of truth for visual behavior remains
`Examples/project/player/app-vivid-MaterialDesign3`; H747 only supplies board
providers and fallback seams until storage, cover, and font providers are
connected.

## Board Smoke Evidence

Serial status lines are the acceptance evidence for this bridge. A healthy
loop line should contain:

- `real_md3=1 mock=0`
- `smoke=1/11111`
- `delta=<frames>/<presents>` with non-zero values on loop status lines
- `frames=<n>` and `present=<n>` increasing across loop status lines
- `content=<bg>:<non_bg>@<min>-<max>` with non-zero content pixels
- `exec_fail=0`, `co=0`, and `to=0`

`smoke=1/11111` means boot, render, present, content, and scene execution are
all green for the current sample window. This is intentionally app-local
evidence: it proves this target is running the real shared MD3 Player runtime,
not the old hand-drawn probe path.

The full board collection checklist and failure triage live in
`../../docs/h747_lab_player_md3_smoke.md`.
