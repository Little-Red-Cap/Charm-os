# H747 POSIX Lab

`posix_lab` is the H747 compatibility shell for C/POSIX ELF programs. It is
separate from the resident App ABI used by `app_lab` and `dev_loader`.

```text
POSIX ProgramImage -> spawn -> load_image -> start -> waitpid
```

It validates POSIX `main(argc, argv, envp)` behavior. It does not adapt that
entry into `charm_app_main` or define `CharmAppApi`.

## Monitor Commands

- `elf list`: list embedded POSIX ELF fixtures.
- `elf status`: print runtime, load-region, cwd and PATH diagnostics.
- `elf run <name> [args...]`: run an embedded fixture.
- `elf run-path <path> [args...]`: use the generic path surface.
- `elf smoke`: run the source-defined compatibility subset.

Fixture names and smoke membership are owned by `posix_lab.cpp` and the embedded
ELF build inputs, not duplicated here.

## Owned Boundary

This target checks:

- C/POSIX ELF loading and entry;
- `spawn/load/start/wait` result propagation;
- argv, env, cwd and PATH behavior;
- RAMFS-backed file operations;
- minimal fd, pipe, stat and terminal compatibility used by its fixtures.

It does not own:

- resident App download, Store or image selection;
- `CharmAppApi` or App capability tables;
- Player/display/input policy;
- a product shell, package manager or general process runtime.

## Current Limits

Embedded ELF is the verified path. Generic `elf run-path <path>` keeps a stable
`not_supported` result until a real file-backed executable source is connected.
Do not replace this with fallback to an unrelated embedded fixture.

The load region and cache behavior are H747 project resources. POSIX source,
file or process semantics must not infer success from a successful firmware
build alone.

## Build And Acceptance

Run from `Examples/project/h747-lab` and reuse the configured build directory:

```powershell
cmake --build --preset build-h747-lab-posix-lab-debug -- -j1
```

Minimum board acceptance:

```text
elf list
elf run hello
elf smoke
elf run-path <unsupported-path>
elf status
```

The embedded run and smoke must complete with their expected exit/results; the
unsupported path must remain a non-executing `not_supported` result. Host,
build-only and H747 evidence remain separate.

The current role split is documented in
[`h747_lab_dynamic_boundary_roadmap.md`](../../docs/h747_lab_dynamic_boundary_roadmap.md).
