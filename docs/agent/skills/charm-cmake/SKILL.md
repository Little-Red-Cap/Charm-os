# charm-cmake

> status: `supporting`

Use this skill for CMake targets, presets, toolchains, source ownership and
generated build inputs. The affected project's CMake and presets are the source
of truth; root conventions do not silently override an independent project.

## Before Editing

1. Identify the real configure root, preset/toolchain and explicit target.
2. Read the target's current minimum CMake version. Root and most examples use
   4.0, but some independent targets intentionally use another minimum.
3. Trace source ownership: first-party, generated, vendor, board/BSP or external
   package.
4. Check whether the worktree already has a build directory for that configure
   root; reuse it instead of creating parallel `cmake-build-*` trees.

## Change Rules

- Keep app/service/board/profile source selection with its owning target.
- Do not add a root default dependency merely to make one project convenient.
- Missing generated/vendor/CubeMX input must fail with a path and affected
  target, not at an arbitrary compiler step.
- Preserve toolchain and language ownership; do not set global flags to repair
  one source file.
- For C++ modules, keep FILE_SET/import and scan behavior consistent with the
  target's compiler; do not generalize a local compiler workaround.
- New options need a real consumer, deterministic default and clear scope.
- Build output stays under a reused `cmake-build-*` directory; source trees do
  not receive `build/` directories.

## Validation

Record configure root, preset/options, reused build directory, target and exact
result. Build the narrowest affected target first, then broader dependents when
the ownership boundary changed. A successful configure is not evidence that a
target linked or ran.

Update the nearest README only when target names, prerequisites or usage change;
do not copy complete source lists or generated paths into prose.
