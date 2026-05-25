# rte_profile_materialization_smoke

This host-only smoke verifies the next RTE v0 semantic bridge: the same
component/profile description can produce both an init projection and an app
`ContextView` projection.

It deliberately keeps all prototype RTE types in `main.cpp`. The smoke is a
semantic proof, not a public module, manifest, DSL, generator, service locator,
or runtime framework.

The smoke checks:

- component topology projects to the existing `init::Node` / `init::Graph`;
- profile bindings explicitly choose providers for app requirements;
- `ContextView` exposes only the app requirements;
- host and mock-MCU providers can materialize the same app semantics;
- multiple `TextSink` providers cannot be chosen implicitly;
- init projection and context projection are separate outputs of the same RTE
  composition boundary.

Build:

```powershell
cmake -S Examples/system/rte_profile_materialization_smoke -B Examples/system/rte_profile_materialization_smoke/cmake-build-rte-profile-materialization-smoke -G Ninja
cmake --build Examples/system/rte_profile_materialization_smoke/cmake-build-rte-profile-materialization-smoke
ctest --test-dir Examples/system/rte_profile_materialization_smoke/cmake-build-rte-profile-materialization-smoke --output-on-failure
```
