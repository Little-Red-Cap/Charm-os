# Minimal Kernel Runtime Trap Contract

## 文档状态

- `status`: `supporting`
- `scope`: `kernel.runtime_trap` 的 request、service、policy、result 与 dispatch
- `authority`: 受 [`CONSTITUTION.md`](../../CONSTITUTION.md) 和
  [`charm_core_contract.md`](../architecture/charm_core_contract.md) 约束

Runtime trap 是 minimal-kernel 的架构无关服务桥，不是 Linux syscall compatibility 或稳定
用户态 ABI。

## 当前服务

| `TrapService` | 数值 | 参数 | 结果名 |
|---|---:|---|---|
| `invalid` | 0 | 无 | `value` |
| `yield_current` | 1 | 无 | `accepted` |
| `sleep_until` | 2 | `arg0=due` | `due` |
| `debug_write` | 3 | `arg0=value` | `bytes-written` |
| `capability_call` | 4 | `arg0=id, arg1=operation, arg2=payload` | `result` |

Catalog 还记录 view kind、参数名称和 supported。未知服务保持 opaque/unsupported。

## Request 与 frame view

`TrapRequest` 和 `TrapFrameView` 共同携带：

```text
service / arg0..arg3 / return_pc / stack_pointer / status /
origin / task / task_valid
```

`TrapOrigin` 当前包括 `kernel_thread`、`user_task`、`supervisor`、`isr`。Runtime bridge 拒绝
ISR origin；如果没有 current task，也返回明确错误。

Typed views 只解释参数：

- `TrapYieldCurrentView`
- `TrapSleepUntilView<Tick>`
- `TrapDebugWriteView`
- `TrapCapabilityCallView`

它们不改变 wire 字段或 service 编号。

## Dispatch

`RuntimeTrapBridge` 使用 `RuntimeTrapPolicy`：

- yield/sleep resume event；
- 可选 sleep event factory；
- 可选 debug write callback；
- 可选 capability call callback。

Dispatch 先验证 origin/current task，再按 service 调用 runtime 或 policy callback。Unsupported
service、未绑定 callback 和 invalid argument 不得映射为成功。

## Result

`TrapResult` 固定包含：

```text
TrapDisposition disposition
TrapError error
u64 value
```

只有 `handled + none` 使 `ok()` 为 true。

当前 error 包括：`no_current_task`、`invalid_origin`、`invalid_argument`、`decode_failed`、
`writeback_failed`、`unsupported_service`、`unbound_bridge`、`unbound_adapter`。

## 分层

```text
arch frame
  -> RuntimeTrapIngress
  -> TrapFrameView / TrapRequest
  -> RuntimeTrapBridge
  -> runtime service or policy callback
  -> TrapResult
  -> arch writeback
```

- ingress 只负责 frame translation；
- runtime trap 只负责服务语义；
- task syscall 可以复用编号和结果，但不拥有 arch ingress；
- 平台 mapping 不得把 synthetic host frame 冒充真实异常现场。

## 证据

- `Examples/kernel/runtime_trap_host`
- `Examples/kernel/runtime_trap_armv7a_host`
- `Examples/kernel/runtime_task_syscall_*`
- `Examples/kernel/armv7a/qemu/run_qemu_runtime_trap_ci.ps1`
- `Examples/kernel/armv7a/qemu/run_qemu_task_syscall_ci.ps1`

这些证据覆盖各自 fixture，不证明完整用户态隔离或 POSIX ABI。

## 非目标

- 不定义 process、privilege transition 或 user memory validation。
- 不定义 signal、errno、restartable syscall 或 blocking ABI。
- 不把 `capability_call` 自动等同于 Charm Capability Contract 调用。
