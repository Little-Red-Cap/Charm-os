# Boot X/YModem 子集

> status: supporting
>
> 本文描述当前 receiver 实现，不是完整 X/YModem 标准。源码入口为
> [`../../Modules/io/proto/modem/modem_xymodem.cppm`](../../Modules/io/proto/modem/modem_xymodem.cppm)
> 和 [`../../Modules/system/boot/boot_xymodem.cppm`](../../Modules/system/boot/boot_xymodem.cppm)。

## Wire 子集

数据帧：

```text
SOH/STX | seq | ~seq | data(128/1024) | CRC16-CCITT
```

| 字节 | 值 | 当前行为 |
|---|---|---|
| `SOH` | `0x01` | 128-byte block |
| `STX` | `0x02` | 1024-byte block；`MaxBlock` 小于 1024 时拒绝 |
| `EOT` | `0x04` | 回复 ACK 并结束 |
| `ACK` | `0x06` | 接收成功响应 |
| `NAK` | `0x15` | 帧、序号或 CRC 不匹配时响应 |
| `CAN` | `0x18` | 终止为 `cancel` |
| `C` | `0x43` | 启动及 header 后请求 CRC mode |

receiver 接受重复的上一块并再次 ACK，不重复写入。CRC 或序号错误会 NAK 并等待
重发；超时由调用方显式调用 `on_timeout()` 驱动，超过 `max_retries` 后返回
`retries_exhausted`。输入 ring 写满返回 `overflow`。

当前实现不提供 sender、checksum mode、batch YModem、ZModem 或链路级时间源。
`Config::timeout_ms` 不会自行调度计时，`Config::use_1k` 也不限制接收帧类型。

## YModem Header

块号 `0` 解析为：

```text
filename NUL decimal_size NUL padding
```

- 文件名和 size 通过 header callback 输出；
- 空文件名 ACK 后结束当前 receiver；
- 只支持单文件语义，不实现 batch continuation；
- 文件名按原始 bytes 解释，未提供完整编码或路径安全策略。

## Boot 写入封装

`boot::XyModemFlashReceiver` 将数据块写入一个 `boot::Partition`：

- `target.size` 与可选 `max_size` 共同限制写入；
- `require_header` 可拒绝无 YModem header 的会话；
- `trim_to_header_size` 可裁掉最后一块 padding；
- 累计逻辑 `bytes_written` 与 payload CRC32；
- 独立报告 transport、missing header、write error 和 size error。

它不校验 `boot::ImageHeader`，也不选择槽位。`boot::XyModemSession` 在传输结束后
才执行 partition verify、pending 写入、`BootPlan` 选择、handoff prepare 和 confirm。

## Session 终态

`boot::XyModemSession` 使用以下阶段：

```text
idle -> receiving -> transport_done -> verified
     -> pending -> selected -> prepared -> confirmed
     -> failed
```

只有 result 的 `ready_to_boot` flag 为真且 handoff 的 `ready_to_jump` 为真，调用方
才可以进入 board jump。`failed` 不代表 Flash 内容已自动回滚或擦除。

## 验证入口

- [`../../Examples/io/xymodem_demo`](../../Examples/io/xymodem_demo)：header、size、
  protocol bytes、logical bytes 与 parser result。
- [`../../Examples/boot/bootloader_demo`](../../Examples/boot/bootloader_demo)：写入 Slot B、
  缺 header 失败、镜像校验、A/B policy、copy/XIP、handoff 与成功确认。

两者均为 host mock，不证明 UART 时序、真实 Flash 或断电恢复。
