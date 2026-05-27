# rte_profile_selection_smoke

This host-only smoke verifies the RTE v0 profile selection boundary: the same
app requirements can materialize against different host/mock and board/H747
providers without changing app semantics.

It deliberately keeps all prototype RTE types in `main.cpp`. The smoke is a
semantic proof, not a public module, manifest, DSL, generator, service locator,
or runtime framework.

The smoke checks:

- `display_demo` declares only `TextSink.log`, `Clock.monotonic_time`, and
  `Display.primary_display` requirements;
- the host profile binds those requirements to `host_log`, `host_clock`, and
  `host_framebuffer`;
- the board profile binds the same requirements to `h747_uart_log`,
  `h747_systick_clock`, and `h747_dsi_display`;
- the same `app_tick` template runs on both `ContextView` shapes and produces
  the same app-level semantic result;
- provider identity changes are visible only through profile/evidence facts,
  not through app code;
- a mixed profile that binds only part of the provider set is rejected.

Build:

```powershell
cmake -S Examples/system/rte_profile_selection_smoke -B Examples/system/rte_profile_selection_smoke/cmake-build-rte-profile-selection-smoke -G Ninja
cmake --build Examples/system/rte_profile_selection_smoke/cmake-build-rte-profile-selection-smoke
ctest --test-dir Examples/system/rte_profile_selection_smoke/cmake-build-rte-profile-selection-smoke --output-on-failure
```
