# rte_component_context_smoke

This host-only smoke verifies the first Charm RTE v0 semantic slice:

- capability kind and role are separate;
- profile binding is explicit;
- `ContextView` exposes only the app requirements;
- component topology can project a minimal init order;
- evidence is structured data, not formatted log text.

It deliberately keeps all prototype types inside `main.cpp`. The example is a
semantic proof, not a promoted public module.

Build:

```powershell
cmake -S Examples/system/rte_component_context_smoke -B Examples/system/rte_component_context_smoke/cmake-build-rte-component-context-smoke -G Ninja
cmake --build Examples/system/rte_component_context_smoke/cmake-build-rte-component-context-smoke
ctest --test-dir Examples/system/rte_component_context_smoke/cmake-build-rte-component-context-smoke --output-on-failure
```
