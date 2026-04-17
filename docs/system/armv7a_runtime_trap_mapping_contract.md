# ARMv7-A Runtime Trap Mapping Contract（host verifier）

这份文档用于把一件事说清楚：

- `Armv7aSvcObservation` / `Armv7aRuntimeTrapObservation`
- 如何被翻译成上半层 `kernel::TrapFrameView`
- 以及当前 host verifier 里的 writeback 到底验证了什么

它对应的证据路径是：

- `Examples/kernel/runtime_trap_armv7a_host/`

它依赖的上下文是：

- `docs/system/minimal_kernel_trap_syscall_contract.md`
- `docs/system/minimal_kernel_trap_ingress_contract.md`
- `targets/armv7a/common/armv7a_runtime_bridge_contract.hpp`
- `targets/armv7a/common/armv7a_runtime_trap_contract.hpp`

## 一句话版本

- ARMv7-A 下半层负责拿到真实 SVC 观察结果
- host/arch adapter 负责把它翻成 `TrapFrameView`
- `RuntimeTrapIngress` 负责跑 `decode -> dispatch -> writeback`
- 当前 writeback 只是 host verifier-local 证据，不等同真实异常返回 ABI

## 当前证据路径的定位

`Examples/kernel/runtime_trap_armv7a_host/` 不是新的 ARMv7-A leaf，也不是 QEMU 替代品。

它的定位更窄：

- 复用 `runtime_minimal_host` 的最小 runtime 闭环
- 只把 trap frame 一层换成 ARMv7-A synthetic observation
- 在不碰 `targets/armv7a/common/` 和 `Examples/kernel/armv7a/qemu/` 热区的前提下
- 先证明 `Armv7aSvcObservation -> TrapFrameView -> TrapResult writeback` 这条 ingress 语义已经闭环

## 当前映射顺序

建议的最小顺序是：

1. 先检查 `armv7a_runtime_trap_ready(observation)`
2. 再用 `armv7a_decode_runtime_bridge_trap(observation.svc)` 解释服务语义
3. 把 ARMv7-A service id 映射为 generic `TrapService`
4. 把 `return_pc / origin_psr / origin mode` 映射进 `TrapFrameView`
5. 把 `TrapFrameView` 交给 `RuntimeTrapIngress`
6. 把 `TrapResult` 回写到 host-local synthetic frame

## 字段映射

| ARMv7-A 观察字段 | 当前上半层落点 | 说明 |
| --- | --- | --- |
| `observation.path` | `armv7a_runtime_trap_ready()` 前置条件 | 当前只接受 `svc_immediate` |
| `observation.service_id` | readiness 校验的一部分 | 必须与 `svc.immediate` 一致 |
| `svc.immediate == 0x43` | `TrapService::yield_current` | 通过 `armv7a_decode_runtime_bridge_trap()` 转义 |
| `svc.immediate == 0x44` | `TrapService::sleep_until` | 同上 |
| `svc.entry.return_pc` | `TrapFrameView.return_pc` | 当前直接透传 |
| `svc.entry.origin_psr` | `TrapFrameView.status` | 当前保留原始 PSR 值 |
| `origin_psr mode == usr (0x10)` | `TrapOrigin::user_task` | 当前 host verifier 允许 |
| `origin_psr mode == sys (0x1f)` | `TrapOrigin::kernel_thread` | 当前 host verifier 允许 |
| `origin_psr mode == svc (0x13)` | `TrapOrigin::supervisor` | 当前 host verifier 允许 |
| 其他 mode | `capture=false` | 当前直接拒绝，不进入 generic trap runtime |
| yield 的 `event_id / event_payload` | `TrapFrameView.arg0 / arg1` | 供 trace 与 policy 校验使用 |
| sleep 的 `due` | `TrapFrameView.arg0` | generic `sleep_until` 当前只消费 `arg0` |
| sleep 的 `event_id / event_payload` | `TrapFrameView.arg1 / arg2` | 供 trace 与 policy 校验使用 |
| `task / task_valid` | 当前留空 | 让 `RuntimeTrapBridge` 继续从当前调度上下文推断任务 |

## 当前 policy 约束

为了让这条证据路径能和现有 host runtime fixture 对齐，当前 verifier 额外检查：

- yield：
  - `event_id == EventId::user1`
  - `event_payload == 1`
- sleep：
  - `event_id == EventId::tick`
  - `event_payload == due`

这不是在说 ARMv7-A ABI 天生如此，而是在说：

- 当前上半层 runtime policy 就是这么接的
- 所以 ARMv7-A synthetic trap 只有满足这组 policy，才算真正接进了现有 runtime 闭环

## 当前 writeback 语义

当前 host verifier 的 writeback 只验证 ingress 这一层有没有把结果带回来。

它回写的是：

- `return_value`
- `TrapError`
- `writeback_seen`

它明确不声称这些内容已经等同于真实 ARMv7-A 异常返回 ABI，例如：

- 返回值最终写哪个寄存器
- SPSR / LR / 通用寄存器恢复顺序
- 异常退出时机与硬件可见状态

这些仍然属于未来 arch/leaf adapter 和真实异常返回路径的职责。

## 当前可观察证据

`Examples/kernel/runtime_trap_armv7a_host/` 当前会给出三类直接证据：

- `armv7a-origin-samples`
  - 验证 `usr / sys / svc` 到 `TrapOrigin` 的映射
  - 验证 `irq` 这类当前不支持 mode 会被 capture 拒绝
  - 验证同一类 invalid mode 经过 ingress 会返回 `decode_failed`
- `trap-trace`
  - 验证 generic runtime 真正看到了 `yield-current / sleep-until`
- `trap-ingress-trace`
  - 验证正常路径上的 `decode -> dispatch -> writeback` 三阶段都走通
  - 验证 invalid mode 会停在 `decode_failed`

## 当前非目标

这份文档当前不定义：

- 真实 ARMv7-A trap frame 公共布局
- 真实异常返回 ABI
- 用户态地址空间切换
- 完整 syscall 编号体系
- fault/upcall/signal 恢复语义

它当前只负责把“ARMv7-A SVC 观察结果怎样进入上半层 trap ingress”这件事先站稳。
