# POSIX / ELF Stage 1 QEMU Baseline

This demo is the official QEMU entry for Charm's POSIX / ELF Stage 1 baseline.

The goal of this stage is not to turn Charm into Linux. It is to prove a
repeatable userland execution chain on `QEMU + Cortex-M7 + same-address-space`
 POSIX userland:

- `spawn / spawnp -> resolve image -> load ELF -> start image -> waitpid`
- real static C ELF samples
- automated smoke as the acceptance bar

Current scope:

- mainline POSIX smoke
- real ELF samples
- BusyBox Phase 2 smoke
- separate `newlib stdio` specialty smoke

Out of scope for this stage:

- H747 board execution
- BusyBox expansion
- ModuleX dual-track validation
- `fork`, full signals, dynamic linking, or full Linux parity

Recommended context:

- [`../../../../docs/system/posix_support_overview.md`](../../../../docs/system/posix_support_overview.md)
- [`../../../../docs/system/posix_elf_stage1_baseline.md`](../../../../docs/system/posix_elf_stage1_baseline.md)
- [`../../../posix/elf_samples/README.md`](../../../posix/elf_samples/README.md)

## Run template

```powershell
& "D:\Toolchains\qemu\qemu-system-arm.exe" `
  -M mps2-an500 -cpu cortex-m7 -nographic `
  -kernel path\to\posix-qemu-demo.elf
```

The demo prints `bb2 all ok` on success.

## CI smoke (PowerShell)

Mainline Stage 1 baseline:

```powershell
.\run_qemu_ci.ps1 -ElfPath .\cmake-build-arm3\posix-qemu-demo.elf
```

Dedicated `newlib stdio` specialty smoke:

```powershell
.\run_qemu_stage1_ci.ps1 -Stage stdio -ElfPath .\cmake-build-arm3\posix-qemu-newlib-stdio.elf
```

## Build (Ninja + ARM toolchain)

```powershell
cmake -S . -B cmake-build-debug -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_TOOLCHAIN_FILE=arm-none-eabi-m7.cmake

cmake --build cmake-build-debug -j 8
```

## Stage 1 acceptance shape

The Stage 1 baseline is considered healthy only when:

- `posix-qemu-demo.elf` passes the mainline smoke
- real ELF samples stay on the regular path
- `posix-qemu-newlib-stdio.elf` remains runnable as a dedicated specialty smoke

This means the success bar is automated validation, not a one-off manual demo.
