# charm_spine_smoke

This host-only smoke is the first executable Charm Spine v0 shape.

It does not promote new public modules. It keeps prototype types in `main.cpp`
and verifies the platform sentence:

```text
Capability -> Component -> Profile -> Projection -> Evidence
```

The smoke checks:

- capabilities are expressed as kind/role requirements and provided tokens;
- components describe system nodes;
- a profile explicitly binds app requirements to provider refs;
- the same component/profile structure materializes an init projection;
- the same profile materializes an app `ContextView`;
- provider evidence is collected as structured facts, not formatted logs.

Build:

```powershell
cmake -S Examples/system/charm_spine_smoke -B Examples/system/charm_spine_smoke/cmake-build-charm-spine-smoke -G Ninja
cmake --build Examples/system/charm_spine_smoke/cmake-build-charm-spine-smoke
ctest --test-dir Examples/system/charm_spine_smoke/cmake-build-charm-spine-smoke --output-on-failure
```
