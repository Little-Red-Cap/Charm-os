# rte_reflected_context_smoke

This host-only smoke connects the RTE v0 semantic proof with C++ static
reflection. A reflected component spec is the source fact, and the smoke
materializes the same `ComponentDesc + ContextView + EvidenceFrame` semantics
used by the non-reflected RTE smokes.

It deliberately keeps all prototype types in `main.cpp`. The smoke is a
semantic proof, not a promoted public module, manifest, DSL, generator, service
locator, or runtime framework.

The smoke checks:

- `<meta>` is available on the host compiler;
- reflected spec field shape is discoverable;
- reflected component names can feed structured evidence;
- reflected kind/role tokens can describe `Requirement/Provided`;
- missing providers fail at compile time;
- `ContextView` exposes only declared app requirements;
- unbound providers are not selected implicitly.

Build:

```powershell
$env:PATH = "D:/Toolchains/w64devkit/bin;$env:PATH"
cmake -S Examples/system/rte_reflected_context_smoke -B Examples/system/rte_reflected_context_smoke/cmake-build-rte-reflected-context-smoke -G Ninja -DCMAKE_CXX_COMPILER="D:/Toolchains/w64devkit/bin/g++.exe"
cmake --build Examples/system/rte_reflected_context_smoke/cmake-build-rte-reflected-context-smoke
ctest --test-dir Examples/system/rte_reflected_context_smoke/cmake-build-rte-reflected-context-smoke --output-on-failure
```
