# H747 Lab Player MD3 Smoke Evidence

This document defines the board-level smoke collection protocol for
`h747_lab_player_md3`. It is evidence for the real MD3 Player-on-H747 bridge,
not a general UI benchmark and not a replacement for visual inspection.

## Verified Target

- Firmware target: `h747_lab_player_md3`
- Serial: `USART1 / 115200 8N1`
- Runtime path: shared MD3 `PlayerRuntime` through `PlayerRuntimeShell`
- Display path: SDRAM render surface -> `PlayerRasterDisplaySink` ->
  `display_raster` service -> LTDC/DSI panel
- Input path: GT970 touch and dual encoder facts -> `PlayerInputEvent`
- Host-only features: storage, file fonts, and host cover decode disabled

## Collection Steps

1. Build and flash `h747_lab_player_md3`.
2. Open the serial console and wait for `player_md3: bootstrap ok`.
3. Wait for at least one `player_md3.loop` status line.
4. Run `status` at the `h747-player-md3>` prompt.
5. Optionally run `touch probe` if touch evidence needs to be refreshed.
6. Exercise input through hardware or console commands:
   `up`, `down`, `enter`, `back`, `play`, `next`, `prev`, and `mode`.
7. Capture the first boot status line, at least one loop status line, and any
   input-related status lines used as evidence.

The currently verified flash command for the DIY H747 lab board is:

```powershell
.\tools\flash-player-md3-pyocd.ps1
```

The script wraps the known-good pyOCD shape:

```powershell
pyocd load -u 0001 -t stm32h747xihx -f 1000k --format elf .\cmake-build-h747-lab-debug\h747_lab_player_md3.elf
```

`Exception reading AP#2 IDR: Memory transfer fault` is expected on this board
with the current CMSIS-DAP/pyOCD path. Treat flashing as successful only when
pyOCD exits with code 0 and prints the final erase/program summary.

Serial collection uses `COM16`, `115200 8N1`.

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

Hardware input evidence is preferred. Console commands are acceptable for
bring-up because they inject the same semantic Player command path, but they do
not prove the GT970 or encoder hardware path by themselves.

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

## Current Acceptance

The first acceptable board evidence for this target is a captured serial sample
where a `player_md3.loop` line reports:

```text
real_md3=1 mock=0 smoke=1/11111 ... delta=<non-zero>/<non-zero> ... exec_fail=0 ... content=<bg>:<non-zero>@...
```

This proves the board is running the shared MD3 Player runtime through the
display/input boundary. It does not yet claim high frame rate, DMA2D
acceleration, storage-backed library scanning, file fonts, host cover decode, or
production touch gesture policy.

## Captured Sample

Captured on 2026-05-25 after flashing `h747_lab_player_md3` with pyOCD:

```text
player_md3 real_md3=1 mock=0 smoke=1/11111 delta=1/1 display=1 sdram1=1/1 rt_store=1 boot=1 render=1 frames=1 present=1 cmd=109/1024 co=0 text=225/4096 to=0 exec_fail=0 content=0xFF101218:381760@0,3-719,1279
```

The same boot log also printed `player_md3: bootstrap ok`,
`player_md3: first render ok`, and the `h747-player-md3>` console prompt.
