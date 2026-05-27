# rte_explain_projection_smoke

This host-only smoke verifies that an explain/report surface is a read-only
projection from an accepted RTE `ResolvedProfile`.

It deliberately keeps all prototype RTE types in `main.cpp`. The smoke is a
semantic proof, not a public module, manifest, DSL, generator, service locator,
runtime framework, or artifact JSON pipeline.

The smoke checks:

- raw `ProfileSpec` is not accepted as an explain projection input;
- invalid profiles cannot materialize a `ResolvedProfile`;
- host and H747 player profiles keep the same four app requirement semantics:
  `TextSink.log`, `Clock.monotonic_time`, `RasterDisplay.primary_display`, and
  `Input.primary_input`;
- provider identity changes only in the explain/profile layer;
- display and input fact fields explain host/H747 differences without entering
  app code or runtime provider instances;
- presentation formatting is a separate read-only step.

Build:

```powershell
cmake -S Examples/system/rte_explain_projection_smoke -B Examples/system/rte_explain_projection_smoke/cmake-build-rte-explain-projection-smoke -G Ninja
cmake --build Examples/system/rte_explain_projection_smoke/cmake-build-rte-explain-projection-smoke
ctest --test-dir Examples/system/rte_explain_projection_smoke/cmake-build-rte-explain-projection-smoke --output-on-failure
```
