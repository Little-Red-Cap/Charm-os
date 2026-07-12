# Host Backends

Host backends run Charm on a PC-class operating system.

Their job is to validate Charm system semantics quickly. They are not expected
to model hardware timing, startup state, interrupt controllers, pinmux, or real
board reset behavior.

## v0 responsibilities

- Own host provider instances and their evidence.
- Export a monotonic clock.
- Export a console text sink.
- Export an optional input source.
- Export a loopback byte channel for IO smoke tests.
- Export optional file- or memory-backed block providers for storage smokes.
- Export an evidence summary listing provided and missing facts.

Host provider instances are binding targets; host provider types, adapters,
file handles, OS APIs, and endpoint names are not binding targets.

## Initial migration candidate

Existing `Modules/platform/win` and `platform.board.win_stub` code can become a
future `host.win32` reference backend, but it is not the contract source of
truth.

Linux host support is intentionally out of scope for the first contract pass.

## SDL3 backend v1

`sdl3/` is the first concrete Host execution backend. It provides:

- a monotonic `charm::system::Clock` backed by SDL ticks;
- one SDL event pump that emits existing `input::RawInputEvent` values;
- `RasterDisplay.present()` through an SDL streaming texture;
- a CMake target named `Charm::host-sdl3`.

It does not own Player commands, UI lifecycle, storage, audio, fonts,
screenshots, or product profiles. An application binds the backend clock and
callbacks to the existing `charm::system::RunLoop`.

Run its independent vertical smoke with:

```powershell
.\Backends\host\run-host-sdl3-smoke.ps1
```

Pass `-Sdl3SourceDir <path>` to use an explicit third-party SDL3 checkout;
otherwise the shared SDL3 discovery rules use a configured package, local
source, or the pinned FetchContent fallback. Explicit source wins over a system
package. Pass `-NoFetch` to require an already available dependency.

The stability gate repeatedly opens and closes the backend while exercising
cross-row partial presentation with pixel readback, ordered input bursts, sink
rejection, and event draining:

```powershell
.\Backends\host\run-host-sdl3-stability-gate.ps1 -NoFetch
```

Its defaults run `100` sessions, `30` partial frames per session, and `64`
events per frame. `-Repeat`, `-Frames`, and `-EventBurst` make the same gate
usable for quick checks and longer soak runs. The final output records the SDL3
origin/version plus sessions, presents, event counts, errors, and elapsed time.
The FetchContent fallback is fixed to `release-3.2.8` and verifies its checked
out Git revision before reporting success.

## Reference backend v0

`host_reference.hpp` is the first host backend implementation candidate. It is
header-only and intentionally narrow:

- `host.buffered_console` provides host memory-backed `TextSink.log` and
  `LineSource.shell` behavior for smokes, using the shared
  `Backends/contract/console_output.hpp` candidate vocabulary.
- `host.memory_block_app_store` provides a memory-backed `BlockDevice.app_store`
  behavior for smokes.
- `ReferenceBackend::evidence_view()` exports backend identity, capability
  exports, selected bindings, and required/provided fact summary input through
  `Backends/contract/backend_evidence.hpp`.

It is not a public `Modules/` API, not a service locator, and not a replacement
for future `host.win32` or `host.linux` OS-backed providers.

## Current validation entrypoints

- `Backends/host/reference_smoke`
- `Examples/system/capability_topology_bridge_smoke`
- `Examples/system/console_output_provider_smoke`
- `Examples/system/block_storage_provider_smoke`
