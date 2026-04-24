# 最小内核 task syscall catalog 契约（草案）

这份文档用于把“当前任务看到的最小 syscall surface，怎样进一步收成一份稳定的编号 / catalog / trap 映射表”单独钉住。

它对应当前新增的：

- `Modules/system/kernel/task_syscall_catalog.cppm`

目标不是现在就承诺完整用户态 syscall ABI，而是先把下面三件事之间的关系收清楚：

- `TaskSyscallApi` 的 `sys_*` 命名面
- `TrapService` 的最小 trap/service catalog
- future syscall number / dispatch surface

## 一句话版本

- `TaskSyscallApi<...>` 负责“当前任务怎样发起 syscall-facing 调用”
- `TaskSyscallCatalog` 负责“这些调用在名字、编号和 trap service 上分别对应什么”

前者是调用面，后者是目录面。

## 为什么还要再加这一层

当前上半层已经有：

- `kernel.runtime_service`
- `kernel.task_runtime_api`
- `kernel.task_syscall_api`
- `kernel.runtime_trap`

它们已经能把能力表达清楚：

- `yield`
- `sleep_until`
- `debug_write`
- `capability_call`

但如果我们继续往 future syscall number / dispatch / ABI 方向长，会很快遇到一个边界问题：

- task-side 调用面想保留 `sys_yield()` 这类 syscall-facing 命名
- trap/service 面当前仍然天然更接近 `yield_current` 这类 runtime 语义命名

如果不把这两层之间的对应关系单独收出来，后面无论是做最小 syscall number、trace printer，还是 host verifier，都容易再次各写一份私有映射表。

## 模块位置与关系

模块位置：

- `Modules/system/kernel/task_syscall_catalog.cppm`

当前建议关系是：

1. `kernel.runtime_service`
   - transport -> task-side services
2. `kernel.task_runtime_api`
   - services -> current-task runtime API
3. `kernel.task_syscall_api`
   - runtime API -> current-task syscall-facing naming surface
4. `kernel.task_syscall_catalog`
   - syscall-facing naming surface -> syscall id / trap service / semantic projection

这意味着：

- `TaskSyscallApi` 不需要承担编号和目录责任
- `runtime_trap` 也不需要反过来替 task-side 发明 `yield` 这样的 syscall-facing 名字

## 当前核心类型

当前新增的核心类型与函数是：

- `TaskSyscallId`
- `TaskSyscallViewKind`
- `TaskSyscallCatalogEntry`
- `task_syscall_catalog_entry(TaskSyscallId)`
- `task_syscall_catalog_entry(TrapService)`
- `trap_service_from_task_syscall(...)`
- `task_syscall_from_trap_service(...)`
- `TaskSyscallSemanticProjection`
- `task_syscall_semantic_projection(request)`
- `task_syscall_semantic_projection(trace_event)`

其中 `TaskSyscallCatalogEntry` 当前至少描述：

- `syscall`
- `syscall_name`
- `trap_service`
- `trap_service_name`
- `view_kind`
- `wire_argument_count`
- `wire_argument_names`
- `result_name`
- `supported`

## 当前最小编号面

当前草图先只给最小 syscall surface 编下面几项：

- `TaskSyscallId::invalid = 0`
- `TaskSyscallId::yield = 1`
- `TaskSyscallId::sleep_until = 2`
- `TaskSyscallId::debug_write = 3`
- `TaskSyscallId::capability_call = 4`

它们当前分别映射到：

- `yield` -> `TrapService::yield_current`
- `sleep_until` -> `TrapService::sleep_until`
- `debug_write` -> `TrapService::debug_write`
- `capability_call` -> `TrapService::capability_call`

当前实现里，这几个最小 syscall id 刻意与对应 trap service 保持同号。

但这里的工程约定仍然是：

- 不鼓励调用侧直接做 `static_cast`
- 应该通过 `trap_service_from_task_syscall(...)`
- 或 `task_syscall_from_trap_service(...)`

因为这层的真正价值就是保留“名字和编号可以独立于 trap service 演化”的空间。

## 当前 view 命名边界

当前 `TaskSyscallViewKind` 和 `TrapServiceViewKind` 看起来很像，但职责不同：

- `TrapServiceViewKind::yield_current`
  - 更像 runtime / trap service 的命名
- `TaskSyscallViewKind::yield`
  - 更像 future syscall surface 的命名

这层最重要的 rename boundary 其实就是：

- trap/service 侧保留 `yield_current`
- syscall-facing 侧收成 `yield`

其它几个 service 当前名字暂时基本一致，但它们仍然值得停在独立的 syscall catalog 里，而不是直接把 `TrapService` 当成 future syscall number 使用。

## 当前 catalog 规则

### 1) 已映射的 syscall

对当前已经进入最小 task syscall surface 的项目：

- `task_syscall_catalog_entry(TaskSyscallId::yield)`
- `task_syscall_catalog_entry(TrapService::yield_current)`

都会回到同一份稳定描述，只是一个从 syscall id 查，一个从 trap service 查。

### 2) 未知 syscall id

对未知的 syscall 编号：

- `task_syscall_catalog_entry(static_cast<TaskSyscallId>(99))`

当前会返回：

- `syscall_name = "unknown"`
- `trap_service = TrapService::invalid`
- `view_kind = opaque`
- `supported = false`

这表达的是：

- “这个编号还不在当前最小 syscall surface 里”

### 3) 未映射 trap service

对“trap service 存在，但当前 task syscall surface 还没给它稳定 syscall-facing 名字”的情况：

- `task_syscall_catalog_entry(service)`

当前会返回：

- `syscall = TaskSyscallId::invalid`
- `syscall_name = "unmapped"`
- `trap_service = service`
- `view_kind = opaque`
- `supported = false`

这表达的是：

- trap/service catalog 和 task syscall catalog 不是同一层
- 未来即使 trap 面先长出新 service，也不等于它已经进入当前任务可见的最小 syscall surface

## 当前语义投影

当前 `task_syscall_semantic_projection(...)` 复用现有：

- `trap_semantic_projection(request)`
- `trap_semantic_projection(trace_event)`

也就是说，它不重写参数槽位语义，而是把：

- `TrapRequest` / trap trace event

重新投影成：

- `TaskSyscallCatalogEntry`
- 命名字段列表
- `result_name`

这样 host verifier、trace printer、future syscall number 草图都能复用同一份 task-side 目录视图，而不必重复猜：

- 这个 syscall-facing 名字对应哪个 trap service
- `arg0` 此时应该叫 `due` 还是 `capability-id`

## 与 `TaskSyscallApi` 的分工

当前建议这样分：

- `TaskSyscallApi`
  - 负责 task/worker 代码实际怎么发起 `sys_*` 调用
- `TaskSyscallCatalog`
  - 负责 `sys_*` 调用的编号、命名和 trap service 映射

也就是说：

- `TaskSyscallApi` 更像操作面
- `TaskSyscallCatalog` 更像协议面和目录面

## 当前证据路径

当前与这层直接相关的独立证据路径是：

- `Examples/kernel/runtime_task_syscall_catalog_host`

它当前验证：

- `TaskSyscallId <-> TrapService` 的 round-trip 映射
- `task_syscall_catalog_entry(...)` 的命名与参数目录
- `task_syscall_semantic_projection(...)` 对 request / trace event 的投影

如果当前要继续看“syscall request 怎样被分派到最小 transport / handler surface”，见：

- `docs/system/minimal_kernel_task_syscall_dispatch_contract.md`

如果当前要继续看“syscall 号怎样被组织成静态 handler table”，见：

- `docs/system/minimal_kernel_task_syscall_table_contract.md`

## 当前非目标

当前这层仍然不处理：

- 真正用户态 syscall ABI
- 真正的 trap 入口保存/恢复细节
- syscall dispatch handler 的完整注册机制
- errno / libc facade
- 用户态地址空间和指针校验

它只是把“最小 syscall 编号 / catalog / trap 映射”先稳定下来，为以后继续长 ABI 或 dispatch 面留出干净的落点。
