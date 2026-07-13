# Signal / State 局部契约 v0

## 文档状态

- `status`: `supporting`
- `scope`: `util.delegate`、`service.signal/state/deferred_signal`
- `authority`: 当前 module 源码

这些类型是固定容量、局部连接原语，不是系统事件框架。历史讨论见
[`signal-state-v0`](../archive/signal-state-v0/README.md)。

## 类型语义

### `util::delegate<Args...>`

- 保存非 owning 的 `ctx` 和 `noexcept` stub；
- 可绑定满足约束的自由函数或成员函数；
- 空 delegate 调用无效果；
- 不管理 target 生命周期或自动解绑。

### `service::signal<void(Args...), MaxSlots>`

- `MaxSlots` 范围为 `1..65536`，广播参数不接受右值引用；
- `connect()` 拒绝空 delegate，满容量返回 `buffer_overflow`；
- `emit()` 同步遍历当前槽位并返回调用数；
- `disconnect()` 只接受槽位和 generation 均匹配的 token；
- `clear()` 清空槽位并使旧 token 失效；
- connection 仅用于断开，不拥有 target，析构时不会自动解绑。

### `service::state<T, MaxSlots>`

`T` 必须可复制、可赋值且 equality comparable。`set()` 对相同值返回 `false`；变化时先更新当前值，
再同步发送 `(new_value, old_value)` 并返回 `true`。连接语义直接继承内部 signal；没有 observer 时
truth cell 仍正常更新。

### `service::deferred_signal<Event, Poster>`

该 wrapper 保存非 owning `Poster*`，只转发 `post(event)`，不执行 slot，也不提供队列或调度器。
poster 的生命周期、容量、IRQ 安全和 dispatch 时机由具体实现负责。

选型上，`signal` 表达一次边沿通知，`state` 表达可读取的当前真相与变化通知，`deferred_signal`
只表达事件必须交给外部 poster 跨域投递。三者不能互相掩盖 ownership、队列或执行上下文。

## 执行域

`signal::emit()` 和 `state::set()` 是同一执行域内的同步调用。调用方承担全部 slot 的耗时和副作用。
ISR 到 task、task 间、driver ingress 到 reactor、worker 到 UI 等跨域路径必须先进入明确的 poster、
queue、reactor ingress 或 scheduler submit。

当前实现不提供并发保护、IRQ-safe broadcast 或递归保护。不得并发修改或调用同一 signal/state，
也不得在 `emit()` 期间修改其连接表。slot 应为 `noexcept`、有界、非阻塞；遍历顺序不构成业务协议。

若调用点不能证明自己可以承担全部 slot 的最坏耗时，默认按不同执行域处理并使用显式 ingress/post。
系统级固定 wiring 不应隐藏在匿名 connect 网中。

## 生命周期与缺口

- target 和 poster 必须比保存它们的 delegate、connection 或 wrapper 活得更久；
- 长期连接由外部 owner 保存 token 并显式断开；
- 不提供动态扩容、队列、mailbox、replay、trace 或 delivery guarantee；
- 不提供跨域路由、调度策略或系统 wiring materialize。

同域、同步、有界条件不成立时，不直接使用 `emit()`。当前行为证据为
`Examples/service/service_signal_state_demo`。
