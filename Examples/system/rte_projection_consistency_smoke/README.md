# rte_projection_consistency_smoke

This host-only smoke verifies projection consistency for RTE v0: init,
context, and evidence projections derived from the same resolved profile must
preserve the same selected provider identity for each requirement.

It deliberately keeps all prototype RTE types in `main.cpp`. The smoke is a
semantic proof, not a public module, manifest, DSL, generator, service locator,
or runtime framework.

The smoke checks:

- a resolved binding has one provider identity;
- init projection reports the same provider identity;
- context projection uses the same provider identity;
- evidence projection reports the same provider identity;
- a mismatched projection provider is rejected at compile time;
- all three projections can still materialize and run independently.

Build:

```powershell
cmake -S Examples/system/rte_projection_consistency_smoke -B Examples/system/rte_projection_consistency_smoke/cmake-build-rte-projection-consistency-smoke -G Ninja
cmake --build Examples/system/rte_projection_consistency_smoke/cmake-build-rte-projection-consistency-smoke
ctest --test-dir Examples/system/rte_projection_consistency_smoke/cmake-build-rte-projection-consistency-smoke --output-on-failure
```
