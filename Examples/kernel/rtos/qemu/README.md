# QEMU quickstart (MPS2)

This is a CPU-only validation path for the RTOS scheduler on Cortex-M7.
It does **not** emulate STM32H7 peripherals.

## Run template

```powershell
& "D:\Toolchains\qemu\qemu-system-arm.exe" `
  -M mps2-an500 -cpu cortex-m7 -nographic `
  -kernel path\to\rtos-qemu-demo.elf
```

If `-nographic` is used, UART output will go to the console.

## GDB attach (optional)

```powershell
& "D:\Toolchains\qemu\qemu-system-arm.exe" `
  -M mps2-an500 -cpu cortex-m7 -nographic `
  -kernel path\to\rtos-qemu-demo.elf -S -gdb tcp::1234
```

Then in another terminal:

```text
arm-none-eabi-gdb path\to\rtos-qemu-demo.elf
target remote :1234
continue
```

Suggested watch points:

```text
watch demo::task_a_hits
watch demo::task_b_hits
```

## CI smoke (PowerShell)

```powershell
.\run_qemu_ci.ps1 -ElfPath .\cmake-build-arm3\rtos-qemu-demo.elf
```

## Build (Ninja + ARM toolchain)

```powershell
cmake -S . -B cmake-build-debug -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_TOOLCHAIN_FILE=arm-none-eabi-m7.cmake

cmake --build cmake-build-debug -j 8
```

## Tick output rate

The demo prints `rtos tick` every `g_tick_mod` milliseconds.
Adjust `demo::g_tick_mod` in `main.cpp` to control output frequency.
