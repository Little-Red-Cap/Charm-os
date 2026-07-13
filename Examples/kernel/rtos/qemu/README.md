# RTOS QEMU MPS2

## 文档状态

- `status`: `supporting`
- `scope`: Cortex-M7 CPU-only scheduler/IRQ evidence
- `source`: 本目录 CMake、`main.cpp` 与 [`run_qemu_ci.ps1`](run_qemu_ci.ps1)

该 target 在 QEMU `mps2-an500` 验证 scheduler、tick、timeout 与 ISR/task 边界，不模拟 STM32H7
peripheral。

## Build 与运行

复用本目录 `cmake-build-arm3`：

```powershell
cmake -S . -B cmake-build-arm3 -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_TOOLCHAIN_FILE=arm-none-eabi-m7.cmake
cmake --build cmake-build-arm3 -- -j1
```

运行 `run_qemu_ci.ps1 -ElfPath cmake-build-arm3/rtos-qemu-demo.elf`，并通过 `-QemuExe` 传入当前环境的
QEMU executable。runner 要求 tick、timeout 和 ISR/task violation check 同时通过；具体 token、timeout
与当前状态由脚本维护。

## 调试与边界

手工 QEMU 可加 `-S -gdb tcp::<port>`，再由 ARM GDB 加载同一 ELF symbols。`demo::g_tick_mod` 只控制
观测输出频率，不改变 scheduler tick source。

QEMU green 只证明 CPU/scheduler fixture，不证明 STM32H7 NVIC、timer、clock 或真实板时序。
