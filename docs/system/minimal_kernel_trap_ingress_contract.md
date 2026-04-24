# 最小内核 Trap Ingress Adapter 契约（草案）

这份文档用于把“真实异常现场怎样翻成 `TrapFrameView`，以及 trap 结果怎样回写到异常现场”这层接口形状写清楚。

它对应当前上半层新增的：

- `Modules/system/kernel/runtime_trap_ingress.cppm`

目标不是提前钉死 ARMv7-A 的真实 trap frame 布局，而是先把下面这件事收成稳定的上半层边界：

- arch/leaf 保存真实异常现场
- ingress adapter 把现场解释成 `TrapFrameView`
- `RuntimeTrapBridge` 处理服务语义
- ingress adapter 把 `TrapResult` 回写回 frame

## 一句话版本

- arch trap handler 负责“拿到真实现场”
- ingress adapter 负责“把真实现场翻译成 runtime trap 可理解的最小视图”

只要这层分工不混，后面 ARMv7-A / 其他架构都能共用同一个 trap runtime，而不用把内核服务语义重新埋回异常处理代码里。

## 当前模块位置

当前相关层次是：

1. `kernel.runtime_glue`
2. `kernel.runtime_bridge`
3. `kernel.runtime_trap`
4. `kernel.runtime_trap_ingress`

其中 `kernel.runtime_trap_ingress` 站在最靠近 arch frame 的位置，但仍然保持架构无关。

## 当前核心类型

### `RuntimeTrapFrameAdapter<Frame>`

它描述“某一种真实 frame 如何接入 trap runtime”：

- `capture(ctx, frame, out_view)`
- `apply_result(ctx, frame, result)`

也就是说，这层强制把 ingress 分成两个动作：

1. `capture`
2. `apply_result`

而不是允许 arch 代码直接绕过 `TrapFrameView` 去调用 trap bridge。

### `RuntimeTrapIngress<TrapBridge, Frame>`

它持有：

- `TrapBridge`
- `RuntimeTrapFrameAdapter`
- 可选 ingress trace buffer

它提供的核心入口是：

- `dispatch(frame)`

这个入口内部固定执行顺序为：

1. 检查 adapter 是否已绑定
2. `capture(frame -> TrapFrameView)`
3. `dispatch_frame(view -> TrapResult)`
4. `apply_result(result -> frame)`

### `RuntimeTrapIngressPort<Frame>`

这是给更下层入口保留的窄 port。

它的目的不是让架构层理解 trap runtime 细节，而只是给它一个单一的：

- `dispatch_frame(frame)`

落点。

### `RuntimeTrapCallFrameAdapter<Frame, Tick>` 与 `RuntimeTrapIngressCaller<Frame, Tick>`

这是一层当前新增的 caller-side 辅助抽象，主要服务于 host/stub 证据路径。

它们的职责不是替代真实 trap ingress，而是把“我要发起一次最小 trap 调用”这件事也收成稳定形状：

- `make_yield_frame(ctx, yield, out_frame)`
- `make_sleep_frame(ctx, sleep, out_frame)`
- `make_debug_write_frame(ctx, write, out_frame)`
- `make_capability_call_frame(ctx, capability, out_frame)`
- 可选 `result_ready(ctx, frame, result)`

其中 `yield` 当前建议直接使用 `TrapYieldCurrentView`，
`sleep` 当前建议直接使用 `TrapSleepUntilView<Tick>`，
`write` 当前建议直接使用 `TrapDebugWriteView`，
而 `capability` 当前建议直接使用 `TrapCapabilityCallView`；
这样 caller-side builder 就不需要在每个 example 里反复裸传 `due`、`value` 或 `capability_id / operation / payload` 这些字段。

在 task-side，这个 caller 还可以继续被 `RuntimeTrapServiceFacade<RuntimeTrapIngressCaller<...>>` 包起来，对 worker 暴露统一的：

- `yield_current(...)`
- `sleep_current_until(...)`
- `debug_write(...)`
- `capability_call(...)`

也就是说：

- ingress adapter 负责“真实或 synthetic frame 怎么被解释”
- caller adapter 负责“最小 trap 调用怎么构造成某一种 frame”

这样 host verifier 就不需要在每个 example 里重复手写一套临时 `RuntimePort` / `yield_current_via_*` / `sleep_current_until_via_*` glue，也不需要把 `RuntimeTrapIngressCaller` 的 transport 形状直接暴露到 worker context。

## 为什么单独需要 ingress 层

当前 `kernel.runtime_trap` 已经解决的是：

- service 编号
- 参数表达
- 最小 trap 语义

但它还不应该直接知道：

- ARMv7-A 的 `lr/spsr/r0-r3`
- 哪一种异常向量会携带 service id
- 返回值应该写回哪一个寄存器
- 返回 PC / status 在哪一个槽位

这些问题天然属于 ingress adapter，而不是 trap service 语义本身。

所以这层的价值就是把：

- “机器现场长什么样”

和

- “内核服务语义是什么”

拆开。

## 当前最小调用顺序

建议的形状是：

1. arch handler 拿到真实 frame
2. 调 `RuntimeTrapIngressPort::dispatch_frame(frame)`
3. ingress adapter 生成 `TrapFrameView`
4. trap bridge 产出 `TrapResult`
5. ingress adapter 回写 frame
6. arch handler 继续按自己的异常返回顺序退出

这条顺序最关键的一点是：

- trap runtime 从不直接接触真实硬件 frame

这样可以保证：

- trap runtime 逻辑可在 host/stub 上验证
- 真实寄存器布局继续停留在 arch/leaf

## 当前错误语义

当前 ingress 层复用 trap 错误面，不再单独发明第二套错误：

- `unbound_adapter`
- `decode_failed`
- `writeback_failed`

这样做的好处是：

- trap 入口、trap service、trap ingress 的失败都能落到同一套 trace / 诊断语言里

## 当前 observability

当前 ingress 模块带有独立的：

- `RuntimeTrapIngressTraceBuffer`

trace 记录的是 ingress 视角下的阶段性动作：

- `decode`
- `dispatch`
- `writeback`

以及对应的：

- service
- origin
- task
- disposition
- error
- `arg0..arg3`
- value

也就是说，ingress trace 现在不只告诉我们“哪个 stage 成功或失败了”，
还保留了足够的 service 参数槽位，因此 host/verifier 侧也可以继续通过：

- `trap_yield_current_view(ingress_trace_event)`
- `trap_sleep_until_view<Tick>(ingress_trace_event)`
- `trap_debug_write_view(ingress_trace_event)`
- `trap_capability_call_view(ingress_trace_event)`

把 decode 出来的最小 service 语义重新投影回来。

这使得我们以后在真实机器上调 trap 路径时，至少可以区分：

- 是 frame 没解出来
- 是 service 没被接受
- 还是结果没能正确回写

## 当前 host 证据

当前 `Examples/kernel/runtime_minimal_host` 与 `Examples/kernel/runtime_trap_armv7a_host` 已经通过共享 caller helper 加各自 frame adapter 验证了：

- `yield_current`
- `sleep_until`
- `debug_write`（generic host 证据）
- `capability_call`（generic host 证据）
- trap result writeback
- ingress `decode_failed`
- ingress `writeback_failed`
- 未绑定 ingress port 时的 `unbound_adapter`

它的意义不是证明“真实 ARMv7-A frame 已经接好”，而是证明：

- ingress adapter 这层抽象本身能成立
- caller-side frame builder 也能复用为同一种最小调用面
- `yield_current / sleep_until` 这类调度语义也能先在 caller 侧收成 view，再翻译成具体 frame
- `debug_write` 这类单字段 trap service 也能在 caller 侧先收成结构化 view，再翻译成具体 frame
- `capability_call` 这类多字段 trap service 也能在 caller 侧先收成结构化 view，再翻译成具体 frame
- trap trace 也能在 host 侧重新投影回这些 service view，而不是只剩一个难解释的 `arg0`
- writeback 这一步不会把 trap runtime 重新耦回 scheduler 内部

进一步说，当前两条证据各自承担的是：

- `runtime_minimal_host`
  - generic `no_current_task / invalid_origin / unsupported_service / decode_failed / writeback_failed / unbound_adapter`
- `runtime_trap_armv7a_host`
  - ARMv7-A `svc observation -> TrapFrameView`
  - invalid mode 经过 ingress 返回 `decode_failed`

并且这两条路径现在都已经开始复用同一个 service catalog，
也就是通过 `trap_service_catalog_entry(service)` 来知道：

- 这个 service 应该投影成哪种 view
- 它声明了几路 wire argument
- 它给这些 wire argument 起什么名字
- 它的结果值应该按什么名字理解
- 它是不是当前最小实现面内的 supported service

进一步说，ingress trace 现在也已经能直接走：

- `trap_semantic_projection(ingress_trace_event)`

所以 host 侧不需要再各自写一套
“如果是 sleep 就打印 due，如果是 capability_call 就打印 cap/op/payload”
的重复 switch 逻辑。

## 对 ARMv7-A 的意义

当前我们没有改动 `targets/armv7a/common` 或 `Examples/kernel/armv7a/qemu` 热区。

但这层 ingress adapter 已经给 ARMv7-A 后续接线预留了非常明确的落点：

- ARMv7-A 真实异常现场继续由 leaf/arch 负责保存
- `svc immediate` / 参数采样继续由 ARMv7-A 契约决定
- 上半层只要求最终提供一个 `capture/apply_result` adapter

也就是说，后面 ARMv7-A 要接进来时，不需要重新设计 trap runtime，只需要把：

- `Armv7aExceptionFrame`
- `Armv7aSvcObservation`

翻成这层要求的 adapter 形状。

## 当前非目标

当前 ingress adapter 仍然不处理：

- 真实 ARMv7-A trap frame 的最终公共契约
- 异常返回的硬件时序
- 用户态地址空间切换
- 完整 syscall ABI
- signal / fault-upcall / 用户进程恢复

这些问题都应在这条最小 ingress seam 站稳后，再由下半层逐步映射进来。
