# H747 Lab Player MD3 Smoke Evidence

This document defines the board-level smoke collection protocol for
`h747_lab_player_md3`. It is evidence for the real MD3 Player-on-H747 bridge,
not a general UI benchmark and not a replacement for visual inspection.

The code-side token list lives in
`apps/player_md3/player_md3_smoke_schema.hpp`. The current schema version is
`1`. Field token names are append-only board evidence identifiers: changing or
removing an existing token requires updating this document and any serial
collector that depends on the old token.

## Verified Target

- Firmware target: `h747_lab_player_md3`
- Serial: `USART1 / 115200 8N1`
- Runtime path: shared MD3 `PlayerRuntime` through `PlayerRuntimeShell`
- Display path: SDRAM render surface -> `PlayerRasterDisplaySink` ->
  `display_raster` service -> LTDC/DSI panel
- Input path: GT970 touch and dual encoder facts -> `PlayerInputEvent`
- Host-only features: host storage, host file fonts, and host cover decode
  disabled
- Resource features: FatFs/VFS storage and file-font probe backend enabled by
  default; MCU runtime TTF binding disabled by default

## Collection Steps

1. Build and flash `h747_lab_player_md3`.
2. Open the serial console and wait for `player_md3: bootstrap ok`.
3. Wait for at least one `player_md3.loop` status line.
4. Run `status` at the `h747-player-md3>` prompt.
5. Optionally run `touch probe` if touch evidence needs to be refreshed.
6. Optionally run `input route reset` before a manual hardware input test, then
   exercise one source and run `input route status` or `status`.
7. Exercise input through hardware or console commands:
   `up`, `down`, `enter`, `back`, `play`, `next`, `prev`, and `mode`.
8. Capture the first boot status line, at least one loop status line, and any
   input-related status lines used as evidence.

The currently verified flash command for the DIY H747 lab board is:

```powershell
.\tools\flash-player-md3-pyocd.ps1
```

The script wraps the current pyOCD shape:

```powershell
pyocd load -u 0001 -t stm32h747xihx -f 1000k --connect halt --erase sector --format bin -a 0x08000000 .\cmake-build-h747-lab-debug\h747_lab_player_md3.bin
```

`Exception reading AP#2 IDR: Memory transfer fault` is expected on this board
with the current CMSIS-DAP/pyOCD path. Treat flashing as successful only when
pyOCD exits with code 0 and prints the final erase/program summary.

Serial collection uses `COM16`, `115200 8N1`.

The automated capture entry is:

```powershell
.\tools\capture-player-md3-smoke.ps1
```

By default it opens `COM16` at `115200`, resets and resumes the target through
`pyocd commander`, then writes the boot log to:

```text
cmake-build-h747-lab-debug/h747_lab_player_md3_smoke.log
```

The script exits with code 0 only when a single captured `player_md3` status
line passes the strict board evidence gate. The gate keeps the original core
tokens:

- `real_md3=1`
- `mock=0`
- `smoke=1/11111`
- `exec_fail=0`
- `co=0`
- `to=0`

It also requires the Display + Player evidence fields that align with
`profiles/profile_evidence.hpp`: `delta`, `display`, `sdram1`, `rt_store`,
`boot`, `render`, `frames`, `present`, `bytes`, `content`, `input`, `t`, `e`,
`p_src`, `p_dst`, `front`, `back`, `lfb`, and `lpf`.

DMA2D acceleration evidence is record-only in v1. Status lines append
`dma2d=<ready>/<used>/<fallback>/<err>/<hal>/<dma_err>`. A healthy accelerated
run should show `ready=1`, increasing `used`, and zero errors. If DMA2D bring-up
or transfer fails, `display_raster` falls back to the CPU copy path and the
basic display smoke gate should still pass.

Console TX DMA evidence is also record-only in v1. Status lines append
`console_tx=<started>/<done>/<bytes>/<fallback>/<dropped>/<busy>/<used>/<size>`.
Long `help`, `touch help`, and `status` output should normally increase
`started`, `done`, and `bytes` while keeping `dropped=0`. If USART1 TX DMA is
not usable, the console service falls back to short blocking bytes or drops
overflowing bytes instead of stalling the main loop.

The current strict numeric checks are intentionally minimal:

- `delta=<frames>/<presents>` has both sides non-zero.
- `frames` and `present` are greater than zero.
- `display=1`, `sdram1=1/1`, `rt_store=1`, `boot=1`, and `render=1`.
- `bytes=0x00384000`, matching the 720x1280 ARGB8888 framebuffer size.
- `content=<bg>:<non_bg>@<min_x>,<min_y>-<max_x>,<max_y>` has `non_bg > 0`.
- `input`, `t`, and `e` fields have the expected fact shape, but hardware input
  events are not required to be non-zero for automatic boot acceptance.

Resource evidence is optional and does not change the basic display smoke:

```powershell
.\tools\capture-player-md3-smoke.ps1 -ResourceSmoke
```

With `-ResourceSmoke`, the capture also waits for `fs=`, `font=`, `cover=`,
and `media=` fields. After the basic display smoke is green, the script sends
`resource status` so the accepted resource line comes from the full app-local
probe instead of the boot-time lightweight state refresh. Missing files are
accepted as evidence when resources have not yet been copied to eMMC; the
fields are intended to show which layer is missing rather than to fail the
first-frame MD3 smoke.

The console command `resource status` re-runs the same app-local resource probe
and prints a fresh `player_md3` status line. It is useful after updating eMMC
contents without changing the firmware.

Manual input route evidence uses the same status schema and does not alter
Player state by itself:

```text
input route reset
input route status
```

`input route reset` clears only the app-local route counters and last-route
fields. It does not clear raw hardware facts, input event totals, controller
state, or smoke verdicts. This makes it useful for one-source-at-a-time board
bring-up: reset, touch/rotate/press once, then read `input_route` and `input`.

Playback smoke is a populated-resource gate:

```powershell
.\tools\capture-player-md3-smoke.ps1 -PlaybackSmoke
```

With `-PlaybackSmoke`, the capture first requires the basic display smoke and a
populated resource line. It then sends `playback smoke` over the same serial
console, and the firmware starts the first scanned track through the existing
Player input/control path. The command is rejected as a playback gate when the
resource line does not satisfy `fs=1/<n>/1/0`, primary `font` open, and
`media=1/*/1/0`. Runtime file-font binding is not required for playback smoke
unless `CHARM_PLAYER_MCU_RUNTIME_FILE_FONTS=ON`; the default H747 firmware
continues with built-in UI fonts when the primary font file is present but not
runtime-bound.

Input smoke is optional and does not require hardware interaction:

```powershell
.\tools\capture-player-md3-smoke.ps1 -InputSmoke
```

With `-InputSmoke`, the capture first waits for a valid basic smoke line, then
sends `input smoke` over the serial console. The firmware injects the fixed
semantic command sequence `down`, `up`, `mode`, `play`, `next`, `prev`,
`enter`, and `back` through the same `PlayerInputEvent` boundary used by
hardware input, forces one evidence render, and prints a fresh status line.

Touch trace collection is optional and does require manual interaction:

```powershell
.\tools\capture-player-md3-smoke.ps1 -TouchTrace
```

With `-TouchTrace`, the capture first waits for a valid basic smoke line, then
waits for a live GT970 trace line. Touching the panel should produce a line of
the form `touch action=<down|move|up|cancel> down=<0|1> x=<n> y=<n> ...`.
This proves the hardware coordinate stream reached the app-local input bridge;
route counters in the following status lines prove the same event path reached
the app-local input boundary. `touch_dispatch` then shows whether those pointer
events were allowed into `PlayerRuntimeShell`.

For direct serial testing without PowerShell automation, use the board prompt:

```text
touch help
touch monitor on
```

`help` intentionally prints only the daily MD3 smoke commands. `touch help`
prints the longer GT9xx bring-up/debug command matrix so normal serial sessions
do not spend time dumping rarely used diagnostics.

This low-load diagnostic mode pauses MD3 render/status work after the command
has been accepted, but keeps polling console input and the GT970 input bridge.
It is useful when the normal long `player_md3.loop` line interleaves with typed
commands. Touching the panel should then print short lines:

```text
touch down x=<n> y=<n> down=1 n=<count>
touch move x=<n> y=<n> down=1 n=<count>
touch up x=<n> y=<n> down=0 n=<count>
```

Use:

```text
touch monitor status
touch monitor off
```

`touch monitor status` prints the monitor counter and the last sampled touch
fields. `touch monitor off` resumes normal rendering and automatic status
behavior.

For a more aggressive low-load hardware sampling mode, use:

```text
touch sample on
<touch or drag the panel for 5-10 seconds>
touch sample status
touch sample off
```

`touch sample on` pauses MD3 render/status work and does not dispatch pointer
events into the Player UI. It keeps only the console and GT9xx input sampler
active, prints short `touch_sample ...` lines, and records sample count, ready
hits, INT changes, raw coordinate range, filtered coordinate, and the last
out-of-bounds clamp. Use it to distinguish "render is too slow and we miss
short touch frames" from "the controller is not producing frames at all".

Hardware touch dispatch into the Player UI remains manually gated for
bring-up/regression isolation:

```text
touch dispatch status
touch dispatch off
touch dispatch on
touch dispatch once
```

The default is `off`. Touch events still print `touch action=...` lines and
update app-local touch/input-route evidence, but valid in-range pointer events
are blocked before `PlayerRuntimeShell` until `touch dispatch on` is issued.
The earlier press-path UsageFault was fixed by the WidgetHandle alignment
closure; keep the gate because it still lets GT9xx coordinate/config testing run
without changing Player UI state. Use `touch dispatch once` for first-pass UI
validation because it only releases the next down/up pointer sequence.

Touch coordinate mapping is app-local and can be changed at the serial prompt:

```text
touch map status
touch map normal
touch map swap
touch map invx
touch map invy
touch map rot90
touch map rot270
```

Each mode records `touch_map=<mode>/<raw_x>,<raw_y>/<ui_x>,<ui_y>@<raw_w>x<raw_h>/<display_w>x<display_h>`
in the status line. Use the four corners and center of the panel to pick the
mode that matches the visible UI.

Touch latency evidence is also record-only:

```text
touch latency reset
touch latency status
```

`touch_latency` reports sample count plus rough millisecond deltas for
INT-to-poll, poll-to-dispatch, and dispatch-to-next-frame. It is intended to
quantify whether M7 render load is delaying input handling before moving input
sampling to M4.

Render throttling is app-local and intended to keep the M7 input/console pump
from being starved by full-frame MD3 renders:

```text
render throttle status
render throttle on
render throttle off
```

When throttle is enabled, the loop still polls console/input first and ticks the
real runtime, but skips full render unless an input event marks the scene dirty
and the short coalescing window has elapsed, or the low-frequency keepalive
interval expires. The status line appends
`render_throttle=<enabled>/<skipped>/<forced>/<interval_ms>/<dirty>` so slow
interaction can be separated from Vivid execute cost.

If the monitor reports `ready=1` but `events=0`, collect raw GT970 evidence:

```text
touch raw
touch raw on
touch raw status
touch raw dump
touch raw off
```

`touch raw` bypasses `InputFrameTracker` and the Player runtime. It prints the
input service snapshot with `ready`, `detected`, `down`, `contacts`, GT970
status byte, INT/RST pin levels, HAL/I2C status, address, coordinates,
resolution, pressure, and version bytes. If raw output changes while
`events=0`, the break is in app-local frame tracking. If raw output never
changes while pressing the panel, the break is in GT970 sampling, INT/RST/I2C,
or the touch controller state machine.

`touch raw dump` reads the raw point/status window starting at `0x814E` and
prints the status byte plus 40 following bytes. It is intended for coordinate
layout checks when a config table produces touch-ready frames but the parsed
coordinates look impossible.

For GT9xx register-level triage:

```text
touch bus status
touch bus recover
touch reprobe
touch debug
touch int reset
touch int status
touch info
touch cfg verify
touch scan status
touch scan wake
touch scan reset
touch wake
touch reset14
touch reset5d
touch reset seq14
touch reset seq5d
touch reset try14
touch reset try5d
touch cfg luat
touch cfg luat0 native
touch cfg luat1 native
touch cfg luat2 native
touch cfg fire native
touch cfg int rising
touch cfg int falling
touch cfg int low
touch cfg int high
touch cfg luat reset
touch softreset
```

Always start a config-table comparison from a stable bus and product read:

```text
touch bus status
touch bus recover
touch reprobe
touch info
touch cfg verify
touch scan status
```

`touch bus status` prints I2C4 HAL state/error, SCL/SDA/INT/RST levels,
current `ready/addr`, and the last probe statuses. `touch bus recover`
deinitializes/reinitializes I2C4, releases SCL in GPIO mode if needed, and then
reprobes `0x14` before `0x5D`. `touch reprobe` only detects the controller
product/version and does not write config bytes. If `touch info` does not show
`product="9157"`, do not compare Luat/Fire config tables yet; first recover the
bus or restore the address.

`touch debug` reads command register `0x8040`, point/status register `0x814E`
plus the first point payload, version register `0x8140`, and the beginning of
the config register block at `0x8047`. `touch wake` writes `0` to command
register `0x8040`, clears `0x814E`, then prints the same debug snapshot. If
I2C reads are ok and version/config are plausible but the point/status byte
stays `0x00` while pressing the panel, the controller is present but not
reporting touch-ready frames.
`touch scan status` prints the command byte, status byte, config summary,
`0x8140..0x8158` runtime window, first point window, INT edge counters, and
HAL/I2C status in one line. It also prints `bus_ok`, `read_mask`, and
`recover_hint`. A healthy full register read should have `read_mask=0x0F`.
`recover_hint=1` means no current address, `2` means I2C error, and `3` means
one or more register windows failed to read. `touch scan wake` writes command
`0`, clears `0x814E`, briefly syncs TP_INT low-to-input, samples a few frames,
and prints the same scan line. `touch scan reset` sends command `0x02`,
re-runs the current ensure-config path, wakes, and prints scan evidence.

`touch reset14` and `touch reset5d` are legacy aliases that perform a GT9xx
address-select reset by driving TP_INT during TP_RST release, then re-probe the
selected address. `touch reset seq14` and `touch reset seq5d` additionally
run the current config ensure path and print `touch info` plus `touch scan`
evidence. They are diagnostic only. `touch reset try14` and `touch reset try5d`
attempt the same address-select reset but preserve the previous usable
`ready/addr/version` if the requested address fails; use the `try*` commands
when comparing address behavior because they avoid poisoning the current
session with `ready=0 addr=0`.

`touch cfg luat` writes Luat candidate table 2 with 720x1280 patched into the
config. `touch cfg luat0/1/2 native` writes the three Luat candidate tables
without changing the table's own resolution bytes. `touch cfg fire native`
writes the Fire BSP `CTP_CFG_GT9157` table without changing its resolution.
The non-native variants patch output resolution to 720x1280. Extra explicit
variants exist for `800x480`, `1024x600`, and `1280x720`, for example
`touch cfg fire 800x480` and `touch cfg luat2 1024x600`. Each command
recomputes the Goodix checksum, sets the fresh flag, and prints verify plus
scan evidence. `touch cfg fire native` additionally prints
`cfg_src=fire/native`, the table max resolution, `module_switch1/2`, and
`fresh`, because it is currently the first candidate observed to produce a
GT9157 touch-ready frame on this board.

Fire native is still a diagnostic candidate, not the startup default. One
captured Fire-native event reported an out-of-range coordinate
`x=19712 y=57089 max=800x480` and then faulted in Vivid:
`pc=0x0804FA32 -> SoaKernel::input_handle_press(int,int,int)`,
`lr=0x08053789 -> SoaKernel::parent(WidgetHandle) const`. The app-local H747
input bridge now records `touch_oob=...` and drops out-of-range pointer samples
before dispatching them to `PlayerRuntimeShell`; this prevents malformed
bring-up coordinates from entering Vivid while preserving raw diagnostic output
for coordinate-layout triage.

`touch cfg int rising/falling/low/high` changes only the Goodix
`module_switch1[1:0]` interrupt mode bits, recomputes checksum/fresh, then
prints INT/raw/scan evidence. MCU EXTI remains configured for both rising and
falling edges, so this command tests the controller's INT mode without hiding
physical edges at the MCU side.

These config commands are not part of the product UI path. They exist to
distinguish a bad panel config table from an address/reset/INT/wiring problem.
`touch cfg luat reset` performs the same write and then sends command `0x02`
to force a GT9157 soft reset before printing the debug snapshot. Use it when
the config bytes can be read back but the point/status register still does not
produce touch-ready frames. `touch softreset` runs only the soft-reset/wake
sequence.

Recommended dead-touch matrix:

```text
touch bus status
touch bus recover
touch reprobe
touch info
touch cfg verify
touch scan status
touch int reset
<touch or drag the panel>
touch scan status
touch int status
touch reset try14
touch bus status
touch reset try5d
touch bus status
touch bus recover
touch reprobe
touch info
touch cfg fire native
touch scan wake
touch int reset
<touch or drag the panel>
touch scan status
touch int status
touch cfg luat0 native
touch scan wake
touch scan status
touch cfg luat1 native
touch scan wake
touch scan status
touch cfg luat2 native
touch scan wake
touch scan status
touch cfg int rising
touch cfg int falling
touch cfg int low
touch cfg int high
```

The Luat/Fire commands are meaningful only when `touch info` can repeatedly
read `product="9157"` and `touch scan status` does not report `recover_hint=1`
or `recover_hint=2`. If a reset or address test leaves the bus at
`ready=0 addr=0`, run `touch bus recover`, `touch reprobe`, and `touch info`
before drawing conclusions about any config table.

If any step produces `status=0x8?`, `contacts>0`, changing point bytes, or
non-zero INT rise/fall counts, keep that address/config/INT combination and
continue with coordinate mapping. If all candidate tables verify but
`status=0x00`, point bytes, and INT edges never change while pressing the
panel, current evidence points away from Player input routing and toward the
panel config table, FPC/sensor/INT physical side, or touch analog/sensor power.

`touch int status` prints only the `TP_INT` evidence:

```text
touch_int ok=<ok> ready=<ready> profile=<id> addr=<addr> int=<level>/<rise>/<fall>/<last_ms>/<exti>@<last_level> rst=<level> pending=<pending>
```

Use `touch int reset`, touch the panel, then run `touch int status` again. If
`rise` and `fall` stay zero while `touch info` and I2C reads are healthy, the
current evidence points at the Goodix panel config, the INT physical wire, or
the touch sensor side rather than the Player runtime.

`touch info` reads the GT9xx runtime block at `0x8140..0x814E`, including
product id, firmware, runtime resolution, sensor/vendor byte, status, HAL/I2C
status, and raw bytes. `touch cfg verify` reads the full `0x8047..0x8100`
config block and prints length, config version, resolution, touch count,
module switch bytes, refresh byte, checksum read/expected/ok, fresh flag, and
the first eight config bytes. If checksum or fresh evidence is wrong, fix the
config write path before debugging Player input routing.

## Required Status Fields

A healthy loop status line must contain:

- `real_md3=1`
- `mock=0`
- `smoke=1/11111`
- non-zero `delta=<frames>/<presents>`
- increasing `frames=<n>` and `present=<n>` across loop samples
- `display=1`
- `sdram1=1/1`
- `rt_store=1`
- `boot=1`
- `render=1`
- `cmd=<used>/<capacity>` with `used > 0`
- `co=0`
- `to=0`
- `exec_fail=0`
- `content=<bg>:<non_bg>@<min_x>,<min_y>-<max_x>,<max_y>` with
  `non_bg > 0` and a non-empty bounds box
- `bytes=0x00384000`
- `p_src=<first>/<center>/<last>` and `p_dst=<first>/<center>/<last>`
- `front=<addr>:<first>/<center>/<last>` and `back=<addr>:<first>/<center>/<last>`
- `lfb=<addr>` and `lpf=<value>`
- record-only `dma2d=<ready>/<used>/<fallback>/<err>/<hal>/<dma_err>`
- record-only `console_tx=<started>/<done>/<bytes>/<fallback>/<dropped>/<busy>/<used>/<size>`

`smoke=1/11111` expands to:

- boot check passed
- render check passed
- present check passed
- content check passed
- scene execution check passed

## Input Evidence

The same status line also carries input bridge evidence:

- `input=<polls>/<events>` records input sampling and dispatched Player input
  events.
- `t=<probe>/<ready>/<down>@<x>,<y>` records touch probe state, current touch
  readiness/down state, and the last reported coordinates.
- `e=<touch>/<encoder>/<button>` records touch, encoder, and button events
  translated into the Player input boundary.
- `input_route=<console>/<touch>/<encoder>/<button>@<src>/<kind>/<code>`
  records app-local routing evidence. The counters show how many events from
  each source reached the app-local input bridge. Console, encoder, and button
  commands are dispatched to the Player runtime immediately; hardware touch
  pointers may be held by the `touch_dispatch` bring-up gate. `src` is `0`
  unknown, `1` console, `2` touch, `3` encoder, and `4` button. `kind=1` means
  a semantic command and `kind=2` means a pointer action. `code` is the command
  enum value for commands or pointer action enum value for touch.
- `input_smoke=<ok>/<cmds>/<before>-<after>/<frames>/<exec_fail>` records the
  automatic serial input smoke verdict, command count, input event count before
  and after injection, frames rendered by the smoke command, and scene execute
  failure count after the sequence.
- `touch_oob=<count>/<raw_x>,<raw_y>/<clamped_x>,<clamped_y>@<max_x>x<max_y>`
  records app-local pointer clamp evidence. A non-zero count means at least one
  raw GT9xx coordinate exceeded the current touch max and was blocked before
  entering the Player runtime.
- `touch_sample=<enabled>/<samples>/<hits>/<int_changes>/<raw_min_x>,<raw_min_y>-<raw_max_x>,<raw_max_y>/<filtered_x>,<filtered_y>`
  records the low-load `touch sample` diagnostic state. It is evidence only and
  is not part of the strict smoke gate.
- `touch_dispatch=<enabled>/<once>/<seq>/<blocked>/<last_action>` records
  whether hardware touch pointers are currently allowed into `PlayerRuntimeShell`,
  whether one-shot dispatch is armed/in-progress, how many valid in-range
  pointers were blocked by the bring-up gate, and the last blocked action.
- `touch_map=<mode>/<raw_x>,<raw_y>/<ui_x>,<ui_y>@<raw_w>x<raw_h>/<display_w>x<display_h>`
  records the active app-local touch mapping and the last raw-to-UI conversion.
- `touch_ui=<enabled>/<sent>/<blocked>/<fault_guard>/<last_action>` records UI
  dispatch evidence for hardware pointer events.
- `touch_latency=<samples>/<last_int_poll>,<last_poll_dispatch>,<last_dispatch_frame>/<max_int_poll>,<max_poll_dispatch>,<max_dispatch_frame>`
  records rough M7 touch latency evidence in milliseconds.

Hardware input evidence is preferred. Console commands are acceptable for
bring-up because they inject the same semantic Player command path, but they do
not prove the GT970 or encoder hardware path by themselves.

The automatic strict gate only checks that input facts are present and shaped
correctly. This matches the Phase 1 `Input.primary_input` profile evidence:
the board provider is `h747_input_service`, pointer capability is
`gt9xx_best_effort`, and encoder capability is `dual_encoder`; it does not
require a human to touch the panel during every CI-style capture.

## Record-Only Render Performance Evidence

The status line also appends record-only Vivid render evidence:

- `cmd_batch=<all>/<rect_like>/<line_path_image>` records command buffer batch
  shrink pressure.
- `exec_batch=<dispatch_groups>/<batch_flushes>/<clip_failures>/<overflow>` records
  executor grouping and clipping pressure.
- `exec_groups=<rect>/<text>/<image>/<other>` records executor dispatch group mix.
- `exec_cmds=<rect>/<text>/<image>/<line>/<path>/<other>` records command kind mix.
- `exec_fail_detail=<text>/<image>/<other>` records failure categories.
- `perf_time=<available>/<frame>/<tick>/<render>/<record>/<execute>/<present>`
  records microsecond timing sampled from DWT `CYCCNT` when available. `record`
  is Vivid scene command recording, `execute` is DrawCmd execution, and
  `present` is the display sink flush/present path.
- `render_throttle=<enabled>/<skipped>/<forced>/<interval_ms>/<dirty>` records
  whether the app-local M7 loop is skipping full renders, how many frames were
  skipped, how many explicit evidence renders were forced, the keepalive
  interval, and whether an input event has requested a redraw.
- `console_tx=<started>/<done>/<bytes>/<fallback>/<dropped>/<busy>/<used>/<size>`
  records USART1 TX DMA activity and fallback/drop evidence for long serial
  output. This field is not part of the basic smoke gate.

These fields are diagnostics only. They do not participate in the strict smoke
gate yet and should be treated as baseline evidence for later render
optimization work.

`input_route` is diagnostic evidence only. It lets manual bring-up distinguish
"hardware sampled but not routed" from "routed into the real Player runtime but
the UI did not visibly react". It is append-only and must not become a product
input API.

For manual hardware route checks, use this sequence:

```text
input route reset
<touch panel, rotate encoder, or press encoder button>
input route status
```

Expected route source changes:

- Touch interaction increments the second `input_route` counter and leaves the
  last route as `2/2/<pointer-action>`; `touch_dispatch` decides whether it was
  forwarded into the Player UI.
- Encoder rotation increments the third `input_route` counter and leaves the
  last route as `3/1/<command>`.
- Encoder button press increments the fourth `input_route` counter and leaves
  the last route as `4/1/<command>`.

If raw fields under `t=` or `e=` change but `input_route` does not, the break is
inside the app-local source-to-event bridge. If `input_route` changes but
`input=<polls>/<events>` does not, the break is after route normalization and
before or inside `PlayerRuntimeShell::dispatch_input()`.

Input smoke acceptance requires `input_smoke=1/<cmds>/<before>-<after>/<frames>/0`
with `cmds >= 8`, `after > before`, and `frames > 0`. It also requires
`input_route` to show at least `cmds` console-routed commands and the last route
to be `1/1/4`, meaning console source, command kind, and `Back` command code.
The command must not directly mutate controller state; it only injects semantic
input into the runtime shell.

## Storage And Resource Evidence

Resource smoke fields are appended to the normal status line:

- `storage=<ready>/<fat_probe>/<reads>/<fails>@<part_lba>:<blocks>` records the
  board storage service state and remains the first storage-layer signal.
- `storage_detail=<attempted>/<initialized>/<block_ready>/<part_auto>@<block_size>:<init>/<hal>/<err>/<card>/<wait_to>/<last_lba>/<last_count>/<sta>`
  records the eMMC bring-up details used to separate "service not called" from
  HAL init, card-state, transfer-timeout, and FAT probe failures. `err` and
  `sta` are hexadecimal.
- `storage_bus=<selected>/<wide8>/<wide4>/<wide1>` records the selected SDMMC
  bus width and the HAL status observed while trying 8-bit, 4-bit, and 1-bit
  operation. The current bring-up starts `HAL_MMC_Init()` in 1-bit mode and
  treats wider modes as optional upgrades so a failed wide-bus switch does not
  hide a usable 1-bit block device path.
- `fs=<mount_ok>/<tracks>/<has_tracks>/<mount_err>` records Player mount and
  media scan state. `tracks` counts audio files discovered by the current scan.
- `font=<primary_open>/<fallback_open>/<runtime_bound>/<err>` probes the active
  Player font resource config. The primary path comes from
  `player.product_config::default_font_path`, currently
  `/font/gflex_variable.ttf`. `fallback_open` is meaningful only when a fallback
  path is configured. `runtime_bound=1` means the MD3 runtime actually bound the
  file-font package. H747 defaults to `CHARM_PLAYER_MCU_RUNTIME_FILE_FONTS=0`,
  so `font=1/0/0/0` is a valid "primary file exists, rendering uses built-in
  fallback" state.
- `font_cfg=<kind>/<primary_configured>/<fallback_configured>/<compat_open>`
  records which font resource mode is active and whether a legacy Noto-style
  compatibility font was also found. `compat_open=1` is diagnostic only and is
  not the product contract.
- `cover=<folder_found>/<decode_ok>/<w>x<h>/<err>` checks first-track folder
  cover candidates, then falls back to embedded cover decode.
- `media=<first_open>/<duration_ok>/<track_ready>/<err>` checks first-track VFS
  open, duration probe, and Player track readiness.
- `playback=<state>/<running>/<track_ready>/<cb>/<underrun>/<stage>/<err>`
  records the AudioPlayer state, whether the player is running, controller
  track readiness, I2S DMA half/full callback total, underrun count, and last
  AudioPlayer error stage/code.
- `playback_smoke=<ok>/<before>-<after>/<frames>/<saw_playing>/<stage>/<err>`
  records the automatic playback smoke verdict, DMA callback count before and
  after the command, frames rendered during the command, whether `playing` was
  observed, and the final stage/error. `stage=-1` is reserved by this board
  smoke for "resource precondition failed"; successful playback smoke requires
  `stage=0` and `err=0`.

Current resource layout:

- Primary font: `/font/gflex_variable.ttf`
- Optional fallback font: configured by `CHARM_PLAYER_RESOURCE_FONT_FALLBACK_PATH`
- Compatibility fonts probed for diagnostics only:
  `/font/NotoSansSC-Regular.ttf`, `/font/NotoSans-Regular.ttf`
- Music: `/music` first, then `/` and one level of subdirectories
- Folder covers: `cover.jpg`, `cover.png`, `cover.bmp`, `folder.jpg`,
  `folder.png`, `folder.bmp`

The optional staging helper creates the expected directory shape on the host:

```powershell
.\tools\stage-player-md3-resources.ps1 `
  -PrimaryFont <path-to-gflex_variable.ttf> `
  -PrimaryFontName gflex_variable.ttf `
  -FallbackFont <path-to-optional-fallback.ttf> `
  -FallbackFontName NotoSans-Regular.ttf `
  -Track <path-to-one-mp3-flac-or-wav> `
  -Cover <path-to-cover-jpg-png-or-bmp>
```

`PrimaryFontName` and `FallbackFontName` control the board-side filenames under
`/font`; the defaults match the current product primary path and optional Noto
compatibility fallback.

By default it writes to:

```text
cmake-build-h747-lab-debug/player_md3_resources
```

Copy that directory's contents to the FAT root of eMMC. The helper does not
write to the board by itself.

Common error values follow the shared `Errc` numeric values: `0` means ok,
`-2` means not found, `-5` means I/O error, `-95` means unsupported capability,
and `1002` means decode failure.
When eMMC resources are not populated, `font=0/*/...`, `fs=.../0/...`, and
`cover=0/0/...` are expected and must not invalidate `smoke=1/11111`.

Two acceptance modes are defined:

- Empty-resource smoke: basic display smoke passes and resource fields are
  present. `fs=0/...`, `font=0/*/...`, `media=0/...`, and `cover=0/0/...` are
  valid as long as the error values explain the missing layer.
- Populated-resource smoke: after copying the minimal resource layout to eMMC,
  expect `fs=1/<n>/1/0`, `font=1/*/<runtime_bound>/0`,
  `font_cfg=1/1/*/*`, and `media=1/*/1/0` when the firmware is configured with
  `CHARM_PLAYER_FILE_FONTS=ON`. The default H747 firmware keeps
  `CHARM_PLAYER_MCU_RUNTIME_FILE_FONTS=OFF`, so `font=1/0/0/0` is valid when
  no fallback path is configured. If runtime file-font binding is explicitly
  enabled, expect the runtime-bound field to become `1`. If a folder cover
  exists, expect
  `cover=1/1/<w>x<h>/0` only when a board cover provider or MCU-safe decoder is
  present. Without that capability, `cover=1/0/0x0/-95` is the expected evidence
  that the file was found but dynamic cover decode is not available on this
  firmware.
- Playback smoke: after populated-resource smoke is green, expect
  `playback_smoke=1/<before>-<after>/<frames>/1/0/0` with `after > before` and
  `frames > 0`. The accepted status line must still include the basic smoke
  tokens `smoke=1/11111`, `exec_fail=0`, `co=0`, and `to=0`. Human hearing is
  useful extra evidence, but it is not an automatic gate.

## Failure Triage

- `real_md3=0` or missing: the target is not running the shared MD3 runtime
  path and must not be accepted.
- `mock != 0`: the app has fallen back to a probe/mock path and must not be
  accepted.
- `display=0`, `sdram1=0/*`, or `rt_store=0`: check memory and raster display
  service readiness before changing Player code.
- `boot=0`: check `PlayerRuntimeShell::bootstrap()` wiring, storage defaults,
  controller construction, and page construction.
- `render=0`: check the Player render loop, render surface binding, and scene
  build path.
- `delta=0/*` or `delta=*/0`: check whether the main loop is running and
  whether `display_raster` is accepting presents.
- `co=1` or `to=1`: increase or reduce scene command/text pressure only after
  recording the failing page and status line.
- `exec_fail != 0`: inspect unsupported draw commands, text/image execution
  failures, and Vivid raster execution stats.
- `content` has zero non-background pixels or an empty bounds box: the scene may
  be executing without visible MD3 content, or the render surface may not be the
  surface being presented.
- `input` counters do not change after hardware interaction: check
  `services/input` first, then the app-local `PlayerInputEvent` translation.
- `input_route` source counters change but `input` total does not: check the
  runtime dispatch bridge before changing hardware sampling.
- `input_route` source counters do not change while raw hardware facts do:
  check the app-local source-to-command mapping in `player_md3_input.cpp`.
- `input_smoke=0/*`: check the serial command path, `dispatch_runtime_command`,
  scene execution health, and command/text overflow fields before changing
  controller logic.
- `touch_oob>0`: keep the raw coordinate and `touch raw dump` bytes, then check
  GT9xx point-byte layout, active config table, sensor/channel mapping, and
  max resolution before changing Vivid or Player UI code.
- `touch_sample` hits grow while normal `touch monitor` misses events: render
  load is likely lowering the polling rate enough to miss short touch-ready
  windows. Keep the working config table and move the next fix toward a higher
  frequency input pump or interrupt-assisted sampling.
- `touch_dispatch=0/<n>/*` with `touch action=...` lines: the GT9xx hardware and
  app-local input bridge are producing pointer events, but dispatch into the
  Player UI is intentionally gated off for bring-up isolation. Use
  `touch dispatch once` or `touch dispatch on` when validating UI input.
- `storage=0/*` or `fs=0/*`: check eMMC init, partition detection, FAT probe,
  and FatFs mount before changing Player UI.
- `storage_detail=0/*`: the storage init node did not run; check profile wiring.
- `storage_detail=1/0/*`: the storage init node ran but HAL/card bring-up did
  not reach ready; use the `init`, `hal`, `err`, `card`, `wait_to`, and `sta`
  values before changing Player resource code.
- `storage_bus=1/*`: the card stayed on the conservative 1-bit path. This is
  acceptable for resource bring-up; fix 4-bit/8-bit only after VFS/media smoke
  is otherwise green.
- `font=0/*/*`: copy the configured primary font to `/font` or update
  `CHARM_PLAYER_RESOURCE_FONT_PATH`. Check `font_cfg` before assuming a Noto
  file name is required.
- `media=0/*`: check that audio files are under `/music` or one scanned root
  subdirectory and that VFS paths can be opened.
- `cover=1/0/*`: a folder cover was found but image decode failed; check image
  format and size. `cover=0/0/*` with no tracks or no cover file is acceptable.
- `playback_smoke=0/*` with `stage=-1`: the playback command did not run because
  resource preconditions are not green. Fix `fs`, `font`, or `media` first.
- `playback_smoke=0/*` with callback count unchanged: check I2S DMA startup,
  DMA IRQ routing, and `audio.sink.i2s` callback wiring before changing UI.
- `playback=<state>/*/<stage>/<err>` with a non-zero stage/error: triage the
  reported AudioPlayer stage first, for example open source, decode, sink open,
  buffer allocation, or sink start.

## Current Acceptance

The first acceptable board evidence for this target is a captured serial sample
where a `player_md3.loop` line reports:

```text
real_md3=1 mock=0 smoke=1/11111 ... delta=<non-zero>/<non-zero> ... display=1 ... bytes=0x00384000 ... exec_fail=0 ... input=<polls>/<events> ... content=<bg>:<non-zero>@...
```

This proves the board is running the shared MD3 Player runtime through the
display/input boundary, with the expected 720x1280 ARGB8888 double-buffered
raster path. Resource smoke can additionally prove storage-backed library
scanning, file font presence, and cover/media decode evidence when eMMC has
been populated. Playback smoke can additionally prove that the first scanned
track reaches the I2S DMA callback path. It does not yet claim high frame rate,
DMA2D acceleration, production touch gesture policy, or subjective audio
quality.

## Captured Sample

Captured on 2026-05-25 after flashing `h747_lab_player_md3` with pyOCD:

```text
player_md3 real_md3=1 mock=0 smoke=1/11111 delta=1/1 display=1 sdram1=1/1 rt_store=1 boot=1 render=1 frames=1 present=1 layer=1 fb=0xC0000000 bytes=0x00384000 render_buf=0xC0708000 platform=0xC0A8C000 runtime=0xC0D97328 pool_bytes=0x00724D20 r_s=0xFF101218/0xFF101218/0xFF12141E cmd=111/1024 co=0 text=225/4096 to=0 exec_fail=0 exec=60/31/10 fail_ti=0/0 input=0/0 t=1/1/0@0,0 e=0/0/0 storage=0/0/0/0@0:0 audio=1/1/0/0 content=0xFF101218:382479@0,3-719,1279 p_src=0xFF101218/0xFF101218/0xFF12141E p_dst=0xFF101218/0xFF101218/0xFF12141E front=0xC0384000:0xFF101218/0xFF101218/0xFF12141E back=0xC0000000:0x00000000/0x00000000/0x00000000 lfb=0xC0384000 lcr=0x00000001 lpf=0x00000000
```

The same boot log also printed `player_md3: bootstrap ok`,
`player_md3: first render ok`, and the `h747-player-md3>` console prompt.

## Current Board Verification

2026-05-26 verification:

- `cmake --build --preset build-h747-lab-player-md3-debug -- -j1` passed and
  produced `h747_lab_player_md3.bin`.
- `.\tools\flash-player-md3-pyocd.ps1` completed successfully through pyOCD
  using `bin` at `0x08000000`. pyOCD still reported the expected
  `Exception reading AP#2 IDR: Memory transfer fault`, then completed with:
  `Erased 1441792 bytes (11 sectors), programmed 1441792 bytes (1408 pages)`.
- `.\tools\capture-player-md3-smoke.ps1` passed the strict display smoke gate.
- `.\tools\capture-player-md3-smoke.ps1 -ResourceSmoke` passed the resource
  field gate and reported:

```text
resource=empty-or-missing fs=not-mounted(err=-2) no-tracks font=font-primary-missing(err=-38) font_cfg=1/1/0/0 media=media-missing(err=-2) cover=cover-missing(err=-2)
```

The accepted status line included:

```text
storage=0/0/0/0@0:0 audio=1/1/0/0 fs=0/0/0/-2 font=0/0/0/-38 font_cfg=1/1/0/0 cover=0/0/0x0/-2 media=0/0/0/-2
```

This is the expected empty-resource board state before eMMC is populated. It
does not invalidate the MD3 Home first-frame smoke.
