# Minimal Kernel Trap Ingress Contract

## 文档状态

- `status`: `supporting`
- `scope`: platform frame 与 `kernel.runtime_trap` 之间的 adapter
- `runtime contract`: [`minimal_kernel_trap_syscall_contract.md`](minimal_kernel_trap_syscall_contract.md)

Ingress 只翻译 frame 和回写结果，不定义 trap service 或真实架构 frame layout。

## Adapter

平台提供 `RuntimeTrapFrameAdapter<Frame>`：

```text
capture(ctx, const Frame&, TrapFrameView&) -> bool
apply_result(ctx, Frame&, const TrapResult&) -> bool
```

两个函数都存在时 adapter 才 ready。

## Dispatch 顺序

`RuntimeTrapIngress::dispatch(frame)` 固定执行：

1. 检查 adapter；缺失返回 `unbound_adapter`。
2. Capture `TrapFrameView`；失败返回 `decode_failed`。
3. 调用 `RuntimeTrapBridge::dispatch_frame()`。
4. Apply result；失败返回 `writeback_failed`，并保留原 result value。
5. 返回 runtime trap result。

Trace stage 固定为 `decode`、`dispatch`、`writeback`，同一次尝试使用同一 sequence。

## Port 与 caller

- `RuntimeTrapIngressPort<Frame>` 擦除具体 ingress 类型，只暴露 `dispatch_frame`。
- `RuntimeTrapCallFrameAdapter<Frame, Tick>` 构造 yield/sleep/debug/capability frame，并可检查结果。
- `RuntimeTrapIngressCaller` 是 host/测试调用 facade，不代表 CPU 执行了真实 trap 指令。

Call adapter 的最小 ready 条件只要求 yield/sleep builder；debug/capability builder 是否存在由对应
调用分支检查。

## 失败边界

- Adapter validity 不证明 frame 内容有效。
- Decode 成功不证明 service supported。
- Runtime dispatch 成功后 writeback 仍可能失败。
- Trace/witness 只记录当前 ingress 尝试，不修复平台 frame。
- Synthetic caller 不能替代 exception entry/return 证据。

## 证据

- `Examples/kernel/runtime_trap_armv7a_host`
- `Examples/kernel/runtime_task_syscall_frame_armv7a_host`
- `Examples/kernel/armv7a/qemu/run_qemu_arch_ingress_seam_ci.ps1`

ARMv7-A 字段映射见
[`armv7a_runtime_trap_mapping_contract.md`](armv7a_runtime_trap_mapping_contract.md)。
