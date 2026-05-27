# rte_multi_role_provider_smoke

This host-only smoke verifies the RTE v0 multi-role provider boundary: one
provider may satisfy multiple roles of the same capability kind, but every role
must still be declared and bound explicitly.

It deliberately keeps all prototype RTE types in `main.cpp`. The smoke is a
semantic proof, not a public module, manifest, DSL, generator, service locator,
or runtime framework.

The smoke checks:

- one `shared_console` provider can declare both `TextSink.log` and
  `TextSink.debug_trace`;
- app requirements bind `log` and `debug_trace` explicitly, even when both
  resolve to the same provider identity;
- a provider that declares only `TextSink.log` cannot satisfy
  `TextSink.debug_trace`;
- `ContextView` keeps role-specific access paths instead of exposing a generic
  `TextSink` lookup;
- evidence reports both role bindings as structured facts.

Build:

```powershell
cmake -S Examples/system/rte_multi_role_provider_smoke -B Examples/system/rte_multi_role_provider_smoke/cmake-build-rte-multi-role-provider-smoke -G Ninja
cmake --build Examples/system/rte_multi_role_provider_smoke/cmake-build-rte-multi-role-provider-smoke
ctest --test-dir Examples/system/rte_multi_role_provider_smoke/cmake-build-rte-multi-role-provider-smoke --output-on-failure
```
