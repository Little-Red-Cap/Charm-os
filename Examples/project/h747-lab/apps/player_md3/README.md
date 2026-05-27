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
- consume typed `input.service` snapshots and translate generic pointer /
  encoder facts into `PlayerInputEvent`;
- render the real MD3 scene into `PlayerRasterDisplaySink`;
- keep host-only storage, cover decode, and file fonts disabled.
- keep FreeType, VFS file-font providers, Vivid font packages, and debug-only
  UI demo modules out of the default firmware module/link set.
- keep Now Playing layered transition capture/compose modules out of the
  default firmware; H747 uses `StaticCut` as both runtime policy and compile
  boundary.

This target is not a replacement design and must not return to a hand-drawn
mock/probe path. The source of truth for visual behavior remains
`Examples/project/player/app-vivid-MaterialDesign3`; H747 only supplies board
providers and fallback seams until storage, cover, and font providers are
connected.

The firmware uses `CHARM_VIVID_FEATURESET=PRODUCT`, not the Windows preview
`FULL` set. PRODUCT keeps the shared MD3 runtime path but admits Vivid
core/gfx/widgets only through the explicit `CHARM_VIVID_PRODUCT_*` whitelists
in `app.cmake`; host snapshot/GIF/screenshot/display-policy paths stay outside
this firmware module set.

The default font path is built-in/resource fonts only. `CHARM_PLAYER_FILE_FONTS`
is intentionally `OFF` for this firmware, so FreeType and Vivid file-font
modules are not compiled or linked unless that product capability is explicitly
enabled. `CHARM_PLAYER_DEBUG_UI` is also `OFF` by default, so
`player.ui_debug` and its table/tree demo widgets do not enter the MCU module
set.

The default transition path is `CHARM_PLAYER_LAYERED_TRANSITIONS=OFF`.
`PlayerUiTransitionMode::StaticCut` remains the runtime policy, and the build
gate also rejects `motion_*`, `page_transition`, and `snapshot` modules from the
default PRODUCT module set. Windows MD3 keeps the FULL host preview path with
layered transitions enabled.

## PRODUCT Capacity Profile

`app.cmake` owns the first conservative Player MD3 PRODUCT SoA profile:

- `CHARM_VIVID_SOA_MAX_NODES=384`
- `CHARM_VIVID_SOA_TEXT_ARENA_BYTES=24576`
- layer cache evidence remains `1 x 720 x 1280`
- enabled payload caps are explicit for label/button/image/list-view,
  segmented-control, slider, switch, progress, scrollbar, scroll-container,
  text-list, and spinner-backed widgets
- disabled payload kinds default to `0`; default firmware keeps table/tree
  payload caps at `0` together with the debug UI gate

These are H747 product-firmware profile values, not Windows `FULL` preview
defaults. The generated evidence is
`cmake-build-h747-lab-debug/generated/vivid/soa_pool_caps.cppm`; configure-time
gates fail if the artifact stops matching the declared profile.

## PRODUCT Module Evidence

`app.cmake` owns the first conservative Vivid PRODUCT module profile:

- `CHARM_VIVID_PRODUCT_CORE_MODULES`
- `CHARM_VIVID_PRODUCT_GFX_MODULES`
- `CHARM_VIVID_PRODUCT_WIDGETS`

The first pass is intentionally not a visual or behavior cut. It keeps the
current shared MD3 firmware module set explicit and auditable, so future
capacity/performance passes can remove modules with evidence instead of relying
on a broad `core/*.cppm` / `gfx/*.cppm` glob. The generated module evidence is
`cmake-build-h747-lab-debug/generated/vivid/player_md3_product_modules.txt`;
configure-time gates reject undeclared Vivid core/gfx modules, `snapshot`,
`display_policy`, StaticCut-disabled layered transition modules, file-font
modules, and debug-only table/tree UI paths in the default firmware.

## Memory Evidence

The default build emits post-link memory ownership evidence to
`cmake-build-h747-lab-debug/generated/memory/player_md3_memory_evidence.txt`.
This file records region usage, section placement, top RAM symbols, and key
Player/Vivid/console symbols. The structural gate is enabled by default and
fails the build if DMA buffers return to RAM_D1 or core Player/Vivid runtime
objects are placed in SDRAM.
The `[player_runtime_profile]` section records the current product ownership
baseline for `PlayerController`, `Theme`, `StyleSheet`, `ImageRegistry`, FatFs
static state, and style slots. Missing controller/theme/style evidence is a
build failure; size thresholds are recorded but not enforced yet.
The `[player_controller_profile]` and `[style_sheet_profile]` sections expose
the fixed-capacity breakdown used by the current product profile. These values
are emitted as link-time absolute symbols, so the evidence is machine-readable
without adding runtime RAM/Flash storage.

Current PRODUCT baseline after Player icon pixels moved to the SDRAM runtime
icon arena and Library row/cover-path caches were made on-demand:

- `RAM_D1 110824 B`
- `RAM_D2 4 KB`
- `FLASH 730608 B`

`RAM_D1.record_threshold_bytes=110824` is currently evidence-only; structural
ownership errors still fail the build, while RAM size movement is recorded for
review.

Previous baseline before Library row/cover-path caches were made on-demand:

- `RAM_D1 272256 B`
- `RAM_D2 4 KB`
- `FLASH 892840 B`

Previous baseline before Player icon pixels were moved out of RAM_D1:

- `RAM_D1 428936 B`
- `RAM_D2 4 KB`
- `FLASH 893464 B`

The `RAM_D2 4 KB` usage is the console UART RX DMA ring buffer in
`.dma_buffer`; it is intentionally outside the Player-critical RAM_D1 budget.
Player icon pixels now use an explicit SDRAM runtime icon arena. The post-link
gate rejects `player::ui::icon_*::buf` symbols in RAM_D1; the expected arena
size is `156672 B`.

Current top RAM_D1 ownership after Library cache on-demand placement:

- `Theme 55556 B`
- `StyleSheet 18880 B`
- `PlayerController 15488 B`
- `FatFs static state 6152 B`
- `ImageRegistry 2936 B`

Current `PlayerController` capacity breakdown:

- `track_capacity=256`
- `row_scratch_bytes=364`
- `cover_path_scratch_bytes=264`
- `list_cover_cache_bytes=6576`
- `text_state_bytes=1444`
- `cover_path_state_bytes=1584`
- `ui_handles_bytes=1228`

Current `StyleSheet` PRODUCT profile:

- `widget_kind_count=28`
- `total_style_slots=66`
- `total_variant_slots=28`
- `style_table_static_bytes=6164`
- `rule_storage_bytes=7808`
- `stylesheet_size_bytes=18880`

Before visual recovery, every visual change must keep the Windows UI CI green
and refresh this H747 memory evidence so RAM/Flash movement remains auditable.

Previous baseline after the StaticCut compile-time gate before the DMA buffer
placement fix:

- `RAM_D1 433088 B`
- `FLASH 893328 B`

Previous baseline before the StaticCut compile-time gate:

- `RAM_D1 429264 B`
- `FLASH 902784 B`

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
