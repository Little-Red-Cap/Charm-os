# Modules/platform

`Modules/platform/` is currently a legacy/prototype area.

It contains useful early platform experiments, Windows host helpers, board stub
modules, and board capability shapes. These files may inform the backend work,
but this directory is not the long-term root of the Charm backend architecture.

New backend contract work belongs under:

```text
Backends/contract/
```

New concrete backend work should be staged under:

```text
Backends/host/
Backends/qemu/
Backends/board/
```

## Compatibility rule

Do not delete or move existing modules from this directory just to introduce the
backend boundary. Existing `system` and `boot` code may still depend on this area
during migration.

New `Modules/system` code should not import concrete backend implementations.
It should depend on backend contracts, capability registry surfaces, or explicit
compatibility layers.

## Promotion rule

Any useful idea from this directory should be promoted intentionally:

1. document the backend contract need under `Backends/contract/`;
2. identify whether the implementation belongs to host, QEMU, or board;
3. keep the old module as compatibility code until callers are migrated;
4. remove or demote the old path only after evidence and build coverage exist.
