# 最小内核 task syscall table 契约（草案）

这份文档用于把“最小 syscall request / dispatch bridge 已经存在以后，怎样再往前收成一张静态 handler table”单独写清楚。

它对应当前新增的：

- `Modules/system/kernel/task_syscall_table.cppm`

目标不是现在就做动态 registry，也不是现在就承诺完整用户态 syscall ABI，而是先把下面这件事变成一条稳定、可验证的最小静态收口面：

- `TaskSyscallId`
- `TaskSyscallRequest`
- `TaskSyscallHandlerEntry`
- `TaskSyscallTable`

## 一句话版本

- `TaskSyscallDispatch` 负责“一个 request 怎样落到一个 transport / handler surface”
- `TaskSyscallTable` 负责“一个 syscall 号应该落到哪一个 handler”

前者是桥，后者是表。

## 为什么现在值得加这一层

当前上半层已经有：

- `TaskSyscallApi`
- `TaskSyscallCatalog`
- `TaskSyscallDispatch`

这已经足够表达：

- 当前任务怎样发起 `sys_*` 调用
- 最小 syscall 编号和 trap service 的关系
- 一个 request 怎样被分派到某个具体 surface

但如果继续往 future syscall table / user boundary 长，还会缺一块很关键的静态组织层：

- 我们需要一张稳定的“号 -> handler”表
- 而不是让更高层自己到处写 `switch`
- 也不是现在就引入动态注册机制

所以当前最健康的下一步，是先落一张最小静态 table。

## 模块位置与关系

模块位置：

- `Modules/system/kernel/task_syscall_table.cppm`

当前建议关系是：

1. `kernel.task_syscall_api`
   - current-task syscall-facing 命名面
2. `kernel.task_syscall_catalog`
   - syscall id / trap service / 语义目录
3. `kernel.task_syscall_dispatch`
   - request -> transport / handler surface
4. `kernel.task_syscall_table`
   - syscall id -> 静态 handler entry / table

这意味着：

- `TaskSyscallDispatch` 不负责“这号应该查哪一项表”
- `TaskSyscallTable` 也不重写 request 到 transport 的参数拼装逻辑

## 当前核心类型

当前新增的核心类型与函数是：

- `TaskSyscallHandler`
- `make_task_syscall_handler(target)`
- `TaskSyscallHandlerEntry`
- `task_syscall_handler_entry(...)`
- `TaskSyscallTableLookup`
- `TaskSyscallTable<Capacity, TraceBuffer>`
- `make_task_syscall_table(...)`

以及一套独立 trace：

- `TaskSyscallTableTraceEvent`
- `TaskSyscallTableTraceBuffer<Capacity>`

## 当前 handler 形状

`TaskSyscallHandler` 当前仍然保持最小：

- `void* self`
- `dispatch_fn(self, request) -> TrapResult`

也就是说，这层仍然继续沿用：

- `TaskSyscallRequest`
- `TrapResult`

而不在 table 层再引入第三套 request/result 协议。

## 当前 entry 形状

`TaskSyscallHandlerEntry` 当前包含：

- `TaskSyscallCatalogEntry descriptor`
- `TaskSyscallHandler handler`

这意味着 table slot 里天然保留了：

- syscall 名字
- trap service 名字
- view kind
- 参数字段名字

所以 table trace、host verifier、future trace printer 不需要再自己维护一份
“slot 2 到底对应 `debug_write` 还是 `sleep_until`” 的私有表。

## 当前 table 责任

`TaskSyscallTable<Capacity, ...>` 当前只做三件事：

1. `lookup(syscall)` 找到静态 slot
2. `dispatch(request)` 把 request 交给对应 handler
3. 记录独立 table trace

它当前不负责：

- 动态注册
- 生命周期管理
- 并发安全
- 用户态 ABI

这些都应该等静态 table 语义稳定以后，再继续往上长。

## 当前 lookup / dispatch 规则

### 1) 找到 slot 且 handler 已绑定

当前行为：

- `matched = true`
- `handler_valid = true`
- 调对应 handler
- 直接返回 handler 的 `TrapResult`

### 2) 找到 slot 但 handler 未绑定

当前行为：

- `matched = true`
- `handler_valid = false`
- 返回：
  - `TrapDisposition::rejected`
  - `TrapError::unbound_adapter`

这表达的是：

- “这个 syscall 号在表里有位置”
- “但这个 slot 还没真正接上 handler”

### 3) 根本没有 slot

当前行为：

- `matched = false`
- `slot = task_syscall_table_unmapped_slot`
- 返回：
  - `TrapDisposition::unsupported`
  - `TrapError::unsupported_service`

这表达的是：

- “这个 syscall 号当前根本不在这张静态表里”

## 当前 observability

当前 table 自带独立 trace：

- `TaskSyscallTableTraceBuffer`

每条 trace 至少记录：

- `sequence`
- `syscall`
- `trap_service`
- `slot`
- `matched`
- `handler_valid`
- `disposition`
- `error`
- `arg0..arg3`
- `value`

并且当前也支持：

- `task_syscall_request_from_trace_event(event)`
- `task_syscall_semantic_projection(event)`

这意味着 table trace 和 dispatch trace 一样，都能被重新投影回同一套 task syscall 语义字段。

## 与现有层的分工

当前建议这样分：

- `TaskSyscallApi`
  - 负责“当前任务怎样调用”
- `TaskSyscallCatalog`
  - 负责“这些调用的编号和名字是什么意思”
- `TaskSyscallDispatch`
  - 负责“一个 request 怎样落到一个 surface”
- `TaskSyscallTable`
  - 负责“一个 syscall 号当前应该连到哪一个 handler”
- `TaskSyscallFrame`
  - 负责“一个 numbered frame 怎样稳定地落到 table，再把结果写回 frame”

这四层拆开以后：

- 名字
- 编号
- request 语义
- handler table

就不会再混回一个文件里。

## 当前证据路径

当前与这层直接相关的独立证据路径是：

- `Examples/kernel/runtime_task_syscall_table_host`

它当前验证：

- table lookup
- table -> dispatch bridge 的最小链路
- table -> 直连 handler 的最小链路
- handler 未绑定与 slot 缺失的负向路径
- table trace 的语义投影

如果当前要继续看“静态 handler table 之上，numbered syscall frame 怎样 decode / writeback”，见：

- `docs/system/minimal_kernel_task_syscall_frame_contract.md`

建议同时结合：

- `docs/system/minimal_kernel_trap_syscall_contract.md`
- `docs/system/minimal_kernel_trap_ingress_contract.md`
- `docs/system/armv7a_runtime_trap_mapping_contract.md`

## 当前非目标

当前这层仍然不处理：

- 动态 syscall handler registry
- per-process / per-namespace handler table
- 真正用户态 syscall ABI
- 用户态地址空间和指针校验
- trap ingress decode / writeback

它只是先把“最小静态 syscall handler table”立住，为 future registry 或 user ABI 提供更干净的落点。
