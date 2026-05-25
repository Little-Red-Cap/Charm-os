# H747 Lab Capability Contract

`h747-lab` uses capability concepts as a trial layer for Charm's future
portable boundaries. The goal is to make app and domain code depend on what a
backend can do, not on which board, HAL handle, or firmware image provides it.

This is a source-level contract. It is not an ELF ABI.

The current platformization roadmap is documented in
`docs/architecture/rte_to_h747_platform_roadmap.md`. In that roadmap, H747 Lab is
the real-board pressure field for the `RTE -> H747` line, and `Display + Player`
is the first vertical slice that must prove host/mock and H747 providers can
carry the same app semantics.

## Contract Home

The first capability headers live under `capabilities/` in this project and use
the `charm::cap` namespace. They are intentionally header-only C++ contracts:

- no virtual dispatch
- no exceptions or RTTI
- no dynamic allocation
- no STM32 HAL dependency
- no root-level Charm module promotion yet

When these contracts survive PC, mock, and H747 backends, they can be promoted
into a shared Charm module with their names and semantics preserved.

## Stream

`ByteSink` is the smallest output capability:

- `write(std::span<const std::byte>) -> Transfer`
- `flush() -> Status`

`TextSink` extends it with:

- `write(std::string_view) -> Transfer`

`LineSource` is the non-blocking shell/input side:

- `poll_line() -> std::optional<std::string_view>`

The H747 USART1 console currently adapts to these contracts through
`h747::console::ConsoleStream` and `h747::console::ConsoleLineSource`.

## Display

`SolidFillDisplay` is the current proven H747 display capability. It is enough
to express the HX8394D red-screen baseline:

- `mode() -> DisplayMode`
- `fill_solid(Argb8888) -> Status`

`RasterDisplaySink` is the future UI boundary:

- `mode() -> DisplayMode`
- `present(SurfaceView, std::span<const Rect>) -> Status`

UI and rendering code should target `RasterDisplaySink` once framebuffer
transport is restored. The current `MinimalPanel` deliberately does not satisfy
`RasterDisplaySink`, because the verified red-screen path is LTDC
background-only and does not yet accept a framebuffer.

The current raster board line is the `display_raster` service. `h747_lab_player_md3`
renders the shared MD3 `PlayerRuntime` into a borrowed SDRAM-backed
`PlayerDisplaySurface`, then presents it through the H747 raster display sink.
That is the first evidence that Player rendering can target a surface/sink
boundary instead of SDL, Win32, Linux, or LTDC directly. LTDC layer setup,
buffer ownership, cache/DMA policy, and future DMA2D acceleration remain
display-service or board-adapter responsibilities, not Player app contracts.

## Input

`Input` should follow the same rule as `Display`: app and domain code must
depend on input facts, not on STM32 timers, GPIOs, I2C handles, or touch IC
driver details.

The first trial contracts live in `capabilities/input.hpp`:

- `EncoderSample`
- `PointerSample`
- `InputFrame`
- `InputSource`

These contracts deliberately describe snapshots and simple facts only:

- encoder detent delta and press state
- pointer detected/down state and coordinates
- touch detection is allowed to fail independently from encoder evidence
- no HAL handle exposure
- no UI routing, gesture semantics, or board-specific timing assumptions

`h747_lab_input_probe` is the board truth target for this contract. It reports
GT970/GT9xx I2C4 probe status, TP_INT/TP_RST levels, touch samples, encoder
detents, and encoder button state. Player-facing code should consume the
resulting `InputFrame` rather than reaching into `services/input` internals.

`h747_lab_player_md3` is the first app bridge that consumes these facts for a
real UI runtime. Its app-local adapter maps GT970 touch samples to
`PlayerInputEvent::Pointer`, encoder1 detents to `Up/Down`, encoder2 detents to
`Prev/Next`, and encoder buttons to `Enter/PlayToggle`. Console commands may
inject the same semantic Player commands for board bring-up, but page/controller
code must still see only Player input events, not HAL, I2C, TIM, GPIO, or console
implementation details.

## World

A `World` is the app assembly boundary. It groups concrete capability
instances, such as log, clock, and display, without exposing the board or host
implementation behind them.

`RasterDisplayWorld` currently requires:

- a `TextSink` log
- a `Clock`
- a `RasterDisplaySink`
- a framebuffer view owned by the world

`InputWorld` adds:

- an `InputSource`

`RasterDisplayInputWorld` is the intended shape for full UI-facing apps once
H747 input transport is wired into a concrete world.

Domain apps should prefer `World` concepts over direct service includes. The
same app can then be instantiated with a host/mock world or the H747 DIY board
world.

`apps/player` follows this rule through a small `PlayerViewModel` and
`PlayerRuntime`: application state is advanced outside rendering, while the
domain renderer only consumes the model and a `RasterDisplayWorld`. This keeps
the current Player shell free of host-only assumptions such as file fonts,
cover decoding, dynamic allocation, exceptions, and direct HAL access.

`apps/player_md3` applies the same rule to the shared desktop MD3 Player: the
app-local layer assembles clock, storage defaults, SDRAM render surface, raster
sink, input bridge, and console diagnostics, while the visual source of truth
remains `Examples/project/player/app-vivid-MaterialDesign3`. It must not fork
the UI into a hand-drawn board mock path.

## Board Evidence

Real-board acceptance evidence for the MD3 Player bridge is emitted over the
serial console. A healthy `h747_lab_player_md3` status line must show:

- `real_md3=1 mock=0`
- `smoke=1/11111`
- non-zero `delta=<frames>/<presents>` on loop status lines
- increasing `frames=<n>` and `present=<n>`
- non-zero `content=<bg>:<non_bg>@<min>-<max>`
- `exec_fail=0`, `co=0`, and `to=0`

`smoke=1/11111` means the current sample window has passed boot, render,
present, content, and scene-execution checks. This evidence is intentionally
app-local: it proves the board is running the shared MD3 Player runtime through
the display/input boundary, not a replacement probe UI.

## ELF Boundary

C++ concepts are compile-time constraints. They are the right shape for source
collaboration, PC validation, mock tests, and zero-cost board backends.

They must not be used directly as a hot-load or independent ELF boundary.
Cross-ELF code should instead receive a stable capability table or hostcall
table that mirrors these semantics with an explicit ABI. In Rust terms, the
concepts are closer to generic trait bounds; the future ELF boundary is closer
to an explicit `dyn Trait` vtable owned by Charm.

The first development goal is therefore to reduce how much code needs board
flashing. A resident monitor and ELF loader can come later, after the source
capability boundaries are stable.
