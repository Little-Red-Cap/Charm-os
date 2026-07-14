# H747 Lab Capability Contract

> status: `supporting`
>
> authority: project source under `capabilities/` and its real consumers

H747 Lab contains project-local C++ capability experiments. They let source code
depend on a narrow behavior instead of STM32 HAL handles or board identity. They
are not Charm Core, an ELF ABI or proof that the same contract is portable.

## Current Surface

| Header | Project-local surface |
|---|---|
| `status.hpp` | `StatusCode`, `Status`, `Transfer` |
| `stream.hpp` | `ByteSink`, `TextSink`, `LineSource` |
| `time.hpp` | `Milliseconds`, `Clock` |
| `display.hpp` | display value types, `SolidFillDisplay`, `RasterDisplaySink` |
| `input.hpp` | input snapshots/edges, `InputFrameTracker`, `InputSource` |
| `world.hpp` | local composition concepts for log/clock/display/input |

The headers are intentionally free of STM32 HAL dependencies, exceptions,
RTTI, dynamic allocation and required virtual dispatch. Concrete services may
adapt board behavior to these types, but the capability headers do not own
peripheral initialization or lifetime.

## Semantic Boundaries

### Stream and time

- `ByteSink`/`TextSink` report both status and transferred byte count.
- `LineSource::poll_line()` is a non-blocking project surface; empty means no
  complete line, not transport success with fabricated data.
- `Clock` exposes project tick/delay behavior. It does not establish a global
  Charm time service.

### Display

- `SolidFillDisplay` models the minimal fill baseline.
- `RasterDisplaySink` accepts an explicit `SurfaceView` and dirty rectangles.
- framebuffer ownership, SDRAM section, cache maintenance, LTDC/DSI and DMA2D
  stay in the concrete service/backend.
- presentation does not grant UI or rendering code access to HAL state.

### Input

- `InputFrame` is a snapshot of encoder and pointer facts.
- `InputFrameTracker` derives pointer/button edges from successive snapshots.
- touch probe failure may be represented independently from encoder evidence by
  the concrete service.
- gesture, focus, routing and product commands do not belong in this project
  capability header.

### Local world helpers

`world.hpp` groups typed log, clock, display and input instances for local
source composition. `World` is not admitted as a repository-wide runtime model,
registry or App ABI. Consumers may use a smaller explicit constructor or view
when that better expresses ownership.

## Ownership

- board/HAL code owns pins, clocks, IRQ, DMA and vendor handles;
- services own peripheral lifecycle and typed adaptation;
- apps own scenario behavior and domain translation;
- profiles select concrete app/service composition;
- capability headers own only the source-level shape shown above.

Provider names, board facts and diagnostic counters must not leak into generic
App logic merely to satisfy a concept.

## ELF Boundary

C++ concepts are compile-time checks and never cross an independently loaded
image boundary. Resident Apps use the explicit C-compatible
`CharmAppApi`/`charm_app_main` contract described by
[`dev_loader`](../apps/dev_loader/README.md). Source concepts may inform an ABI
capability, but template identity, layout and name mangling are not protocol.

## Promotion Gate

No name in this directory is grandfathered into Charm Core. Promotion requires
a real consumer, stable failure semantics, evidence from more than one runtime
environment, platform-independent meaning and a smaller exception budget than
keeping the type project-local.

Host/QEMU prototype admission follows the project ownership and migration rules in
[`h747_lab_layering_contract.md`](h747_lab_layering_contract.md).
