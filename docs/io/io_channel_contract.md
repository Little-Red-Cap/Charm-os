# io.channel 契约

## 文档状态

- `status`: `canonical`
- `scope`: `io::Channel` 的同步调用与错误边界
- `source`: [`io.channel.cppm`](../../Modules/io/channel/io.channel.cppm)

## 调用结果

`Channel` 是一组不拥有调度器的同步函数指针。`read()`、`write()` 和 `flush()` 不得在内部等待、
sleep、spin 或重试。

| 操作 | 成功 | 暂不可用 | 缺少实现 |
|---|---|---|---|
| `read(out)` | `Ok(n)`，`0 < n <= out.size()` | `would_block` | `invalid` |
| `write(in)` | `Ok(n)`，`0 < n <= in.size()`，允许 partial write | `would_block` | `invalid` |
| `flush()` | backend 定义的结果 | `would_block` | `not_supported` |

`read()` 或 `write()` 返回 `Ok(0)` 时，wrapper 调用 `util::halt()`。EOF、关闭和传输失败分别使用
`end_of_stream` / `closed` 与 `io_error`；同一实现不得混用 EOF 分类。

## Buffer 与并发

- buffer 由调用方拥有；函数返回后不得保留其指针。
- `Channel` 不包含锁、等待队列或 reentrancy 元数据。
- ISR 可用性由注册时的 `EndpointCaps::isr_safe` 或更窄的 driver 契约声明。
- 未声明并发保证时，由上层串行化 reader/writer。

等待、timeout 和事件派发属于 Reactor、scheduler 或协议状态机，不属于 `Channel`。
