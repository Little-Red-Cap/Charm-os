# ARMv7-A QEMU Bare-metal Leaf

> `status: supporting`

本目录提供 QEMU `virt` / Cortex-A7 的 ARMv7-A bare-metal machine evidence，不是 Linux image，也不证明
RK3506 或其它真实 SoC 的 clock、DDR、GIC、cache timing 与外设行为。当前事实以 `CMakeLists.txt`、
`CMakePresets.json`、runner 和当次日志为准。

## 入口

- [`run_qemu_ci.ps1`](run_qemu_ci.ps1)：复用 `debug` preset 和 `out/build/debug`，完成 configure、串行
  build、QEMU 运行与 token gate。
- [`run_qemu.ps1`](run_qemu.ps1)：用 `-device loader` 运行已有 bare-metal ELF；可通过
  `-WaitForGdb -GdbPort <port>` 等待调试器。

具体 QEMU 参数、默认 ELF、timeout、token 和专项 runner 只由脚本与 CMake 维护，不在 README 复制。

## 边界

startup、linker、vector、mode stack、PL011、GIC/timer、MMU/cache 和 runner 属于本 QEMU leaf。
可复用 frame、handoff 和 trap adapter 位于
[`targets/armv7a/common`](../../../../targets/armv7a/common/)；平台责任见
[`armv7a_platform_contract.md`](../../../../docs/system/armv7a_platform_contract.md)，trap 映射见
[`armv7a_runtime_trap_mapping_contract.md`](../../../../docs/system/armv7a_runtime_trap_mapping_contract.md)。

Handoff evidence 不定义产品 image、slot、signature 或真实板 jump policy。专项 runner 也不能代替默认
runtime evidence bundle。

Host verifier 只证明字段映射，不证明 exception entry；QEMU 只证明可仿真的 CPU/firmware 行为，不证明
真实 SoC。preset、script 或 token 的存在也不代表当前绿色，证据以当次退出码和日志为准。
