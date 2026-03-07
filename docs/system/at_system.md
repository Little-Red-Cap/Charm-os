# AT 子系统（文档草案）

目标：为 MCU 常见 AT 设备提供最小可复用的解析与会话骨架，并与 EDA 事件流对齐。

## 分层设计

L0 Transport（HAL/IO）
- UART / USB CDC / Socket 的字节流读写
- 提供 `read(span<u8>)` 与 `write(span<u8>)` 能力

L1 Parser（核心层）
- 输入字节流，按行解析
- 输出事件：`OK / ERROR / URC / LINE`
- 无动态分配、无阻塞

L2 Session（服务层）
- 命令队列、超时、重试
- URC 分发与订阅
- 与 EDA 事件循环对接

L3 Adapter（设备层）
- 设备指令集封装（如 LTE/BT/WiFi/GNSS）
- 状态机与重连策略

## 当前模块

- `at.parser`：最小行解析器
- `at.session`：命令队列 + 超时 + 重试 + URC 分发
- 后续计划：`at.transport` / `at.device.*`

## 最小事件模型

```
AT input bytes
    -> Parser
    -> Event(kind, text)
```

事件类型：
- `ok`：行内容为 `OK`
- `error`：行内容为 `ERROR`
- `urc`：以 `+` 开头的异步通知
- `line`：普通行

## 最小使用示例

```cpp
import at.parser;
import at.session;

using AtParser = at::Parser<128>;

static void on_event(void*, const at::Event& ev) noexcept {
    // 按需分发到 EDA/日志
    (void)ev;
}

void demo_feed(std::span<const util::u8> data) {
    AtParser p;
    p.set_handler(on_event, nullptr);
    p.feed(data);
}
```

## Session 使用要点

```
enqueue(cmd) -> send -> wait OK/ERROR -> done
```

说明：
- 解析事件由 `at.session` 内部转发。
- `URC` 独立回调，不打断当前命令。
- 超时触发重试，超过 `retries` 后回调失败。

## Transport 绑定（UART/CDC）

最小约束：
- `send` 只负责发出字节流，不保证阻塞完成。
- 接收侧以回调或轮询形式调用 `session.feed(...)`。

UART 轮询示例（伪码）：

```
UartBridge<8, 128, 128> bridge(session);
bridge.set_io(rx_fn, tx_fn, uart_ctx);
bridge.poll(); // 在 EDA tick 中调用
```

CDC 回调示例（伪码）：

```
CdcBridge<8, 128, 512, 64> bridge(session);
auto cb = bridge.callbacks();
// usb.class_cdc 侧：on_out -> feed，on_in_request -> 取发送队列
```

## 约束与注意事项

- 解析器只做“行”与“状态”识别，不负责命令队列。
- 事件 `text` 视图只在下一次 `feed()` 前有效。
- 超时、重试、URC 路由由 `at.session` 处理。

## 下一步计划

- `at.transport`：与 UART/CDC 绑定
- 最小 Demo：发送 `AT` → `OK`
