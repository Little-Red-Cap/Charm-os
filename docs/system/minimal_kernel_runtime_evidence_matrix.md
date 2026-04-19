# 最小内核运行时证据矩阵

这页不是新契约，也不是重写现有语义，而是把“哪条 seam 由哪个 verifier 证明”对齐到一张表里。

它把两类证据分开：

- 上半层 host / stub 证据：证明运行时语义、命名和最小闭环
- 下半层 ingress / ARMv7-A 映射证据：只在这里做引用，不在本页展开

QEMU 叶子和板级落地仍然是单独的证据线，不属于这页的编辑范围。

## 读法

- `what it proves` 只写这一层已经确认的事实
- `what it does not prove` 专门标出边界，避免把 host 证据误读成 leaf 证据
- `handoff` 说明这层证据继续向上或向下接哪条线

## 证据矩阵

| Layer / seam | Primary boundary | Primary verifier(s) | What it proves | What it does not prove | Handoff |
| --- | --- | --- | --- | --- | --- |
| `RuntimeBridge bind_idle / bind_trace` | `kernel.runtime_bridge` | `Examples/kernel/runtime_bridge_binding_host` | stateful idle/trace rebinding, getter 对齐, trace reroute, fallback idle retarget | 不证明 worker bootstrap、tick drain、ISR defer、thread port | 接 `runtime_run_loop` 与 `RuntimeThreadPort` |
| `RuntimeLoopPort` | `kernel.runtime_bridge` / lower-half loop port | `Examples/kernel/runtime_loop_port_host` | type-erased lower-half loop entry for `advance_tick`、`defer_from_isr`、`bootstrap_idle`、`bootstrap_worker`、`run_once_or_idle` | 不证明 thread-side `yield/sleep`、trap/syscall transport | 接 `runtime_run_loop`、`runtime_tick`、`runtime_isr_defer`、ARMv7-A leaf |
| `bootstrap_idle / bootstrap_worker / run_once_or_idle` | `kernel.runtime_glue` / `kernel.runtime_bridge` | `Examples/kernel/runtime_run_loop_host` | direct idle bootstrap, worker bootstrap, empty-run fallback to idle, invalid idle binding rejection | 不证明 tick drain、ISR defer、thread-side port、trap transport | 接 `runtime_tick`、`runtime_isr_defer`、`RuntimeThreadPort` |
| `runtime_advance_tick` | `kernel.runtime_glue` / `runtime_bridge` | `Examples/kernel/runtime_tick_host` | `advance_tick(now)` 的未到期/到期路径，timer drain，tick trace 对齐 | 不证明硬件 timer、IRQ 路由、trap ingress | 接 `runtime_bridge` 和 `scheduler/timer` |
| `runtime_defer_from_isr` | `kernel.runtime_glue` / `runtime_bridge` | `Examples/kernel/runtime_isr_defer_host` | ISR defer、scheduler post、worker resume 的最小闭环 | 不证明 VBAR/GIC/leaf exception frame | 接 `runtime_bridge` 和 `scheduler/post` |
| `RuntimeThreadPort` | `kernel.runtime_bridge` / `thread port` | `Examples/kernel/runtime_thread_port_host` | `yield_current`、`sleep_current_until`、idle/worker 端口语义 | 不证明 trap/syscall transport | 接 `thread / timer` |
| `RuntimeMailbox` | `kernel.runtime_mailbox` | `Examples/kernel/runtime_mailbox_host` | stateful `send / recv(timeout) / reply`，request/reply queue，timeout cancel，与 `server / client / idle / tick` 的最小 host 闭环 | 不证明 trap/syscall ingress、user ABI、multi-endpoint routing | 接 future task message / syscall surface |
| `TaskMessageApi` | `kernel.task_message_api` | `Examples/kernel/runtime_task_message_host`, `Examples/kernel/runtime_mailbox_host` | `RuntimeMailbox` 之上的 current-task message naming，默认未绑定占位、`bind_mailbox(...)`、task-named `send / receive / reply / wait_*` 入口，以及经由同一 API 驱动的 `server / client / idle / tick` host 闭环 | 不证明 trap/syscall ingress、user ABI、service discovery | 接 `task message table` 与 future IPC / syscall surface |
| `TaskMessageTable` | `kernel.task_message_table` | `Examples/kernel/runtime_task_message_table_host` | server-side `label -> handler` lookup、late bind、handled/unhandled 边界，以及将 mailbox request 收成稳定 table seam 的 host 证据 | 不证明 transport/wait loop/reply send、catalog/discovery、user ABI | 接 future service task routing / IPC protocol |
| `RuntimeTrapServiceFacade` | `kernel.runtime_trap` / `kernel.runtime_service` | `Examples/kernel/runtime_service_host`, `Examples/kernel/runtime_binding_chain_host`, `Examples/kernel/runtime_minimal_host`, `Examples/kernel/runtime_trap_armv7a_host` | task-side trap/service facade 的 `valid()`、`bind_transport(...)`、`yield/sleep/debug/capability` 命名，以及跨 wrapper 链的 transport 重绑定可见性 | 不证明 frame capture / writeback / arch origin | 接 `task runtime API` |
| `TaskRuntimeApi` | `kernel.task_runtime_api` | `Examples/kernel/runtime_task_api_host`, `Examples/kernel/runtime_binding_chain_host`, `Examples/kernel/runtime_minimal_host`, `Examples/kernel/runtime_trap_armv7a_host` | current-task self-service 命名面，`yield()` / `sleep_until(...)` / `debug_write(...)` / `capability_call(...)`，以及 `bind_services(...)` 对顶层 facade 的传播 | 不证明 syscall 编号、dispatch、frame builder | 接 `task syscall API` |
| `TaskSyscallApi` | `kernel.task_syscall_api` | `Examples/kernel/runtime_task_syscall_host`, `Examples/kernel/runtime_binding_chain_host`, `Examples/kernel/runtime_task_syscall_frame_caller_host`, `Examples/kernel/runtime_minimal_host`, `Examples/kernel/runtime_trap_armv7a_host` | syscall-facing 命名面，`sys_*` 语义和 `TaskRuntimeApi` 的分层边界，以及 `bind_runtime(...)` 后顶层 `sys_*` 的切换 | 不证明 catalog / table / frame ingress | 接 `catalog` / `dispatch` |
| `TaskSyscall catalog` | `kernel.task_syscall_catalog` | `Examples/kernel/runtime_task_syscall_catalog_host` | syscall 编号与服务语义的稳定映射 | 不证明具体 handler table 或 frame writeback | 接 `dispatch` |
| `TaskSyscall dispatch` | `kernel.task_syscall_dispatch` | `Examples/kernel/runtime_task_syscall_dispatch_host` | request 到 transport / handler 的稳定分发 | 不证明静态 table 细节 | 接 `table` |
| `TaskSyscall table` | `kernel.task_syscall_table` | `Examples/kernel/runtime_task_syscall_table_host` | syscall number 到 handler 的静态表映射 | 不证明 numbered frame 解析 | 接 `frame` |
| `TaskSyscall frame bridge` | `kernel.task_syscall_frame` | `Examples/kernel/runtime_task_syscall_frame_host` | frame -> request -> table -> writeback 闭环 | 不证明 arch frame 捕获和真实 ingress | 接 `frame caller` 与 `trap ingress` |
| `TaskSyscall frame caller` | `kernel.task_syscall_frame` | `Examples/kernel/runtime_task_syscall_frame_caller_host` | `sys_* -> frame -> table` 的独立 host 闭环 | 不证明真实 leaf frame capture | 接 `trap ingress` |
| `Trap / syscall bridge` | `kernel.runtime_trap` / `kernel.runtime_service` | `Examples/kernel/runtime_minimal_host`, `Examples/kernel/runtime_trap_armv7a_host` | generic host 与 ARMv7-A host 上的 trap/service 证据链 | 不证明真实 leaf frame 的最终写回布局 | 接 `trap ingress` |
| `Trap ingress adapter` | `kernel.runtime_trap_ingress` | `Examples/kernel/runtime_trap_armv7a_host`, `Examples/kernel/runtime_task_syscall_frame_armv7a_host` | frame capture / writeback 的 ingress 入口边界 | 不证明上半层 syscall 语义本身 | 接 ARMv7-A leaf / QEMU 叶子 |

## 这页的边界

- 上半层证据优先由 `Examples/kernel/runtime_*_host` 系列承载
- 下半层 ARMv7-A / leaf 证据只在这里做路由引用
- 新增 seam 时，先补这张矩阵，再补对应 verifier
