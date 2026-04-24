# POSIX BusyBox Phase 2 smoke (QEMU)

This demo validates the minimal POSIX wrapper path with a BusyBox-like
Phase 2 smoke set on MPS2/QEMU.

## Run template

```powershell
& "D:\Toolchains\qemu\qemu-system-arm.exe" `
  -M mps2-an500 -cpu cortex-m7 -nographic `
  -kernel path\to\posix-qemu-demo.elf
```

The demo prints `bb2 all ok` on success.

## CI smoke (PowerShell)

```powershell
.\run_qemu_ci.ps1 -ElfPath .\cmake-build-arm3\posix-qemu-demo.elf
```

## Build (Ninja + ARM toolchain)

```powershell
cmake -S . -B cmake-build-debug -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_TOOLCHAIN_FILE=arm-none-eabi-m7.cmake

cmake --build cmake-build-debug -j 8
```
