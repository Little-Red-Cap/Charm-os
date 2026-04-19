# 最小内核 task syscall frame 契约（草案）

这份文档用于把“最小 syscall handler table 已经存在以后，怎样把一个架构无关的 numbered syscall frame 稳定地接进来”单独收口。

它对应当前新增的：

- `Modules/system/kernel/task_syscall_frame.cppm`

以下内容按当前 `task_syscall_frame.cppm` 与两条 host verifier 重新整理。  
如果文档和实现冲突，优先以代码为准。

目标不是现在就承诺真实 ARMv7-A SVC frame，也不是马上定义完整用户态 syscall ABI，而是先把下面这条链路做成一条薄而稳、可观察、可回归的桥面：

- `frame -> TaskSyscallRequest -> TaskSyscallTable -> TrapResult -> frame writeback`

## 一句话版本

- `TaskSyscallTable` 负责“一个 syscall 号应该落到哪一个 handler”
- `TaskSyscallFrame` 负责“一个最小 numbered frame 怎样 decode、dispatch、再 writeback”

前者是静态表，后者是 frame 桥。

## 为什么现在值得加这一层

当前上半层已经有：

- `TaskSyscallApi`
- `TaskSyscallCatalog`
- `TaskSyscallDispatch`
- `TaskSyscallTable`
- `RuntimeTrapIngress`

这已经足够表达：

- task 侧 syscall-facing 命名
- syscall 号和 trap service 的关系
- request 怎样落到 surface / handler
- trap frame ingress 怎样产出 `TrapFrameView`

但在 `TaskSyscallTable` 和真实 arch frame 之间，还缺一层很关键的“中间桥”：

- 它不直接理解 ARMv7-A 保存布局
- 它也不重新发明第二套 request/result 协议
- 它只负责把一个最小 numbered syscall frame 稳定地收成 `TaskSyscallRequest`

如果这一层不单独收出来，后面不同 host verifier、future arch ingress、future user boundary 很容易又各自维护一份“syscall 号在哪个槽里、参数怎么采、结果怎么回写”的重复逻辑。

## 模块位置与关系

模块位置：

- `Modules/system/kernel/task_syscall_frame.cppm`

当前建议关系是：

1. `kernel.task_syscall_api`
   - task-facing syscall 命名面
2. `kernel.task_syscall_catalog`
   - syscall 号 / trap service / 语义目录
3. `kernel.task_syscall_dispatch`
   - request -> transport / handler surface
4. `kernel.task_syscall_table`
   - syscall 号 -> 静态 handler 表
5. `kernel.task_syscall_frame`
   - numbered frame -> request -> table -> writeback
6. `kernel.runtime_trap_ingress`
   - 真实 trap frame / arch ingress -> `TrapFrameView`

这意味着：

- `TaskSyscallFrame` 不取代 `RuntimeTrapIngress`
- `TaskSyscallFrame` 也不回写 `TaskSyscallTable`
- 它只是把“最小 syscall 号 frame”收成一条独立桥面

## 当前核心类型

当前这层的核心类型与函数包括：

- `TaskSyscallFrameStage`
- `task_syscall_frame_stage_name(...)`
- `TaskSyscallFrameView`
- `task_syscall_frame_view_ready(...)`
- `task_syscall_frame_view_from_request(...)`
- `task_syscall_request_from_frame_view(...)`
- `task_syscall_frame_view_decode(const TrapRequest&, ...)`
- `task_syscall_frame_view_decode(const TrapFrameView&, ...)`
- `task_syscall_semantic_projection(const TaskSyscallFrameView&)`
- `TaskSyscallFrameAdapter<Frame>`
- `task_syscall_frame_adapter_ready(...)`
- `TaskSyscallFrameIngressAdapter<Frame>`
- `task_syscall_frame_ingress_adapter_ready(...)`
- `TaskSyscallFrameTraceEvent`
- `task_syscall_frame_view_from_trace_event(...)`
- `task_syscall_request_from_trace_event(...)`
- `task_syscall_semantic_projection(const TaskSyscallFrameTraceEvent&)`
- `TaskSyscallFrameTraceBuffer<Capacity>`
- `TaskSyscallFrameBridge<Table, Frame, TraceBuffer>`
- `TaskSyscallFramePort<Frame>`
- `TaskSyscallCallFrameAdapter<Frame>`
- `TaskSyscallTrapCallFrameAdapter<Frame>`
- `TaskSyscallFrameCaller<Frame, Tick>`
- `make_task_syscall_frame_ingress_adapter(...)`
- `make_task_syscall_frame_adapter(...)`
- `make_task_syscall_call_frame_adapter(...)`
- `make_task_syscall_frame_bridge(...)`
- `make_task_syscall_frame_port(...)`
- `make_task_syscall_frame_caller(...)`

## 当前最小 frame 视图

`TaskSyscallFrameView` 当前只表达最小 numbered syscall frame 所需字段：

- `syscall`
- `arg0`
- `arg1`
- `arg2`
- `arg3`

它有意不在这一层引入：

- 真实寄存器保存布局
- `TrapOrigin`
- `return_pc / status / stack_pointer`
- 用户态地址空间或指针语义

这些仍然应该继续留给 arch ingress 或 future user ABI。

同时这层已经提供了两条给 future ingress adapter 复用的公共转换：

- `task_syscall_frame_view_decode(const TrapRequest&, ...)`
- `task_syscall_frame_view_decode(const TrapFrameView&, ...)`

它们的意义是把“已经被 lower-half 收成 generic trap request / trap frame view 的东西”进一步稳定地解成 `TaskSyscallFrameView`，而不是让每个 leaf adapter 再各自维护一份 syscall 号映射。

## 当前 ingress / helper 形状

`TaskSyscallFrameIngressAdapter<Frame>` 用来复用已经存在的 `RuntimeTrapFrameAdapter<Frame>`：

- 先把真实或 synthetic frame capture 成 `TrapFrameView`
- 再解成 `TaskSyscallFrameView`
- 最后继续复用原来的 `TrapResult -> frame` writeback

这让 leaf 或 verifier 如果已经持有稳定的 trap adapter，就不需要再手写第二份 syscall-frame capture/apply glue。

当前这层提供的公共拼接件包括：

- `make_task_syscall_frame_ingress_adapter(RuntimeTrapFrameAdapter<Frame>)`
- `make_task_syscall_frame_adapter(RuntimeTrapFrameAdapter<Frame>&)`
- `make_task_syscall_frame_adapter(TaskSyscallFrameIngressAdapter<Frame>&)`
- `make_task_syscall_frame_bridge(table, RuntimeTrapFrameAdapter<Frame>&)`
- `make_task_syscall_frame_bridge(table, RuntimeTrapFrameAdapter<Frame>&, trace)`
- `make_task_syscall_frame_bridge(table, TaskSyscallFrameIngressAdapter<Frame>&)`
- `make_task_syscall_frame_bridge(table, TaskSyscallFrameIngressAdapter<Frame>&, trace)`
- `make_task_syscall_call_frame_adapter(TaskSyscallTrapCallFrameAdapter<Frame>&)`
- `make_task_syscall_frame_caller(port, TaskSyscallTrapCallFrameAdapter<Frame>&)`

其中几条直接接 `RuntimeTrapFrameAdapter<Frame>&` 或 `TaskSyscallFrameIngressAdapter<Frame>&` 的 helper，目的是把“leaf 已经持有稳定 trap adapter / ingress adapter”这条接法再压薄一层。

这里故意只接收 lvalue reference，而不是临时对象。原因很简单：

- `TaskSyscallFrameAdapter<Frame>` 当前仍然通过 `ctx` 指针回指外部 adapter 存储
- 如果 helper 偷偷接受临时 `RuntimeTrapFrameAdapter<Frame>` 或临时 ingress adapter，就会把 `ctx` 指到生命周期已经结束的对象
- 所以这些 glue 明确要求 trap adapter / ingress adapter 的存储由 leaf 或 verifier 自己持有，并且至少活到 bridge 用完为止

## 当前 adapter 责任

`TaskSyscallFrameAdapter<Frame>` 当前只做两件事：

- `capture(ctx, frame, out_view)`
- `apply_result(ctx, frame, result)`

也就是说，这层桥不直接理解具体 frame 结构，而是把：

- 如何从某个 frame 抽取 syscall 编号与参数
- 如何把结果写回某个 frame

都交给 adapter。

这让它天然可以对接：

- host verifier 里的 fake frame
- future arch-neutral stub frame
- 以后真实 ARMv7-A / AArch64 / 其他 leaf target 的 syscall frame adapter

## 当前 task-side caller

`TaskSyscallFrameCaller<Frame, Tick>` 表示 task-side 的最小 caller 闭环，负责把 task-facing syscall facade 变成可走 `TaskSyscallFramePort` 的 numbered frame 调用。

它当前覆盖两类输入：

- `TaskSyscallRequest`
- `TrapRequest`

并提供最小 task-facing 调用面：

- `yield(...)`
- `sleep_until(...)`
- `debug_write(...)`
- `capability_call(...)`

最小调用路径保持为：

- `request -> frame builder -> TaskSyscallFramePort -> TrapResult`

caller 侧新增的 helper 放在这里理解更自然：

- `make_task_syscall_call_frame_adapter(TaskSyscallTrapCallFrameAdapter<Frame>&)`
  - 把 leaf 已稳定提供的 `TaskSyscallRequest -> TrapRequest` glue 和 `TrapRequest -> Frame` builder 组合成 `TaskSyscallCallFrameAdapter<Frame>`。
- `make_task_syscall_frame_caller(port, TaskSyscallTrapCallFrameAdapter<Frame>&)`
  - 在已有 trap-call builder 的前提下，直接生成可绑定到 `bind_runtime(...)` 或 task syscall facade 的 `TaskSyscallFrameCaller`。

这样我们同时覆盖两条证据路径：

- `frame -> request -> table -> writeback`
- `task-side syscall surface -> request -> frame`

## 当前 bridge 责任

`TaskSyscallFrameBridge<Table, Frame, ...>` 当前按固定三段走：

1. `decode`
   - 通过 adapter 把 `Frame` 收成 `TaskSyscallFrameView`
2. `dispatch`
   - 把 `TaskSyscallFrameView` 转成 `TaskSyscallRequest`
   - 再交给 `TaskSyscallTable`
3. `writeback`
   - 把 `TrapResult` 回写到原始 `Frame`

它当前不负责：

- 动态 syscall registry
- 真实 trap frame decode
- trap origin / privilege 解释
- 第二套 errno / negative-return ABI

## 当前错误语义

这层继续直接复用：

- `TrapDisposition`
- `TrapError`
- `TrapResult`

当前最小规则是：

- adapter 未绑定
  - `TrapDisposition::rejected`
  - `TrapError::unbound_adapter`
- frame capture 失败
  - `TrapDisposition::rejected`
  - `TrapError::decode_failed`
- table 未绑定
  - `TrapDisposition::rejected`
  - `TrapError::unbound_bridge`
- result writeback 失败
  - `TrapDisposition::rejected`
  - `TrapError::writeback_failed`

caller 侧也延续同一套错误面：

- builder 未绑定或 caller 未就绪
  - `TrapError::unbound_adapter`
- builder 无法构造 frame
  - `TrapError::decode_failed`
- 可选 `result_ready(...)` 失败
  - `TrapError::writeback_failed`

这意味着 frame bridge 和 caller 都没有引入第二套 frame-specific result 协议。

## 当前 observability

当前 frame bridge 自带独立 trace：

- `TaskSyscallFrameTraceBuffer`

每条 trace 至少记录：

- `sequence`
- `stage`
- `syscall`
- `trap_service`
- `disposition`
- `error`
- `arg0..arg3`
- `value`
- `ok`

并且当前已经支持：

- `task_syscall_frame_view_from_trace_event(event)`
- `task_syscall_request_from_trace_event(event)`
- `task_syscall_semantic_projection(event)`

也就是说，frame trace 和 dispatch/table trace 一样，都能被重新投影回同一套 task syscall 语义字段。

## 与现有层的分工

当前建议这样分：

- `TaskSyscallDispatch`
  - 负责“一个 request 怎样落到一个 surface”
- `TaskSyscallTable`
  - 负责“一个 syscall 号应该落到哪一个 handler”
- `TaskSyscallFrame`
  - 负责“一个 numbered frame 怎样 decode、dispatch、再 writeback”
- `RuntimeTrapIngress`
  - 负责“真实 trap frame 怎样被解释成架构无关 trap 请求”

这几层拆开以后：

- request 语义
- handler table
- numbered frame writeback
- 真实 trap ingress

就不会再被揉回同一个文件里。

## 当前证据路径

当前与这层直接相关的独立证据路径是：

- `Examples/kernel/runtime_task_syscall_frame_host`
- `Examples/kernel/runtime_task_syscall_frame_caller_host`

它们当前覆盖：

- `TaskSyscallFrameView <-> TaskSyscallRequest` 的最小转换
- frame -> table -> handler 的正常闭环
- table 混合挂接 dispatch bridge 与直连 handler
- `RuntimeTrapFrameAdapter<Frame> -> TaskSyscallFrameIngressAdapter<Frame> -> TaskSyscallFrameAdapter<Frame>` 的公共拼接
- `RuntimeTrapFrameAdapter<Frame>& -> TaskSyscallFrameAdapter<Frame>` 的直接 helper
- `RuntimeTrapFrameAdapter<Frame>& / TaskSyscallFrameIngressAdapter<Frame>& -> TaskSyscallFrameBridge` 的直接 helper
- `TaskSyscallApi -> TaskSyscallFrameCaller -> TaskSyscallFramePort -> TaskSyscallFrameBridge -> TaskSyscallTable`
- `TaskSyscallApi -> TaskSyscallFrameCaller -> generic trap frame -> TaskSyscallFrameBridge -> TaskSyscallTable`
- `TaskSyscallTrapCallFrameAdapter<Frame>& -> TaskSyscallCallFrameAdapter<Frame>`
- `TaskSyscallTrapCallFrameAdapter<Frame>& -> TaskSyscallFrameCaller<Frame, Tick>`
- `bind_runtime(...)` 切换不同 caller
- `unbound_adapter`
- `decode_failed`
- `writeback_failed`
- frame trace 的 `stage / semantic projection`
- caller builder 失败与 caller result-ready 失败的负向路径

## 当前非目标

当前这层仍然不处理：

- 真实 ARMv7-A SVC frame 形状
- 真实 trap origin / privilege 解释
- 用户态地址空间与指针校验
- 完整 syscall ABI
- 动态 syscall handler registry

它只是先把“最小 numbered syscall frame bridge”立住，为 future arch ingress 或 user ABI 提供一个更干净的落点。
