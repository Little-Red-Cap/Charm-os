# USB CDC 最小收发路径与回调契约

本页固定 CDC ACM 的最小数据通路与回调契约，便于 DCD/驱动层接入。

## 数据通路（最小）

```
Host OUT -> EP_OUT -> CdcAcm::on_out_packet()
Host IN  <- EP_IN  <- CdcAcm::on_in_request() -> DCD send
IN 完成 -> CdcAcm::on_tx_done()
```

## 回调契约（CdcOps）

- `rx_buffer()`：返回 OUT 接收缓冲区（可写）。
- `on_rx_done(len)`：接收完成，len 为写入字节数。
- `tx_buffer()`：返回 IN 发送缓冲区（只读）。
- `tx_length()`：可选，返回本次实际可发送长度（<= tx_buffer.size）。
- `on_tx_done(len)`：发送完成回调。
- `notify(data)`：发送 CDC 通知（SERIAL_STATE 等）。
- `on_line_coding/on_control_line`：控制端点回调。

## DCD 接入最小示例

```cpp
// OUT: 收到数据
cdc.on_out_packet(data);

// IN: 需要发送
auto payload = cdc.on_in_request(max_len);
if (!payload.empty()) {
    dcd.ep.send(ctx, ep_in, payload, false);
}

// IN 完成
cdc.on_tx_done(sent);
```

## 约束

- 不阻塞、不分配。
- 缓冲区由上层持有，CDC 仅读取/写入。
