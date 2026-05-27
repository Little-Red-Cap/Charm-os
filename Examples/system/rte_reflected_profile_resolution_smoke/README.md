# rte_reflected_profile_resolution_smoke

This host-only smoke verifies that reflected specs can participate in the RTE
v0 profile resolution boundary. The reflected spec is the source fact, and the
profile must prove every app requirement is bound to exactly one selected
provider that declared a matching reflected capability token.

It deliberately keeps all prototype types in `main.cpp`. The smoke is a
semantic proof, not a public module, manifest, DSL, generator, service locator,
or runtime framework.

The smoke checks:

- `<meta>` is available on the host compiler;
- reflected component, provider, and profile spec field shapes are discoverable;
- reflected `Requirement/Provided` tokens drive profile resolution;
- missing, duplicate, wrong-role, duplicate-provider-token, and stale bindings fail;
- the accepted profile can still materialize a minimal runtime `ContextView`;
- unbound providers are not selected implicitly.

Build:

```powershell
$env:PATH = "D:/Toolchains/w64devkit/bin;$env:PATH"
cmake -S Examples/system/rte_reflected_profile_resolution_smoke -B Examples/system/rte_reflected_profile_resolution_smoke/cmake-build-rte-reflected-profile-resolution-smoke -G Ninja -DCMAKE_CXX_COMPILER="D:/Toolchains/w64devkit/bin/g++.exe"
cmake --build Examples/system/rte_reflected_profile_resolution_smoke/cmake-build-rte-reflected-profile-resolution-smoke
ctest --test-dir Examples/system/rte_reflected_profile_resolution_smoke/cmake-build-rte-reflected-profile-resolution-smoke --output-on-failure
```
