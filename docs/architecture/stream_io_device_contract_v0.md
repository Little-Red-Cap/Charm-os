# Stream IO Device Interface v0

> status: `supporting`
>
> 本文是当前 stream/channel implementation interface 的状态卡，不替代 `io_channel_contract`，
> 也不定义 Charm Core 或公共 ABI。

## 文档角色

本文是 stream/channel implementation interface 的当前状态卡，不是 `io_channel_contract` 的替代品，也不是 Charm Core 或公共 ABI。

完整的早期讨论已保留在 [`../archive/device-interface-drafts-v0/stream_io_device_contract_v0.md`](../archive/device-interface-drafts-v0/stream_io_device_contract_v0.md)。准入规则见 [`interface_admission_policy.md`](interface_admission_policy.md)。

## 代码事实

当前可核对的基础是：

- `Modules/io/channel/io.channel.cppm` 提供 `read/write/flush` 和 `util::Errc` 结果；
- read/write 的成功结果不能为零，否则触发保护；
- 缺失操作分别返回 `invalid` 或 `not_supported`；
- `io.registry`、`io.reactor`、`ChannelSlot` 和 USB runtime channel 提供注册、通知和稳定槽位的胚胎。

这些代码支持 channel/runtime glue，但没有证明一个跨 UART/USB/socket 的公共 Stream device contract 已成立。

## 当前边界

- 非阻塞、关闭、短读写、背压和 flush 语义需要由调用链共同确认；
- notify/drain、ISR/task 上下文、并发和 ownership 尚未形成统一公共承诺；
- framing、overrun、flow control、timeout 等错误目前不能假设已有稳定 taxonomy；
- endpoint name 或 registry slot 不是自动生成的 capability contract。

## 状态与下一证据

实现成熟度：`proposed`；这不是 Constitution 裁决。

若继续推进，先用现有 channel API 做一个独立 line/frame consumer 和 fault script，验证 non-blocking 与 detach 行为，再决定是否需要更窄的 Stream device interface。基础契约不应自行吸收 scheduler、timeout 或 protocol framing。
