# rte_evidence_slice_smoke

This host-only smoke verifies the RTE v0 relationship between per-component
`ContextView` slices and profile-wide evidence collection.

It deliberately keeps all prototype RTE types in `main.cpp`. The smoke is a
semantic proof, not a public module, manifest, DSL, generator, service locator,
or runtime framework.

The smoke checks:

- UI and diagnostics apps receive separate `ContextView` slices;
- evidence is collected through a separate profile-wide side channel;
- providers hidden from an app context can still publish evidence;
- apps do not receive an evidence collector through their context;
- presentation formatting is separate from evidence collection;
- evidence collection does not mutate provider/app execution counters.

Build:

```powershell
cmake -S Examples/system/rte_evidence_slice_smoke -B Examples/system/rte_evidence_slice_smoke/cmake-build-rte-evidence-slice-smoke -G Ninja
cmake --build Examples/system/rte_evidence_slice_smoke/cmake-build-rte-evidence-slice-smoke
ctest --test-dir Examples/system/rte_evidence_slice_smoke/cmake-build-rte-evidence-slice-smoke --output-on-failure
```
