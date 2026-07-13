# USB CDC 数据与回调契约

## 文档状态

- `status`: `supporting`
- `scope`: `usb.class_cdc` 的 CDC ACM control/data callback
- `source`: [`usb.cdc.cppm`](../../Modules/io/usb/class/usb.cdc.cppm)

`CdcAcm` 不拥有 transport、buffer 或 callback context，也不调度 DCD transfer。

## 数据路径

| 入口 | 行为 |
|---|---|
| `on_out_packet(data)` | 将 host OUT 复制到 `rx_buffer()`，随后调用 `on_rx_done(copied)` |
| `on_in_request(max_len)` | 从 `tx_buffer()` 返回不超过 buffer、`max_len` 与可选 `tx_length()` 的 view |
| `on_tx_done(sent)` | 将真实 IN completion 长度转交 callback |
| `send_serial_state(bits)` | 通过 `notify()` 发送 SERIAL_STATE；callback 缺失时返回 `false` |

OUT buffer 为空时返回 `false`。buffer 小于 packet 时只复制前缀并调用 `on_rx_done()`，返回
`false` 表示未完整接收；上层不能只看 callback 被调用就判定成功。

IN buffer 或 callback 缺失时返回空 view。`on_in_request()` 只暴露数据，不表示 DCD 已发送；DCD
完成后必须调用 `on_tx_done()`。

## Control 与生命周期

- Line coding 和 control-line state 由 class request handler 更新，并通过对应 callback 通知。
- `CdcOps`、context 和所有返回 buffer 都由调用方拥有；buffer 至少在当前调用或 transfer 约定期间
  保持有效。
- class 本身不分配、不等待；callback 是否阻塞由 backend 决定，事件循环接入方必须自行约束。
- reset 会恢复 line coding/control state，并可能触发 line-coding callback；它不清理调用方 buffer。

Native 与 replay 入口见 [`Examples/usb/README.md`](../../Examples/usb/README.md)。这些 fixture 不证明
真实 DCD、USB CDC COM 枚举或 host driver 兼容性。
