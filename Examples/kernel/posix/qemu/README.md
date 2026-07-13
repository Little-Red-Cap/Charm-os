# POSIX / ELF Stage 1 QEMU

## 文档状态

- `status`: `supporting`
- `scope`: QEMU MPS2 Cortex-M7 上的 same-address-space POSIX/ELF baseline
- `source`: 本目录 CMake、runner 与 [`POSIX contract`](../../../../docs/system/posix_support_overview.md)

主链为：

```text
spawn/spawnp -> resolve image -> load static C ELF -> start -> waitpid
```

该 target 验证 mainline POSIX、真实 ELF sample、BusyBox phase-2 与独立 newlib stdio fixture。它不是
Linux，不证明 H747、ModuleX、dynamic linking、`fork`、完整 signal 或 Linux parity。Sample 入口见
[`Examples/posix/elf_samples`](../../../posix/elf_samples/README.md)。

## Build 与运行

复用本目录 `cmake-build-arm3`：

```powershell
cmake -S . -B cmake-build-arm3 -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_TOOLCHAIN_FILE=arm-none-eabi-m7.cmake
cmake --build cmake-build-arm3 -- -j1
```

| 范围 | Runner |
|---|---|
| mainline POSIX + BusyBox | `run_qemu_ci.ps1 -ElfPath cmake-build-arm3/posix-qemu-demo.elf` |
| newlib stdio | `run_qemu_stage1_ci.ps1 -Stage stdio -ElfPath cmake-build-arm3/posix-qemu-newlib-stdio.elf` |

通过 `-QemuExe` 传入当前环境的 QEMU executable；README 不保存个人绝对路径。token、timeout、report
与当前状态由 runner 输出维护。

## 验收边界

- mainline runner 必须通过 POSIX 与 BusyBox gate。
- static ELF sample 保持 regular image path。
- newlib stdio 保持独立 specialty smoke，不替代 mainline。
- 自动 smoke 通过只证明 QEMU baseline，不证明真实板或完整 POSIX。
