# Signal / State 局部契约 v0

状态：supporting contract。

本文件只约束当前已实现的 `util::delegate`、`service::signal`、
`service::state` 与 `service::deferred_signal`。它们是局部连接原语，
不是 Charm Core 身份，也不定义系统级事件框架。

源码入口：

- `Modules/core/util/delegate.cppm`
- `Modules/core/service/signal.cppm`
- `Modules/core/service/state.cppm`
- `Modules/core/service/deferred_signal.cppm`

可运行证据：

- `Examples/service/service_signal_state_demo`

历史设计讨论已归档到
[`../archive/signal-state-v0/`](../archive/signal-state-v0/README.md)，不作为当前 API 事实。

## 一句话边界

> 当前上下文能够同步、有界地完成时使用 `emit()`；需要跨上下文时，通过明确的
> poster 使用 `post()`。

## 已实现语义

### `util::delegate<Args...>`

- 保存一个不拥有的 `ctx` 与一个 `noexcept` stub。
- 可绑定满足约束的自由函数或成员函数。
- 空 delegate 调用不产生效果。
- 不管理 target 生命周期，也不提供自动解绑。

target 必须比所有保存或调用该 delegate 的对象活得更久。

### `service::signal<void(Args...), MaxSlots>`

- 固定容量，`MaxSlots` 的合法范围是 `1..65536`。
- `connect()` 拒绝空 delegate；容量耗尽时返回 `buffer_overflow`。
- `emit()` 按槽表同步调用当前连接，并返回实际调用数量。
- `disconnect()` 只接受当前 generation 的 connection token。
- `clear()` 清除全部连接；旧 token 随后无效。
- `size()`、`capacity()` 与 `empty()` 只反映本地槽表。
- 广播签名不接受右值引用参数。

`connection` 只是断开句柄，不拥有 target，也不承诺析构时自动断开。

### `service::state<T, MaxSlots>`

- 保存一个当前值，并用 `signal<void(const T&, const T&)>` 通知变化。
- `T` 必须可复制、可赋值且可比较相等。
- `set()` 遇到相同值时返回 `false`，不通知。
- 值变化时先更新真相，再同步通知 `(new_value, old_value)`，并返回 `true`。
- 观察者断开后，后续 `set()` 仍然更新真相。
- `connect()`、`disconnect()` 与 `changed()` 直接使用内部 signal 的语义。

### `service::deferred_signal<Event, Poster>`

- 构造时保存一个不拥有的 `Poster*`。
- 只暴露 `post(event)` 与 `poster()`。
- `post()` 直接转发给 poster，不执行 slot，也不自带队列或调度器。

poster 的生命周期必须覆盖 wrapper。是否有界、是否 irq-safe、何时 dispatch，均由具体
poster 决定，不能从 `deferred_signal` 类型本身推断。

## 执行域规则

`signal::emit()` 与 `state::set()` 是同步调用。调用方承担所有 slot 的执行成本和副作用，
因此只允许用于同一执行域内可立即完成的短路径。

以下情况必须先进入明确的 poster、queue、reactor ingress 或 scheduler submit：

- ISR 到 task
- task 到另一个 task
- driver ingress 到 reactor drain
- background worker 到 UI 或其它运行域

当前实现不提供并发保护或 irq-safe broadcast。不得并发调用同一个对象的
`connect()`、`disconnect()`、`emit()`、`clear()` 或 `state::set()`；ISR 中也不得直接调用
普通 `emit()` 或 `state::set()`。

## 生命周期与修改规则

- signal、state 和 deferred_signal 都不拥有回调 target 或 poster。
- 长期连接必须由外部所有者保证 target 生命周期，并显式保存 connection token。
- 不得依赖 connection token 析构完成自动解绑。
- 不得在同一个 signal 的 `emit()` 过程中修改其连接表；当前实现没有定义这种重入修改的语义。
- slot 必须 `noexcept`、有界且非阻塞；不得在其中等待 IO、锁或调度完成。
- fanout 的当前遍历顺序不是业务协议，不应依赖它表达先后关系。

## 不提供的能力

这些原语当前不提供：

- 动态扩容或动态内存所有权
- 线程安全、ISR-safe 广播或递归保护
- 事件队列、mailbox、replay、trace 或自动传播图
- 跨域路由选择、调度策略或 delivery guarantee
- 系统级 wiring 的自动生成或 materialize

若需求属于上述范围，应选择对应 runtime/IO 机制，而不是扩张 `signal` 的含义。

## Review 清单

- 这里表达的是一次边沿事件，还是一个当前真相？
- 所有 slot 能否在调用点同步、有界地完成？
- 是否跨 ISR、task、reactor 或其它运行域？
- target 与 poster 的生命周期是否明确覆盖连接？
- 是否在 emit 期间修改同一连接表，或依赖 fanout 顺序？
- 需求是否本质上需要队列、调度、replay 或 trace？

任一项不清楚时，不应直接使用 `emit()`。
