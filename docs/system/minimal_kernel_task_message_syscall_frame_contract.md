# 最小内核 task message syscall frame transport 契约（草案）

这份文档用于把上半层第二条更强的 `message runtime -> syscall semantic` 焊缝收成稳定 seam。

它对应当前新增的：
- `Modules/system/kernel/task_message_syscall_frame.cppm`

目标不是现在就发明完整 user/kernel ABI，而是先把“多参数 syscall 怎样穿过 message runtime”这件事用一个显式、可验证、可继续接 lower-half 的 v1 transport 固定下来。

## 一句话版本

- message 侧不再把 `label/value` 直接硬投影成 syscall 参数
- message 固定承载一个协议 label 和一个 frame token
- 真正的 syscall frame 由上半层 frame store 以 `(from, token)` 作为键持有
- server 收到 message 后，先解 token，再把 frame 交给现有 `task_syscall_frame` 闭环
- reply 表示“frame 已完成处理且结果已经稳定可观察”，不等于“syscall 一定成功”

也就是说，这一层证明的是：

“message runtime 已经可以稳定承载一个显式的 syscall frame transport，而不是继续把 message bridge 偷偷长成隐式 ABI。”

## 当前模块形状

当前模块导出：
- `task_message_syscall_frame_request_label`
- `task_message_syscall_frame_request_label_name()`
- `make_task_message_syscall_frame_request(...)`
- `task_message_syscall_frame_token(...)`
- `TaskMessageSyscallFrameStore<Frame, Capacity>`
- `TaskMessageSyscallFrameResultAdapter<Frame>`
- `TaskMessageSyscallFrameBridgeResult`
- `TaskMessageSyscallFrameBridgeTraceEvent`
- `TaskMessageSyscallFrameBridgeTraceBuffer<Capacity>`
- `TaskMessageSyscallFrameBridge<...>`
- `make_task_message_syscall_frame_bridge(...)`

## v1 transport 规则

当前 v1 约定如下：

1. request 的 `label` 固定使用 `task_message_syscall_frame_request_label`
2. request 的 `value` 固定承载 frame token
3. frame store 以 `(from, token)` 查找 frame，避免跨 task 混淆 token
4. bridge 只负责 `token -> frame -> frame port`，不直接解释具体 syscall 语义
5. 真正的 syscall decode / dispatch / writeback 继续由 `task_syscall_frame` 和其下游 table/dispatch 负责

## reply 语义

这一层最重要的语义选择是：

- reply 表示“frame processing completed and result is ready to inspect”
- 它不等于“syscall succeeded”

因此当前有三种关键结果：

1. frame 找到，frame port 完成 writeback，结果适配器确认结果已稳定
   这时返回 `handled_task_message(result.value)`，即使结果本身是 `unsupported` 或其他失败语义
2. frame token 缺失，或找不到对应 frame
   这时保持 `handled=false`、`replied=false`，由 client 侧 timeout 观察
3. frame 找到，但 frame port 未绑定或结果尚未 ready
   这时同样不伪装成成功 reply，而是通过 trace 暴露边界

这条规则把两类事情明确分开了：

- “传输是否成功闭环”
- “syscall 语义是否成功”

## 与现有层的关系

建议把这条上半层链理解成：

1. `kernel.runtime_mailbox`
2. `kernel.task_message_api`
3. `kernel.task_message_table`
4. `kernel.task_message_dispatch`
5. `kernel.task_message_service_loop`
6. `kernel.task_message_service_drain`
7. `kernel.task_message_service_pump`
8. `kernel.task_message_syscall_frame`
9. `kernel.task_syscall_frame`
10. `kernel.task_syscall_table`
11. `kernel.task_syscall_dispatch`

其中：

- `task_message_service_*` 继续负责 server task 的 wait/drain/pump 编排
- `task_message_syscall_frame` 负责 message transport 与 frame store/token 解析
- `task_syscall_frame` 负责 frame decode / dispatch / writeback 闭环

## 当前 live 证据路径

当前与这层直接相关的 live 证据路径是：

- `Examples/kernel/runtime_task_message_syscall_frame_host`
- `Examples/kernel/runtime_task_syscall_frame_host`
- `Examples/kernel/runtime_task_syscall_frame_caller_host`
- `scripts/minimal_kernel_runtime_host_smoke.ps1`

其中新的 `runtime_task_message_syscall_frame_host` 当前证明：

- `TaskMessageServicePump` 驱动的真实 server task 可以消费 tokenized syscall-frame request
- `yield`、`sleep_until`、`capability_call` 三类 frame 能经由 message runtime 进入既有 syscall frame bridge
- `capability_call` 的 `arg0/arg1/arg2` 可以完整穿过 transport
- invalid syscall frame 会得到“失败结果已写回”的 reply，而不是被错误归类为 timeout
- missing token 仍然保持为无 reply，由 timeout 明确暴露

## 当前非目标

当前这层仍然不处理：

- 真实 arch leaf frame 的捕获与写回
- user/kernel ABI
- 可变长 payload 或零拷贝传输
- 多 endpoint/service discovery
- 跨 task frame 共享策略

后续如果继续往下焊，更健康的方向是：

- 往下接真实 `trap ingress adapter`
- 再把 arch leaf frame 捕获/写回对到这条 frame transport seam

而不是继续把单字 message bridge 塞成越来越隐式的 ABI。
