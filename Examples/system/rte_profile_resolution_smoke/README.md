# rte_profile_resolution_smoke

This host-only smoke verifies the RTE v0 profile resolution boundary: a profile
is not a bag of available providers and it is not a runtime lookup table. It is
an explicit composition conclusion that must prove every app requirement is
bound to exactly one provider that declared a matching capability token.

It deliberately keeps all prototype RTE types in `main.cpp`. The smoke is a
semantic proof, not a public module, manifest, DSL, generator, service locator,
or runtime framework.

The smoke checks:

- every app requirement must have an explicit binding;
- duplicate bindings for the same requirement are rejected;
- duplicate provider declarations for the same capability token are rejected;
- bindings to requirements outside the resolved component are rejected;
- a binding cannot point to a provider that does not declare the matching
  `Provided<Kind, Role>` token;
- stale bindings to providers outside the selected profile are rejected;
- multiple `TextSink` providers are legal only when role and binding make the
  selected provider explicit;
- init, context, and evidence projections are derived from the accepted profile
  resolution result.

Build:

```powershell
cmake -S Examples/system/rte_profile_resolution_smoke -B Examples/system/rte_profile_resolution_smoke/cmake-build-rte-profile-resolution-smoke -G Ninja
cmake --build Examples/system/rte_profile_resolution_smoke/cmake-build-rte-profile-resolution-smoke
ctest --test-dir Examples/system/rte_profile_resolution_smoke/cmake-build-rte-profile-resolution-smoke --output-on-failure
```
