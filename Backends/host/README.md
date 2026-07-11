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
