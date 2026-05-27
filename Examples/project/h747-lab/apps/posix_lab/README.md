# H747 POSIX Lab

`posix_lab` is the H747 compatibility shell for Charm POSIX and C ELF samples.
It is intentionally separate from `app_lab`.

`app_lab` validates the primary resident App ABI:

```c
int charm_app_main(const CharmAppApi* api, int argc, char** argv);
```

`posix_lab` instead validates the POSIX process-oriented line:

```text
spawn / load_image / start / waitpid
```

## Monitor Commands

- `elf list`: list embedded ELF samples
- `elf status`: print runtime status, load region, cwd, and PATH
- `elf run <name> [args...]`: run an embedded ELF sample
- `elf run-path <path> [args...]`: run a file-backed path through the POSIX path surface
- `elf smoke`: run the official Stage-2 sample subset

## Official Sample Set

The embedded sample set currently includes:

- `hello`
- `argv_dump`
- `env_dump`
- `stderr_demo`
- `exit_code`
- `cat_file`
- `write_file`
- `append_file`
- `fd_probe`
- `stat_probe`

This set remains the compatibility regression group. It is not the primary
dynamic app model for H747.

## What It Owns

`posix_lab` is the board-local validation line for:

- C/POSIX ELF execution
- embedded ELF sample loading
- `PATH=/bin:/apps`
- `cwd=/`
- RAMFS-backed file semantics
- `spawn/load/wait` compatibility behavior
- fd / pipe / stat / term minimal semantics

`elf run-path <path>` already freezes the generic path surface and its stable
failure mode when the backend is not connected.

## What It Does Not Own

`posix_lab` does not define:

- the primary resident app model
- `CharmAppApi`
- `charm_app_main`
- the H747 App capability table
- the meaning of `app_lab` image source or QSPI App store behavior

It is a compatibility line, not an upper layer that `app_lab` must inherit.

## Current Limits

- Embedded ELF is the current official path.
- `elf run-path <path>` keeps a stable `not_supported` result until a real
  file-backed executable backend is connected.
- The monitor surface stays text-first and diagnostic-first; it is not a shell
  product or a general package/runtime manager.

## Build

```powershell
cmake --preset h747-lab-debug -DH747_LAB_ARM_GNU_TOOLCHAIN_ROOT="D:/Toolchains/Arm GNU Toolchain arm-none-eabi/latest"
cmake --build --preset build-h747-lab-posix-lab-debug
```

Expected artifacts:

- `cmake-build-h747-lab-debug/h747_lab_posix_lab.elf`
- `cmake-build-h747-lab-debug/h747_lab_posix_lab.bin`

## Acceptance Focus

The current compatibility regression focus is:

- `elf list`
- `elf run hello`
- `elf smoke`
- `elf run-path <path>` returns stable `not_supported` until backend wiring exists

This target should remain isolated from display/player policy and from the
resident App ABI mainline.
