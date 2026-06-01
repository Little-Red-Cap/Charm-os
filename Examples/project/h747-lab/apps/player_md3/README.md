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
enabled. When `CHARM_PLAYER_FILE_FONTS=ON`, the H747 runtime binds
`/font/NotoSansSC-Regular.ttf` with `/font/NotoSans-Regular.ttf` fallback and
the resource smoke expects `font=1/1/1/0`. `CHARM_PLAYER_DEBUG_UI` is also `OFF`
by default, so
`player.ui_debug` and its table/tree demo widgets do not enter the MCU module
set.

The default transition path is `CHARM_PLAYER_LAYERED_TRANSITIONS=OFF`.
`PlayerUiTransitionMode::StaticCut` remains the runtime policy, and the build
gate also rejects `motion_*`, `page_transition`, and `snapshot` modules from the
default PRODUCT module set. Windows MD3 keeps the FULL host preview path with
layered transitions enabled.

The default cover-theme path is `CHARM_PLAYER_COVER_THEME_EXTRACT=OFF`.
H747 derives the same MD3 color roles from a fixed fallback theme and does not
compile `alg_color_extract` or `material_color_utils` into the default Player
MD3 firmware. Windows MD3 keeps dynamic Material color extraction enabled for
host preview and UI CI.

Cover ownership is split at the `player.cover` seam. Shared controller state
stores only `ResolvedCover` metadata (`ImageId`, dimensions, fixed key, and
release flags). Resource cover views and host decoded pixels are registered by
`player.cover`; host decode buffers stay in the host-only cover detail path and
are not owned by `PlayerController`. The H747 default keeps host cover decode
disabled, so missing resources resolve to the same default/placeholder visible
semantics instead of pulling dynamic image buffers into firmware. If a folder
cover file is present but no board cover provider or MCU-safe decoder is linked,
resource smoke reports `cover=1/0/0x0/-95`; that is an unsupported-capability
state, not a missing-file state.

## PRODUCT Capacity Profile

`app.cmake` owns the first conservative Player MD3 PRODUCT SoA profile:

- `CHARM_VIVID_SOA_MAX_NODES=384`
- `CHARM_VIVID_SOA_TEXT_ARENA_BYTES=24576`
- `CHARM_VIVID_STYLE_CLASS_MAX=16`
- `CHARM_VIVID_STYLE_RULE_CAP=8`
- `CHARM_VIVID_STYLE_METRICS_POOL_CAP=16`
- layer cache evidence remains `1 x 720 x 1280`
- enabled payload caps are explicit for label/button/image/list-view,
  segmented-control, slider, switch, progress, scrollbar, scroll-container,
  text-list, and spinner-backed widgets
- disabled payload kinds default to `0`; default firmware keeps table/tree
  payload caps at `0` together with the debug UI gate

These are H747 product-firmware profile values, not Windows `FULL` preview
defaults. The generated evidence is
`cmake-build-h747-lab-debug/generated/vivid/soa_pool_caps.cppm` and
`cmake-build-h747-lab-debug/generated/vivid/config.generated.cppm`;
configure-time gates fail if either artifact stops matching the declared
profile.

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
modules, dynamic cover-theme extraction modules, and debug-only table/tree UI
paths in the default firmware.

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
The serial status schema also emits `style=<rules>/<rule_cap>/<metrics_used>/<metrics_cap>/<overflow>`
after the StyleSheet table is rebuilt; this is the runtime evidence for actual
rule and metrics pool usage, while the post-link evidence records capacity and
memory ownership.
Render performance evidence is record-only in this phase. Status lines append
`cmd_batch`, `exec_batch`, `exec_groups`, `exec_cmds`, and `exec_fail_detail`
tokens so DrawCmd pressure can be compared across UI changes without turning
the first baseline into a hard gate. Status lines also append
`perf_time=<available>/<frame>/<tick>/<render>/<record>/<execute>/<present>`.
Timing values are microseconds sampled from DWT `CYCCNT` when available; they
remain diagnostics only and do not participate in strict smoke gates.

Current PRODUCT baseline after the Library list cover cache was made a
compile-time product capacity and the default H747 profile set it to zero:

- `RAM_D1 46008 B`
- `RAM_D2 4 KB`
- `FLASH 712440 B`

Latest Now Playing closure slice evidence:

- `RAM_D1 46016 B`
- `RAM_D2 4 KB`
- `FLASH 714688 B`
- `PlayerController 9560 B`

Previous Now Playing seekbar slice evidence:

- `RAM_D1 46016 B`
- `RAM_D2 4 KB`
- `FLASH 715536 B`
- `PlayerController 9560 B`

Previous first Now Playing visual recovery slice evidence:

- `RAM_D1 46008 B`
- `RAM_D2 4 KB`
- `FLASH 715064 B`
- `PlayerController 9552 B`

The default H747 firmware keeps `CHARM_PLAYER_LIST_COVER_CACHE_ENTRIES=0`, so
Library rows still show the existing fallback/default cover semantics without
allocating controller cache slots. Windows MD3 keeps a fixed 12-entry cache for
host preview and UI CI. Evidence now records `list_cover_cache_capacity=0` and
`list_cover_cache_bytes=0`; `current_cover_bytes=280` remains the fixed
metadata for the active cover.

Previous baseline after `CoverImage` ownership was moved out of
`PlayerController` and controller cover state was reduced to fixed
`ResolvedCover` metadata:

- `RAM_D1 52552 B`
- `RAM_D2 4 KB`
- `FLASH 716136 B`

This slice intentionally trades the previous hidden dynamic cover fields for
fixed, auditable controller metadata. `current_cover_bytes=280` and
`list_cover_cache_bytes=6576` are the current fixed-capacity cover profile;
host decoded pixels remain inside the `player.cover` host-only detail path.

Previous baseline after dynamic cover-theme extraction and
`material_color_utils` were removed from the default H747 target:

- `RAM_D1 49552 B`
- `RAM_D2 4 KB`
- `FLASH 714096 B`

`RAM_D1.record_threshold_bytes=52552` is currently evidence-only; structural
ownership errors still fail the build, while RAM size movement is recorded for
review.

Previous baseline after Vivid style class/rule/metrics caps were moved to the
Player product profile:

- `RAM_D1 49624 B`
- `RAM_D2 4 KB`
- `FLASH 765896 B`

Previous baseline after Player icon pixels moved to the SDRAM runtime icon
arena and Library row/cover-path caches were made on-demand:

- `RAM_D1 108688 B`
- `RAM_D2 4 KB`
- `FLASH 772784 B`

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
SDRAM use must stay explicit and runtime-carved after the board memory service
is initialized. Core state such as `PlayerController`, `Theme`, and
`StyleSheet` must not be placed in SDRAM because startup code may touch static
storage before external memory is safe.

Current top RAM_D1 ownership after the Now Playing seekbar slice:

- `StyleSheet 11872 B`
- `PlayerController 9560 B`
- `FatFs static state 6152 B`
- `StyleSlot 4704 B`
- `Theme 3476 B`
- `ImageRegistry 2936 B`

Current `PlayerController` capacity breakdown:

- `track_capacity=256`
- `list_rows_bytes=0`
- `row_scratch_bytes=364`
- `list_cover_paths_bytes=0`
- `cover_path_scratch_bytes=264`
- `list_cover_cache_capacity=0`
- `list_cover_cache_bytes=0`
- `current_cover_bytes=280`
- `text_state_bytes=1444`
- `cover_path_state_bytes=1584`
- `ui_handles_bytes=1228`

Current `StyleSheet` PRODUCT profile:

- `widget_kind_count=28`
- `total_style_slots=66`
- `total_variant_slots=28`
- `style_rule_capacity=8`
- `max_metrics_pool=16`
- `style_table_static_bytes=5012`
- `rule_storage_bytes=1952`
- `stylesheet_size_bytes=11872`

Current `Theme` PRODUCT profile:

- `style_class_max=16`
- `player_style_class_max_id=11`
- `class_patch_bytes=3456`
- `class_on_bytes=16`
- `theme_size_bytes=3476`

Before visual recovery, every visual change must keep the Windows UI CI green
and refresh this H747 memory evidence so RAM/Flash movement remains auditable.
Current handoff gate:

- Windows `charm-player-win-vivid-md3` builds again.
- Windows `--ui-ci` is the host visual/runtime regression gate, including
  `now_playing_seek_*` coverage for hitbox, drag preview, commit, cancel,
  no-duration fallback, and time-label sync, plus `now_playing_closure_*`
  coverage for layout stack, cover stage, title block, control hierarchy, and
  fallback/no-duration layout stability.
- H747 `h747_lab_player_md3 -j 1` remains the PRODUCT/StaticCut firmware gate.

Now Playing is the first visual recovery slice after the portability pass. It
may refine shared layout, surface, typography, and control chrome, but it must
not add a H747-only page fork or reopen host-only capabilities. If a visual
change needs a new widget, payload cap, style cap, cover cache, font provider,
or transition capability, admit it through the PRODUCT profile and regenerate
this evidence.

Visual recovery handoff rules:

- Keep using the shared MD3 runtime/controller path; do not introduce a
  board-only hand-drawn UI fork.
- New widgets, style slots, payload caps, cover caches, font paths, or
  transition capabilities must enter through the PRODUCT profile and evidence
  gates.
- H747 default must keep `CHARM_PLAYER_LIST_COVER_CACHE_ENTRIES=0`,
  `CHARM_PLAYER_COVER_THEME_EXTRACT=0`,
  `CHARM_PLAYER_LAYERED_TRANSITIONS=0`,
  `CHARM_PLAYER_FILE_FONTS=0`, and `CHARM_PLAYER_DEBUG_UI=0` unless a dedicated
  product capability is explicitly admitted.
- Core state such as `PlayerController`, `Theme`, and `StyleSheet` must remain
  in RAM_D1; SDRAM usage must be runtime-carved and address-profiled.

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
