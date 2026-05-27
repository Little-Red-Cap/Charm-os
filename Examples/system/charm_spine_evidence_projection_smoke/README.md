# charm_spine_evidence_projection_smoke

This host-only smoke verifies the missing Charm Spine v0 evidence boundary:
evidence is a read-only side-channel projection derived from the same
component/profile shape, not part of init, runtime scheduling, or provider logs.

It deliberately keeps all prototype types in `main.cpp`. The smoke is a
semantic proof, not a promoted public module, manifest, DSL, generator, service
locator, or runtime framework.

The smoke checks:

- `power_service -> display_service -> app` is still only an init DAG projection;
- evidence collection count stays zero during init;
- evidence is collected after init by a separate collector;
- providers return structured `EvidenceFrame` values instead of formatted logs;
- a separate presentation helper can format evidence without changing provider
  or app state;
- an app component without an evidence producer does not make collection fail.

Build:

```powershell
cmake -S Examples/system/charm_spine_evidence_projection_smoke -B Examples/system/charm_spine_evidence_projection_smoke/cmake-build-charm-spine-evidence-projection-smoke -G Ninja
cmake --build Examples/system/charm_spine_evidence_projection_smoke/cmake-build-charm-spine-evidence-projection-smoke
ctest --test-dir Examples/system/charm_spine_evidence_projection_smoke/cmake-build-charm-spine-evidence-projection-smoke --output-on-failure
```
