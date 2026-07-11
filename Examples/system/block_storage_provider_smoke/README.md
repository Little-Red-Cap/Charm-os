# block_storage_provider_smoke

This host-only smoke verifies the `block/storage` provider topology candidate on
top of the Capability-Oriented Runtime Topology bridge.

It deliberately keeps all prototype types in `main.cpp`. The smoke is a
semantic proof, not a promoted module, backend contract, Store/FAT/ImageSource
model, manifest, DSL, service locator, or public API.

The smoke checks:

- `BlockDevice` capability kind and `app_store` requirement role are separate
  tokens;
- profile binding targets `host.memory_block_app_store` as a provider instance;
- provider type, adapter, backend, HAL/file token, and `BlockEndpoint` cannot be
  binding targets;
- a provider instance must declare `Provided<BlockDevice, app_store>`;
- profile binding to provider instance and provider publication of
  `BlockEndpoint` are two distinct steps;
- runtime/storage consumer reads through `BlockEndpoint`, not through provider
  identity;
- geometry, counters, last error, readiness, and degraded state remain provider
  evidence, not app/domain API.

This smoke does not replace `device_runtime_block_slot_demo`: that demo proves
runtime attach/detach through a stable block slot, while this smoke proves
provider topology and evidence boundaries.

Build:

```powershell
cmake -S Examples/system/block_storage_provider_smoke `
  -B Examples/system/block_storage_provider_smoke/cmake-build-block-storage-provider-smoke `
  -G Ninja
cmake --build Examples/system/block_storage_provider_smoke/cmake-build-block-storage-provider-smoke
ctest --test-dir Examples/system/block_storage_provider_smoke/cmake-build-block-storage-provider-smoke --output-on-failure
```
