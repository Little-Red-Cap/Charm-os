# Reflected Profile Smoke

> status: `exploration`
>
> scope: host-only reflection and compile-time rejection experiment

This target keeps all reflected spec, profile, projection and report types in
`main.cpp`. It is not a public module, Charm Core model, manifest, generator,
service locator or runtime framework.

## What The Smoke Checks

```text
reflected local spec
-> compile-time binding checks
-> accepted or blocked result
-> local init/context/evidence projection
```

- `<meta>` can inspect the fixture field shapes on the selected Host compiler.
- missing, duplicate, extra, stale and wrong-role bindings are rejected.
- rejected fixtures receive a deterministic local status such as
  `duplicate_provider_tag`, `duplicate_provider_token`, `missing_binding`,
  `duplicate_binding`, `extra_binding` or `invalid_binding`.
- only an accepted fixture can enter the local init graph and materialize the
  smoke's context view.
- accepted reports order diagnostics before selected-provider evidence.
- blocked reports contain diagnostics only; they do not select a fallback
  provider or emit provider evidence.
- evidence collection is read-only and occurs outside provider logs and init
  control flow.

## Boundaries

The status names, report sections, builders, frame capacities and reflected
types are test fixtures. They do not define a Charm `Profile`, `Provider`,
`ContextView` or evidence ABI. Any production use must start again from a real
consumer, ownership, failure semantics and cross-environment evidence.

Host compiler success does not prove support on QEMU, H747 or another toolchain.
The smoke also does not justify copying reflection into a resident monitor or
dynamic App ABI.

## Validation

Configure, build and run this target only in one existing build directory owned
by the smoke; do not create parallel build trees for repeated checks:

```powershell
cmake -S Examples/system/charm_spine_reflected_profile_smoke -B <cmake-build-dir> -G Ninja -DCMAKE_CXX_COMPILER=<compiler>
cmake --build <cmake-build-dir> -- -j1
ctest --test-dir <cmake-build-dir> --output-on-failure
```

The source and `CMakeLists.txt` are the authority for compiler flags and current
assertions.
