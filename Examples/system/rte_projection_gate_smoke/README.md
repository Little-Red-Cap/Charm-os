# rte_projection_gate_smoke

This host-only smoke verifies the RTE v0 projection gate: init, context, and
evidence projections must be derived from an accepted `ResolvedProfile`, not
from raw component/provider lists or ad-hoc per-projection facts.

It deliberately keeps all prototype types in `main.cpp`. The smoke is a
semantic proof, not a public module, manifest, DSL, generator, service locator,
or runtime framework.

The smoke checks:

- raw `ProfileSpec` is not a projection input;
- invalid profiles do not produce `ResolvedProfile`;
- init projection only accepts `ResolvedProfile`;
- context projection only accepts `ResolvedProfile`;
- evidence projection only accepts `ResolvedProfile`;
- all three projections preserve the same selected provider identity.

Build:

```powershell
cmake -S Examples/system/rte_projection_gate_smoke -B Examples/system/rte_projection_gate_smoke/cmake-build-rte-projection-gate-smoke -G Ninja
cmake --build Examples/system/rte_projection_gate_smoke/cmake-build-rte-projection-gate-smoke
ctest --test-dir Examples/system/rte_projection_gate_smoke/cmake-build-rte-projection-gate-smoke --output-on-failure
```
