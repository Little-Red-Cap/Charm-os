# H747 Lab Capability Contract

`h747-lab` uses capability concepts as a trial layer for Charm's future
portable boundaries. The goal is to make app and domain code depend on what a
backend can do, not on which board, HAL handle, or firmware image provides it.

This is a source-level contract. It is not an ELF ABI.

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

## World

A `World` is the app assembly boundary. It groups concrete capability
instances, such as log, clock, and display, without exposing the board or host
implementation behind them.

`RasterDisplayWorld` currently requires:

- a `TextSink` log
- a `Clock`
- a `RasterDisplaySink`
- a framebuffer view owned by the world

Domain apps should prefer `World` concepts over direct service includes. The
same app can then be instantiated with a host/mock world or the H747 DIY board
world.

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
