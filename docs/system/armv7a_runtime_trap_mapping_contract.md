# ARMv7-A Runtime Trap Mapping

## 文档状态

- `status`: `supporting`
- `scope`: ARMv7-A observation/frame 到 runtime trap ingress 的映射证据
- `ingress contract`: [`minimal_kernel_trap_ingress_contract.md`](minimal_kernel_trap_ingress_contract.md)

本映射分为 host synthetic verifier 和 QEMU leaf。两者不能互相替代。

## Host verifier

`Examples/kernel/runtime_trap_armv7a_host` 使用 synthetic ARMv7-A observation：

```text
Armv7a observation
  -> platform adapter capture
  -> TrapFrameView
  -> RuntimeTrapIngress
  -> TrapResult
  -> platform adapter writeback
```

它验证字段翻译和失败分支，不证明 CPU exception entry、banked register、SPSR 或 exception return。

## QEMU leaf

`Examples/kernel/armv7a/qemu` 提供实际 ARMv7-A firmware 形态的 SVC/trap 路径，包括：

- exception frame/context；
- runtime trap adapter/dispatch/caller；
- task syscall frame/glue/surface；
- roundtrip 与 failure evidence。

定向入口：

```powershell
./Examples/kernel/armv7a/qemu/run_qemu_runtime_trap_ci.ps1
./Examples/kernel/armv7a/qemu/run_qemu_task_syscall_ci.ps1
./Examples/kernel/armv7a/qemu/run_qemu_arch_ingress_seam_ci.ps1
```

QEMU 证明 firmware/exception seam 可以运行，不证明真实 SoC 的中断控制器、MMU/cache 时序或
板级异常现场完全一致。

脚本存在只证明验证入口存在；Host/QEMU 状态以当次输出为准。历史工具链阻断记录在
[`minimal-kernel-runtime-v0`](../archive/minimal-kernel-runtime-v0/README.md)。

## 映射约束

- 平台 adapter 明确指定 service id、argument register、PC/SP/status 和 origin/task 来源。
- Capture 失败、unsupported service 和 writeback 失败保持不同错误。
- Host fixture 的 writeback 仅修改 fixture 定义的结果位置。
- 上层 `TrapRequest` 不包含 ARM 专用寄存器名。
- 新架构必须实现自己的 adapter，不能在 runtime trap 中加入平台条件分支。

## 非目标

- 不冻结产品 SVC ABI。
- 不定义 user/kernel address validation。
- 不把 QEMU 结果声明为真实板电气或时序证据。
