# Charm Signal / State v0

## 目标

`signal_state_v0` 定义的是 Charm 在 Foundation 层的最小事件连接原语。

它不是 Qt 式统一大框架，也不是新的事件系统，更不是对象化消息总线。
它要解决的是更克制的问题：

- 如何在同一执行域内做强类型、固定容量、零动态分配的同步广播
- 如何把“立即调用”和“延迟投递”明确区分开
- 如何让系统级事件关系继续保持对 `init.graph` / materialize / system compiler 可见

一句话哲学：

> 能当前上下文安全做完的，用 `emit()`；不能当前上下文安全做完的，用 `post()`。

## 非目标

v0 明确不做以下事情：

- 不做动态内存分配
- 不做运行时反射、RTTI 或元对象系统
- 不做隐式跨上下文调度
- 不做隐藏队列语义
- 不做无上限 observer list
- 不做全局自由订阅图
- 不把 `io.reactor`、scheduler submit 或 run loop 包装成新的统一 runtime 框架

## 四个原语

### 1. `util.delegate`

`delegate` 是 Foundation 层最小可调用基元。

它只表达：

- 一个 `ctx`
- 一个 `stub`
- 一个 `noexcept` 调用约束

它不表达：

- 连接管理
- 生命周期拥有
- 线程安全
- 调度语义

### 2. `service.signal`

`signal` 是同执行域、同步、固定容量的广播器。

v0 约束：

- 强类型签名
- 固定容量 slot
- `O(N)` 分发
- 无动态分配
- 广播签名不接受 `T&&`；需要共享较重载荷时使用值或 `const T&`
- 不阻塞
- 不睡眠
- 不做重活

它只解决“当前上下文里通知多个观察者”，不解决跨上下文传递。

### 3. `service.state`

`state<T>` 表示“带真相存储的状态单元 + 变化通知”。

v0 只提供：

- `get()`
- `set()`
- `changed()`

其中：

- `set()` 只在值真的变化时通知
- `changed()` 底层仍然是 `signal`
- `state<T>` 不是响应式框架，不做自动传播图

### 4. `service.deferred_signal`

`deferred_signal` 不是“排队版 signal”，而是显式 `post()` 语义壳。

它只表达：

- 这个事件不能 `emit()`
- 它必须交给外部 poster 去 `post()`

v0 不把 scheduler / reactor / run loop 适配强绑进 Foundation。
真正的 poster 适配器应靠近各自子系统落地。

## 三条硬规则

### 1. `emit()` 只在同执行域

`emit()` 只能用于当前上下文立即调用 slot 的场景。

典型适用：

- 同一线程 / 同一 run loop
- UI 内部
- 组件内部轻量状态通知
- 不跨 ISR / task / reactor 的局部协作

### 2. `post()` 只用于跨上下文

只要事件要跨 ISR / task / reactor / scheduler 边界，就必须显式走 `post()`。

禁止把“直接调用”和“延迟投递”做成隐式可切换语义。

### 3. 大拓扑交给 `init.graph` / system compiler

系统级 wiring 不是运行时自由订阅图。

Charm 更适合的模型是：

- 拓扑在系统描述里可见
- 构建期 / 配置期确定关系
- 运行时只 materialize

局部 `signal` 可以动态 connect/disconnect，但默认是弱动态、强静态。

## 与现有系统的关系

### 与 `io.reactor`

`io.reactor` 已经是 IO 世界的 deferred primitive。

它的硬规则是：

- `notify()` 只入队，不执行重处理
- `drain()` 在 task context 做 budgeted 消费

因此：

- `signal` 不替代 `io.reactor`
- `deferred_signal` 也不包装 `io.reactor` 成新的“通用 signal”
- 当问题本质上是 IO ready / ISR deferral 时，应优先继续使用 `io.reactor`

### 与 scheduler submit 语义

SSU 当前已经收口三类提交入口：

- `event-submit`
- `io-ready-submit`
- `demand-submit`

因此：

- `deferred_signal` 只表达“必须 post”
- 真正 post 到哪一类 submit，由外部 poster 决定
- runtime 侧可以用 `kernel.poster` 显式适配成 `event` / `io_ready` / `demand` 三类 poster
- `charm.system.app_host` 可以继续把这些 poster materialize 成面向具体 task 的 typed handle
- 当上层需要一次拿到某个 task 的全部提交面时，可优先使用 `kernel.poster::poster_set` / `AppHost::posters<Task>()`
- 需要 replay / trace / stats 的事件，应优先走已有 scheduler submit 主路径

### 与 replay / trace

`signal` v0 不内建 replay / trace。

原因不是它们不重要，而是：

- 同域同步广播本身只是局部连接原语
- 系统级可观测执行事件，应该优先投到已有 scheduler / trace 主路径

### 与 `init.graph` / materialize

`signal_state_v0` 不替代装配图。

未来系统级 wiring 更适合被表达为：

- `init.connection` 这类最小 `source / sink / mode` 描述
- materialized connection
- system compiler 可生成的结构化拓扑

而不是让大拓扑隐式藏在对象内部 connect 里。

当前仓库里，这一层已经以 `init.connection` 的最小形式落地：

- `direct_connection(...)`
- `deferred_connection(...)`
- materialize 后可见 `kind=connection`

它不是运行时“自由订阅图”，而是让静态 wiring 关系继续留在 `init.graph` / materialize 可见面上。

## v0 边界

v0 第一版只落以下内容：

- `util.delegate`
- `service.signal`
- `service.state`
- `service.deferred_signal` 的最薄语义壳

v0 暂不做：

- 优先级 slot
- 自动去重
- 递归保护
- 并发安全
- ISR-safe 直接广播变体
- 自动 wiring 生成
- scheduler / reactor 专用 adapter 框架

## 使用建议

推荐：

- 页面内部、组件内部、状态变化通知，使用 `signal` / `state`
- ISR -> task、driver -> service、background -> UI，使用显式 `post()`
- 系统级固定连接关系，继续交给 `init.graph` / materialize

不推荐：

- 在 ISR 里直接 `emit()` 非 irq-safe 槽
- 用 `signal` 承担跨调度域通知
- 用 `state` 承担自动传播图
- 把系统主拓扑写成运行时匿名 connect 网
