# AT Runtime 契约

## 文档状态

- `status`: `supporting`
- `scope`: `at.parser`、`at.session` 与 `at.driver_reactor` 的当前局部行为
- `authority`: 受 [`CONSTITUTION.md`](../../CONSTITUTION.md) 和
  [`charm_core_contract.md`](../architecture/charm_core_contract.md) 约束

AT runtime 是固定容量的行解析与命令会话实现，不是 Charm Core、设备协议库或可靠 transport。

## 模块边界

- [`at.parser`](../../Modules/io/at/at.parser.cppm) 将 CR/LF 分隔的文本分为精确匹配的 `OK`、
  `ERROR`、以 `+` 开头的 `urc` 和其它 `line`。
- [`at.session`](../../Modules/io/at/at.session.cppm) 管理固定容量命令队列、发送进度、timeout、
  retry、普通行、URC 和完成回调。
- [`at.driver_reactor`](../../Modules/io/at/at.driver_reactor.cppm) 用 `io::Channel` 与
  `io::Reactor` 驱动收发，并以固定 ring 和单次事件 budget 限制工作量。

Parser 不拥有 transport，Session 不拥有设备指令集，ReactorDriver 不拥有 Channel、Reactor 或
Session。LTE、BT、Wi-Fi、GNSS 等命令和重连策略应位于外部 adapter。

## 生命周期与推进

- `Parser::feed()` 同步触发 handler；`Event.text` 指向 parser 内部缓冲，只能在回调期间使用。
- `Session::enqueue()` 保存 `Command` 中的 `string_view` 和 callback ref；其引用对象必须活到命令
  完成。
- `Session::tick(now_ms)` 推进 timeout/retry；`notify_writable()` 继续此前的 partial send。
- timeout 从一轮命令完整发送后开始计算。`OK` 完成成功，`ERROR` 或耗尽 retry 完成失败。
- URC 独立通知，不完成或打断活动命令。
- `ReactorDriver::start()` 订阅 readable、writable 与 closed；调用方仍负责周期性调用 `tick()`。

## 失败边界

- parser 行容量为 `LineCap - 1`。超长行当前被静默截断，`overflow_` 不产生错误事件。
- 队列满时 `enqueue()` 返回 `false`；未绑定 sender 时活动命令不会取得发送进度。
- sender 的 `would_block` 保留剩余数据；其它错误使当前命令失败。
- 对非空输入返回成功但写入 `0` 会进入 `util::halt()`，transport adapter 不得这样返回。
- ReactorDriver 的 ring、RX/TX buffer 与事件 budget 均为模板或运行参数；容量不足不会自动扩容。
- Channel closed 当前没有 session completion；非 `would_block` 的 channel 错误也没有统一上报面。

## 验证状态

仓库当前没有独立 AT example、smoke 或产品装配，除三个 module source 外没有 tracked consumer。
因此本文只描述局部代码行为，不证明 UART、USB CDC、设备兼容性、实时性或断线恢复。
