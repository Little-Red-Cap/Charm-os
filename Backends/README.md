# Charm Backends

`Backends/` is the home for Charm backend contracts and backend-oriented
integration material.

This directory exists to keep the backend boundary separate from
`Modules/platform/`, which currently contains early platform experiments and
legacy adapters. New backend work should start here unless a narrower document
explicitly says otherwise.

## Terms

- `backend`: the execution resource domain for Charm, such as host, QEMU, or a
  real board. A backend owns provider instances, adapters, and downstream
  HAL/OS/simulator/file/syscall dependencies.
- `capability`: an app/domain-consumable semantic contract, such as `TextSink`,
  `LineSource`, or `BlockDevice`.
- `requirement`: an app/component declaration that it needs a capability role.
- `binding`: a profile result that maps a requirement to a provider instance.
- `provider instance`: the concrete provider selected by a profile binding.
- `provider type`: a provider implementation family. It is metadata, not a
  binding target.
- `adapter`: provider-internal mechanism translation. It is not the architecture
  boundary.
- `BSP`: the board support package for a real board. It owns board facts such as
  clocks, pinmux, memory regions, UART routes, IRQ wiring, and vendor SDK
  bindings.
- `target`: a build leaf that selects toolchain, architecture, backend, and
  optional board/profile details.
- `platform`: a historical Charm area. It may contain useful prototypes, but it
  is not the backend architecture root.

## Layout

```text
Backends/
|- contract/   # backend identity, capability export, evidence, and facts
|- host/       # PC host backend notes and reference providers
|- qemu/       # QEMU backend notes and reference evidence
`- board/      # real-board BSP notes and reference evidence
```

## Rules

- Backend contract work belongs in `Backends/contract/`.
- Applications declare capability requirements.
- Profiles bind requirements to provider instances.
- Providers adapt backend resources into stable capability contracts.
- Host, QEMU, and real-board implementations must depend on the contract, not on
  each other.
- New `Modules/system` code must not import concrete backend implementations.
  It may depend only on backend contracts, capability registry surfaces, or
  existing compatibility layers during migration.
- Bindings must not target provider types, adapters, transports, HAL handles, or
  endpoint names.
- `Endpoint` is not the provider-instance identity. Endpoint-like objects may be
  provider-published consumption surfaces for narrower domains such as block or
  stream.
- `targets/` remains the place for concrete build leaves such as `rk3506`.

## Current status

Backend Contract Candidate v1 now contains the v0 topology/evidence contract
plus three extracted capability slices: `console/output`, `block/storage`, and
the minimal `raster/display` candidate. It still does not promote these
candidates to `Modules/` or make a concrete backend part of Charm core.

`Backends/host/sdl3` is the first real Host execution provider. It owns SDL
lifecycle, one event pump, the monotonic clock adapter, and raster presentation.
Applications consume its neutral projections; SDL window and event types remain
inside the backend.

## v1 smoke gate

Run the current backend contract candidate smokes with:

```powershell
.\Backends\run-backends-v1-smoke.ps1
```

The gate builds and runs:

- `Backends/contract/topology_header_smoke`
- `Backends/contract/evidence_header_smoke`
- `Backends/contract/console_output_header_smoke`
- `Backends/contract/block_storage_header_smoke`
- `Backends/contract/raster_display_header_smoke`
- `Backends/host/reference_smoke`
- `Backends/qemu/reference_smoke`
- `Backends/board/reference_smoke`
- `Examples/system/capability_topology_bridge_smoke`
- `Examples/system/console_output_provider_smoke`
- `Examples/system/block_storage_provider_smoke`

The SDL-backed vertical smoke has a separate gate because it requires SDL3 and
a usable Host video environment:

```powershell
.\Backends\host\run-host-sdl3-smoke.ps1
```

Longer Host lifecycle, frame, and input pressure is kept out of the portable
contract gate and runs through:

```powershell
.\Backends\host\run-host-sdl3-stability-gate.ps1 -NoFetch
```
