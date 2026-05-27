# charm_spine_reflected_profile_smoke

This host-only smoke verifies the first reflected Charm Spine v0 proof chain:

```text
reflected spec -> profile resolution -> init projection -> ContextView -> evidence projection
```

It deliberately keeps all RTE/Spine/reflection prototype types in `main.cpp`.
The smoke is a semantic proof, not a promoted public module, manifest, DSL,
generator, service locator, or runtime framework.

The smoke checks:

- `<meta>` is available on the host compiler;
- reflected component, provider, and profile spec field shapes are discoverable;
- reflected `Requirement/Provided` tokens drive compile-time profile resolution;
- missing, duplicate, wrong-role, duplicate-provider-token, stale, and extra
  bindings fail at compile time;
- failed profiles produce stable compile-time resolution statuses:
  duplicate provider tag, duplicate provider token, missing binding, duplicate
  binding, extra binding, or invalid binding;
- failed profiles can be projected into diagnostic evidence, but not into
  `init.graph` or app `ContextView`;
- report assembly uses an explicit section order:
  `diagnostics -> selected_providers`;
- each evidence frame carries an explicit section tag, so report ordering and
  frame ownership are both verifiable;
- the smoke exposes two report-construction shapes:
  blocked reports and accepted reports;
- the smoke also exposes matching presentation shapes for blocked and accepted
  reports;
- the smoke keeps matching verifier paths for blocked and accepted reports;
- diagnostic-only profile status evidence is also handled through its own
  construct/present/verify path;
- diagnostics and blocked report paths also use dedicated orchestration helpers,
  while the accepted path still keeps init/projection context visible;
- accepted profile reports include both profile diagnostic evidence and selected
  provider evidence;
- blocked profile reports remain diagnostic-only and never include provider
  evidence;
- only the accepted profile can drive `init.graph` and evidence projection;
- only the accepted profile can materialize app `ContextView` bindings;
- providers outside the selected profile are rejected by compile-time projection
  gates instead of being ignored at runtime;
- evidence is collected as a read-only side-channel after init, not during init
  and not through provider logs.

See also:

- `reflected_profile_report_overview.md` for the unified accepted-vs-blocked
  report semantics frozen by this smoke.

Build:

```powershell
$env:PATH = "D:/Toolchains/w64devkit/bin;$env:PATH"
cmake -S Examples/system/charm_spine_reflected_profile_smoke -B Examples/system/charm_spine_reflected_profile_smoke/cmake-build-charm-spine-reflected-profile-smoke -G Ninja -DCMAKE_CXX_COMPILER="D:/Toolchains/w64devkit/bin/g++.exe"
cmake --build Examples/system/charm_spine_reflected_profile_smoke/cmake-build-charm-spine-reflected-profile-smoke
ctest --test-dir Examples/system/charm_spine_reflected_profile_smoke/cmake-build-charm-spine-reflected-profile-smoke --output-on-failure
```
