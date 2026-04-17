# Charm Signal / State v0

这份文档描述的是 v0 的原语形态、目标与系统关系。

如果你要看允许用法、禁止用法、ISR 边界、生命周期规则与 review checklist，
请优先同时阅读：

- `docs/architecture/signal_state_contract_v0.md`

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

## 执行域定义

这里的“执行域”不是抽象线程模型，而是一个更实用的判断：

- 当前回调能否在这里立即执行完
- 执行期间是否不需要跨 scheduler / reactor / ISR 边界
- 成本和时序是否仍然可由当前调用点承担

在 Charm 里，下面这些通常应视为不同执行域：

- ISR 和 task
- 不同 task
- driver ingress 和 reactor drain
- background worker 和 UI / page controller
- 两个需要显式 submit / wakeup 才能互相到达的执行面

如果拿不准是否同域，默认按“不同执行域”处理，走 `post()`。

## Slot 契约

`signal` / `state.changed()` 的 slot 在 v0 默认遵守同一套硬约束：

- 必须是 `noexcept`
- 必须是有界、可预估的短路径
- 不阻塞
- 不睡眠
- 不等待 IO / lock / condition
- 不做隐藏堆分配
- 不做大循环、大扫描、重计算

更直白一点：

- slot 适合做轻量状态同步、轻量派发、写入局部缓存
- slot 不适合承担完整业务流程、设备访问、UI 重绘流水线、文件系统操作

如果 slot 真的需要“做事”，推荐模式是：

- 先在 slot 内提取最小事件
- 再显式 `post()` 给对应执行面

## ISR 与跨上下文规则

v0 没有提供 irq-safe 的直接广播变体，因此默认规则是：

- 把 ISR 中直接 `emit()` 视为禁止用法

原因不是 `emit()` 本身神秘，而是 slot 集合通常无法证明：

- 每个 slot 都 irq-safe
- 不会触发阻塞路径
- 不会触发 UI / scheduler / service 的错误上下文进入

因此 ISR 中允许做的事情应收敛为：

- 读取并确认硬件状态
- 生成最小边沿事件或状态快照
- 通过 irq-safe 的 `poster` / queue / reactor ingress 显式 `post()`

ISR 中明确不应做：

- 直接 `emit()` 到未知 slot 集合
- 调用 `state::set()`，因为它会继续走 `changed().emit()`
- `connect()` / `disconnect()`
- 借由 `signal` 间接唤起 UI、service 或重逻辑

一句更狠的判断可以写成：

> ISR 里默认只有 `post()`，没有普通 `emit()`。

## 拓扑与生命周期规则

v0 允许动态 connect/disconnect，但不鼓励把它当作系统主拓扑机制。

推荐的连接生命周期：

- 初始化期 connect
- 页面进入 / session 建立时 connect
- 生命周期稳定后只 `emit()` / `set()` / `post()`
- 页面退出 / session 结束时 disconnect

不推荐的用法：

- 在热路径里频繁 connect/disconnect
- 一边 `emit()` 一边修改同一个 `signal` 的连接集合
- 把运行时匿名 connect 网当成系统 wiring

v0 还应默认遵守：

- fanout 顺序不应成为语义契约
- stale token 失效是正常保护，不应绕过
- `signal` / `state` 默认不提供并发安全
- 同一个 `signal` 的 connect / disconnect / emit 不应跨线程或跨 ISR 并发调用

## 事件与状态的选型

这层最容易混淆的不是 API 名称，而是“你手上拿着的到底是什么”。

### 用 `signal`

当你表达的是一次边沿事件：

- button pressed
- track changed
- page entered
- encoder rotated

它关心的是“发生了一次”，而不是“当前真相是什么”。

### 用 `state`

当你表达的是一个当前真相：

- current volume
- current page
- online / offline
- battery percent

它关心的是“现在是什么”，并且允许观察变化。

### 用 `deferred_signal` / `poster`

当你表达的是上下文跨越：

- ISR -> task
- driver ingress -> reactor drain
- background -> UI
- task A -> task B

这时候重点不是“广播”，而是“安全跨域到达”。

## 允许与禁止

推荐用法：

- 页面控制器内部的轻量观察，用 `signal`
- 小型 service 内部的状态真相与变化通知，用 `state`
- task-local deferred delivery，用 `deferred_signal`
- scheduler submit 语义选择，用 `kernel.poster`
- 系统固定 wiring，用 `init.connection`

明确禁止或不推荐：

- 把 `signal` 当跨域消息总线
- 在 ISR 里直接 `emit()` 普通信号
- 在 slot 里做阻塞、睡眠、重 IO、重计算
- 用 `state` 组织自动传播图
- 依赖 slot 顺序表达业务语义
- 把系统 wiring 藏进运行时匿名 connect 关系

## 选型速查

最小判断表可以压成下面几句：

- 同域、边沿、轻量广播：`signal`
- 同域、当前真相、变化通知：`state`
- 跨域、显式延迟投递：`deferred_signal` / `poster`
- 系统级固定拓扑：`init.connection`

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
