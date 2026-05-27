# rte_context_slice_smoke

This host-only smoke verifies the RTE v0 `ContextView` slice boundary: one
resolved profile can contain many components and providers, but each app gets
only the capability slice declared by its own requirements.

It deliberately keeps all prototype RTE types in `main.cpp`. The smoke is a
semantic proof, not a public module, manifest, DSL, generator, service locator,
or runtime framework.

The smoke checks:

- one profile can resolve bindings for two app components;
- `UiContext` exposes only UI app requirements;
- `DiagContext` exposes only diagnostics app requirements;
- neither context exposes the whole profile/provider registry;
- unbound providers remain untouched;
- both context slices can share the same clock provider without becoming a
  global world object.

Build:

```powershell
cmake -S Examples/system/rte_context_slice_smoke -B Examples/system/rte_context_slice_smoke/cmake-build-rte-context-slice-smoke -G Ninja
cmake --build Examples/system/rte_context_slice_smoke/cmake-build-rte-context-slice-smoke
ctest --test-dir Examples/system/rte_context_slice_smoke/cmake-build-rte-context-slice-smoke --output-on-failure
```
