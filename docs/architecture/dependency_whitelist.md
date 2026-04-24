# Dependency Whitelist (Build-time Entry Contract)

Goal: enforce layering rules at configure/build time. Modules should import only entry modules by default and avoid cross-layer direct imports.

## Entry Modules

The whitelist blocks historical top-level entry modules inside `Modules/*`:

- `charm.foundation` — legacy compatibility entry, retained for examples/migration
- `charm.runtime` — legacy compatibility entry, retained for examples/migration
- `charm.domain` — retired historical entry, replaced by `charm.media` + `charm.ui.*`

Do not import them from `Modules/*`. Prefer subsystem entries such as
`charm.core`, `charm.system`, `charm.io`, `charm.net`, `charm.media`,
`charm.ui.ink`, and `charm.ui.vivid`.

## Layering (one-way dependency)

Refer to `docs/architecture_overview.md` for dependency rules.
This whitelist blocks historical compatibility / retired entry modules at build time.

## Exceptions (platform/adapters only)

- `Modules/platform/*`
- `Examples/*` (examples may import directly, but prefer entry modules)

## Execution Rules

1. New modules should not import blocked historical entry modules.
2. If direct import is required, record it in the exception list with rationale.
3. Violations are treated as build failures.

## Checkpoints

- CMake flag: `CHARM_ENABLE_DEPENDENCY_WHITELIST=ON`
- Rule implementation: `cmake/DependencyWhitelist.cmake`

## Examples

### Blocked historical entry modules (`Modules/*` must not use)

```
import charm.foundation;
import charm.runtime;
import charm.domain;
```

## Reference

- Current architecture overview: [`docs/architecture_overview.md`](../architecture_overview.md)
- Driver model: [`driver_model.md`](driver_model.md)
- Historical comparison only: [`docs/reference/vsf/vsf_comparison.md`](../reference/vsf/vsf_comparison.md)

