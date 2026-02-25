# Dependency Whitelist (Build-time Entry Contract)

Goal: enforce layering rules at configure/build time. Modules should import only entry modules by default and avoid cross-layer direct imports.

## Entry Modules

- Foundation: `charm.foundation`
- Runtime: `charm.runtime`
- Domains: `charm.domain`

## Layering (one-way dependency)

```
Charm.Foundation  <-  Charm.Runtime  <-  Charm.Domains
```

### Foundation (capability base)

Allowed:
- `import charm.foundation`

Forbidden:
- `charm.runtime` / `charm.domain`
- any `kernel/*` / `fs/*` / `hal/*` / `ui/*` / `audio/*`

### Runtime (system/runtime capabilities)

Allowed:
- `import charm.foundation`
- `import charm.runtime`

Forbidden:
- `charm.domain`
- any direct `ui/*` / `audio/*`

### Domains (feature systems)

Allowed:
- `import charm.foundation`
- `import charm.runtime`
- `import charm.domain`

Forbidden:
- reverse dependency into Foundation/Runtime implementation details

## Exceptions (platform/adapters only)

- `Modules/platform/*`
- `Examples/*` (examples may import directly, but prefer entry modules)

## Execution Rules

1. New modules import entry modules only by default.
2. If direct import is required, record it in the exception list with rationale.
3. Violations are treated as build failures.

## Checkpoints

- CMake flag: `CHARM_ENABLE_DEPENDENCY_WHITELIST=ON`
- Rule implementation: `cmake/DependencyWhitelist.cmake`

## Examples

### Runtime module

```
import charm.foundation;
import charm.runtime;
```

### Domain module

```
import charm.foundation;
import charm.runtime;
import charm.domain;
```

## Reference

- VSF comparison: `docs/vsf_comparison.md`
