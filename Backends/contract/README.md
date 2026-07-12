# Backend Contract

This directory owns the Charm backend contract.

The contract is intentionally independent from `Modules/platform/`. Existing
`platform.board*` modules may inform this work, but they are not the contract
source of truth.

Start with:

- [`backend_contract_v0.md`](backend_contract_v0.md)
- [`backend_contract_v1.md`](backend_contract_v1.md)
- [`capability_topology.hpp`](capability_topology.hpp)
- [`backend_evidence.hpp`](backend_evidence.hpp)
- [`console_output.hpp`](console_output.hpp)
- [`block_storage.hpp`](block_storage.hpp)
- [`raster_display.hpp`](raster_display.hpp)

## Contract surface

The v0 contract covers the first backend topology ideas:

- backend identity
- capability requirement
- profile binding
- provider type and provider instance
- provider adapter
- capability export
- evidence export
- required/provided/missing facts

It does not define a runtime framework, scheduler, service locator, board
generator, YAML manifest, or CMake preset schema.

`capability_topology.hpp` is the first header-only contract candidate. It only
contains the common topology vocabulary and compile-time checks shared by the
system smokes; console, block, host, QEMU, and board evidence shapes remain
outside this header until they earn their own contract review.

`backend_evidence.hpp` is the first evidence/facts contract candidate. It
contains backend identity, capability export metadata, selected binding
evidence, backend facts, and readiness counters. Domain-specific evidence such
as console TX counters or block geometry still belongs outside this common
header.

`console_output.hpp` is the first v1 candidate slice extracted from the
host-only console smoke. It defines the shared `ByteSink`, `TextSink`,
`LineSource`, console roles, transfer/status result, and structured console
provider evidence vocabulary. H747 status-line presentation remains outside the
contract header.

`block_storage.hpp` is the second v1 candidate slice extracted from the
host-only block/storage smoke. It defines the shared `BlockDevice`, storage
roles, provider-published `BlockEndpoint`, status result, and structured block
provider evidence vocabulary. Store v1, FAT paths, ImageStore, and ResourcePack
remain outside the contract header.

`raster_display.hpp` is the third v1 candidate slice. It defines only a
bounded read-only pixel surface, explicit packed pixel formats, dirty region,
clipping, present result, and the `RasterDisplay.primary` requirement shape.
Window systems, textures, scaling policy, cache maintenance, frame scheduling,
screenshots, and application commands remain backend or application concerns.

## Validation entrypoints

The current contract candidate is validated by contract-local header smokes,
host/QEMU/board reference smokes, and the first host-only system smokes before
it is promoted further:

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

Run the current Backends v1 gate with:

```powershell
.\Backends\run-backends-v1-smoke.ps1
```
