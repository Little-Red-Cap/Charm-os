# Dependency Whitelist (Build-time Entry Contract)

Goal: enforce layering rules at configure/build time. Modules should import only entry modules by default and avoid cross-layer direct imports.

## Entry Modules

Legacy entry modules have been removed:

- `charm.foundation`
- `charm.runtime`
- `charm.domain`

Do not import them. Use capability entry modules or subsystem entries
(`charm.system`, `charm.io`, `charm.ui.*`, etc.) instead.

## Layering (one-way dependency)

Refer to `docs/architecture_overview.md` for dependency rules.
This whitelist only blocks removed entry modules at build time.

## Exceptions (platform/adapters only)

- `Modules/platform/*`
- `Examples/*` (examples may import directly, but prefer entry modules)

## Execution Rules

1. New modules should not import removed entry modules.
2. If direct import is required, record it in the exception list with rationale.
3. Violations are treated as build failures.

## Checkpoints

- CMake flag: `CHARM_ENABLE_DEPENDENCY_WHITELIST=ON`
- Rule implementation: `cmake/DependencyWhitelist.cmake`

## Examples

### Removed entry modules (do not use)

```
import charm.foundation;
import charm.runtime;
import charm.domain;
```

## Reference

- VSF comparison: `docs/vsf/vsf_comparison.md`

