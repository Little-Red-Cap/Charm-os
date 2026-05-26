# rte_init_projection_smoke

This host-only smoke verifies the first projection bridge from RTE v0 component
topology into the existing Charm `init::Node` / `init::Graph` contract.

It deliberately keeps the RTE prototype types local to `main.cpp`. The example
is a semantic proof, not a promoted public module, manifest format, generator,
or runtime framework.

The smoke checks:

- `ComponentDesc.name` projects to `init::Node.name`;
- component phase projects to `init::Phase`;
- component provides/requires project to `init::CapId` arrays;
- component init entry projects to `init::Node.init`;
- `power -> display -> app` is an init DAG projection, not a runtime event
  topology;
- duplicate providers, missing requirements, and phase inversions are rejected
  by the existing `init::Graph` rules.

Build:

```powershell
cmake -S Examples/system/rte_init_projection_smoke -B Examples/system/rte_init_projection_smoke/cmake-build-rte-init-projection-smoke -G Ninja
cmake --build Examples/system/rte_init_projection_smoke/cmake-build-rte-init-projection-smoke
ctest --test-dir Examples/system/rte_init_projection_smoke/cmake-build-rte-init-projection-smoke --output-on-failure
```
