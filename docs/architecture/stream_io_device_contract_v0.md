# Stream IO Device Interface v0

> status: `exploration`
>
> scope: 尚未统一的 stream device interface

本文不替代 [`io.channel` 契约](../io/io_channel_contract.md)，也不定义 Charm Core 或公共 ABI。

早期 short IO、detach 与 flush 未决语义见
[`Device Interface v0`](../archive/device-interface-drafts-v0/README.md#stream-io)。
准入规则见 [`interface_admission_policy.md`](interface_admission_policy.md)。

## 代码事实

当前 channel、registry、reactor、stable slot 和 USB runtime glue 支持非阻塞 IO 装配，但没有证明跨
UART/USB/socket 的公共 Stream device contract 已成立。已冻结的 `read/write/flush` 行为只属于
`io.channel` 契约。

## 当前边界

- 非阻塞、关闭、短读写、背压和 flush 语义需要由调用链共同确认；
- notify/drain、ISR/task 上下文、并发和 ownership 尚未形成统一公共承诺；
- framing、overrun、flow control、timeout 等错误目前不能假设已有稳定 taxonomy；
- endpoint name 或 registry slot 不是自动生成的 capability contract。

## 重新推进条件

若继续推进，先用现有 channel API 做一个独立 line/frame consumer 和 fault script，验证 non-blocking 与 detach 行为，再决定是否需要更窄的 Stream device interface。基础契约不应自行吸收 scheduler、timeout 或 protocol framing。
