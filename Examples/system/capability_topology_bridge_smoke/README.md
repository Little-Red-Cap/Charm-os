# capability_topology_bridge_smoke

This host-only smoke verifies the candidate Capability-Oriented Runtime Topology
bridge before it becomes a public backend contract.

It deliberately keeps all prototype types in `main.cpp`. The smoke is a
semantic proof, not a promoted module, manifest, DSL, generator, service
locator, backend contract, or public API.

The smoke checks:

- capability kind and requirement role are separate tokens;
- a profile binding targets a provider instance, not a provider type, adapter,
  HAL/backend token, or `BlockEndpoint`;
- a provider instance must declare a matching `Provided<Capability, Role>`;
- one provider instance may provide multiple roles, but each role must be
  explicitly declared and bound;
- duplicate bindings for the same requirement are rejected;
- provider metadata and evidence remain explain/projection data, not app
  dependencies;
- `BlockEndpoint` is a stable block consumption surface published by a provider,
  not a provider instance identity.

Build:

```powershell
cmake -S Examples/system/capability_topology_bridge_smoke `
  -B Examples/system/capability_topology_bridge_smoke/cmake-build-capability-topology-bridge-smoke `
  -G Ninja
cmake --build Examples/system/capability_topology_bridge_smoke/cmake-build-capability-topology-bridge-smoke
ctest --test-dir Examples/system/capability_topology_bridge_smoke/cmake-build-capability-topology-bridge-smoke --output-on-failure
```
