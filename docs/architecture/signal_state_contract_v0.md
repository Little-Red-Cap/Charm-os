# signal/state Contract v0

这份文档定义 Charm `delegate / signal / state / deferred_signal / poster / connection wiring`
在 v0 阶段的硬边界。

它不是使用技巧汇总，不是示例导读，也不是未来 API 愿景。
它的目标只有一个：

> 把允许用法、禁止用法、ISR 边界、跨上下文规则与生命周期规则冻结成可审查的契约。

配套设计文档：

- `docs/architecture/signal_state_v0.md`

一句话哲学：

> 能当前上下文安全做完的，用 `emit()`。
> 不能当前上下文安全做完的，用 `post()`。

## 1. 核心判断

- `delegate` 是可调用槽的 Foundation 基元。
- `signal` 是同执行域同步广播。
- `state` 是当前真相加变化通知。
- `deferred_signal` / `poster` 是显式跨上下文投递。
- `connection wiring` 是系统级事件拓扑，不是运行时自由订阅玩具。

这套原语的默认目标不是“提高对象间灵活度”，而是：

- 保持成本透明
- 保持时序边界清楚
- 保持 wiring 对 materialize / export / explain 可见

## 2. 四类原语是什么 / 不是什么

### 2.1 `delegate`

是什么：

- 零动态分配
- 不拥有对象
- 不跨上下文
- 只表达“一个可调用槽”

不是什么：

- 闭包系统
- 生命周期管理器
- 调度器
- 自动解绑机制

### 2.2 `signal`

是什么：

- 同域
- 同步
- bounded
- 立即广播

不是什么：

- 队列
- mailbox
- 线程边界桥
- 隐式异步机制

### 2.3 `state`

是什么：

- 当前真相
- 变化通知
- 可观察状态面

不是什么：

- 命令总线
- 事件回放队列
- 响应式图引擎

### 2.4 `deferred_signal` / `poster`

是什么：

- 显式 `post`
- 跨上下文投递入口
- 接到现有执行面

不是什么：

- 自动线程切换魔法
- direct call fallback
- 通用消息总线替身

## 3. 允许用法矩阵

执行域判断规则：

- 如果当前回调不能在这里立即、有界、可预测地完成，就不属于同执行域。
- 如果需要跨 ISR / task / reactor / scheduler submit 边界，就必须视为跨执行域。
- 拿不准时，默认按跨执行域处理。

| 上下文 \\ 操作 | `connect` | `disconnect` | `emit` | `state.set` | `post` |
| --- | --- | --- | --- | --- | --- |
| `init/materialize` | 允许，但仅限局部稳定装配点 | 允许，但仅限局部稳定装配点 | 不推荐，除非明确只是局部同步初始化通知 | 不推荐，除非明确只是局部同步初始化状态 | 不推荐 |
| `task/runloop` | 允许 | 允许 | 允许，但仅限同执行域短路径 | 允许，但遵守 `emit` 同域规则 | 允许 |
| `reactor drain` | 允许，但应克制并保持局部 | 允许，但应克制并保持局部 | 允许，但仅限当前 drain 所在执行域 | 允许，但仅限当前 drain 所在执行域 | 允许 |
| `ISR` | 禁止 | 禁止 | 禁止 | 禁止 | 只允许 irq-safe `poster` / queue / reactor ingress |
| `page/session local` | 允许，推荐作为临时 wiring 主场景 | 允许，推荐用 token 管理 | 允许 | 允许 | 允许 |

补充规则：

- `connect/disconnect` 默认只允许发生在稳定点，而不是热路径。
- `emit` 和 `state.set` 都默认继承“同执行域、同步、有界”的硬规则。
- `post` 是跨上下文的唯一正路，不得再做隐式异步替代。

## 4. 禁止用法

v0 黑名单如下：

- 禁止把 `signal` 当队列用。
- 禁止跨上下文 direct `emit`。
- 禁止在 slot 中阻塞、睡眠、等待 IO、等待锁、做重活。
- 禁止在 ISR 中 `emit` 非 irq-safe 槽。
- 禁止在 ISR 中调用 `state.set()`。
- 禁止把 `state` 当 mailbox、event log 或 replay 队列。
- 禁止在 `emit` 过程中修改同一 `signal` 的连接拓扑。
- 禁止把系统长期 wiring 藏在对象构造函数里偷偷 `connect`。
- 禁止用 runtime 自由订阅图替代 `init.connection / materialize`。
- 禁止 `deferred_signal` 偷做 direct call fallback。
- 禁止把短命对象通过 `delegate` 绑定后越域存活。

这几条里最需要被反复强调的是：

- `signal` 不是异步机制。
- `state` 不是传播图。
- `post` 不能偷偷退化成直接调用。

## 5. 生命周期与所有权规则

`delegate` / `signal` / `state` 在 v0 默认遵守下面这组所有权法律：

- `delegate` 不拥有 target。
- target 生命周期必须覆盖 connection 生命周期。
- `signal::connection` 只是断开句柄，不是所有权句柄。
- v0 不提供自动解绑语义。
- v0 不支持依赖析构副作用的隐式 disconnect。
- 持久 wiring 由 `init.connection` materialize 后长期有效。
- 临时 wiring 仅推荐 page / session local 场景，并由显式 token 管理。
- `connection token` 是唯一合法断开句柄，不支持 `disconnect(slot)` 匹配式断开。

默认推论：

- 如果 target 生命周期不清楚，就不应把它绑定进长期存在的 `signal`。
- 如果 wiring 需要长期稳定存在，就应优先进入 `init.connection` / materialize，而不是运行时匿名 connect 网。

## 6. 与现有执行面的关系

### 6.1 与 `io.reactor`

`io.reactor` 已经是 IO 世界的 deferred primitive。

因此：

- 不要再包一层“通用异步 signal”去模糊它。
- IO ingress / ready / wakeup 问题，优先贴到 reactor 语义上。
- `signal` 不替代 reactor。

### 6.2 与 scheduler submit

现有 scheduler submit 已有三类语义：

- `event`
- `io_ready`
- `demand`

因此：

- 跨上下文时优先贴到这三类 submit 上。
- `poster` 的职责是把“必须 post”明确接到这些提交面。
- `deferred_signal` 的职责只是表达“这里不能 `emit()`，只能 `post()`”。

### 6.3 与 replay / trace / stats

需要下面这些能力的事件：

- replay
- trace
- stats
- budget
- coalesce

应优先走 scheduler / runtime observe 主路径，而不是偷偷埋在 `signal.emit()` 里。

### 6.4 与 `connection wiring`

系统级 wiring 应进入：

- `init.connection`
- materialized graph
- artifact report
- explain surface

而不是只存在对象私有字段或构造过程的匿名 connect 操作里。

### 6.5 与 Vivid object-level widget / SoA SceneAccess

Vivid 当前同时存在两层表面：

- object-level widget 表面
- SoA `SceneBuilder / SceneAccess / SoaKernel` 表面

这两层不是同一件事，禁止混说。

#### object-level widget 表面

典型形态：

- `Checkbox::observe_checked()`
- `Dropdown::observe_selected()`
- `Slider::observe_value()`
- `ProgressBarSimple::observe_value()`

它的语义是：

- 直接绑定 widget 对象实例
- 同执行域、同步、bounded
- 本质上仍然继承 `state<T>` 的契约
- 适合局部 widget 组合、对象级 smoke、非 SoA 小系统

#### SoA `SceneAccess` 表面

典型形态：

- `access.set_value(handle, value)`
- `access.set_checked(handle, on)`
- `builder.set_value(handle, value)`
- `builder.set_checked(handle, on)`

它的语义是：

- 句柄驱动的 runtime / kernel 更新入口
- 服务于 SoA 场景装配、输入、布局、record/execute 边界
- 默认不承诺 object-level widget 的 `observe_*` / 兼容回调语义

硬规则：

- 禁止因为 API 对称性冲动，把 `SceneAccess` 伪装成 object-level `observe_*` 表面。
- 禁止假设 `SceneAccess::set_value()` 自动等价于对象级 widget 的旧 `on_change` 兼容语义。
- SoA 页面/控制器中的跨 widget 关系，应显式写在 controller / app-state / page logic 中，不要偷藏在 kernel 更新接口里。

一句话判断：

- 直接拿 widget 对象时，看 `observe_*`。
- 走 SoA 句柄和 `SceneAccess` 时，看 scene/kernel/runtime 语义，而不是对象级 signal/state 语义镜像。

## 7. 当前 contract smoke 证据

当前仓库里，`signal/state` v0 的 contract smoke 主要靠下面两条示例链冻结：

- `Examples/service/service_signal_state_demo`
  冻结局部 contract：空 delegate 拒绝、固定容量溢出拒绝、stale token 拒绝、`state.set(same)` 不通知、`state.disconnect(token)` 后真相继续更新但观察者静默、`deferred_signal` 保留显式 poster 身份且不 direct call。
- `Examples/system/signal_state_closure_demo`
  冻结跨层 contract：同域 `emit()` 同步落地、`state` 真相只在值变化时通知、`init.connection` 的 direct/deferred wiring 保持 graph 可见、worker 只会在真实 scheduler dispatch 后收到 deferred work。

这两条示例加在一起表达的是：

- local contract 已经可运行、可断言
- cross-layer 边界已经可运行、可断言
- `emit()` 和 `post()` 的职责分界已经不只是文档主张，而是仓库里的执行证据

## 8. v0 审查清单

每次引入或评审一段 signal/state 用法时，至少问下面这几句：

- 这是边沿事件还是状态真相？
- 这是当前上下文可立即完成，还是必须跨上下文？
- 这个连接是临时局部关系，还是系统长期拓扑？
- 有没有隐藏队列或隐式异步？
- 有没有生命周期不清楚的 target？
- 这个事件以后是否需要 replay / trace / stats？
- 这件事是不是本该进 scheduler / reactor，而不是 `signal`？
- 这里到底是 object-level widget 表面，还是 SoA `SceneAccess` / runtime 表面？

如果有任意一条答不上来，默认先不要把它做成 `signal.emit()`。
