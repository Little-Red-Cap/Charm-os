# X/YModem 最小协议设计（草案）

目标：为 Bootloader 下载模式提供可实现、可验证的最小协议规范，优先支持 XModem（CRC），在其上扩展 YModem 头帧。

## 1. 协议定位

- **XModem**：单文件传输（128B/1K），可靠但简单。
- **YModem**：在 XModem 基础上增加“文件名/大小”头帧，可批量传输。
- **ZModem**：复杂，不作为最小实现目标。

## 2. 帧结构（XModem）

```
SOH/STX | blk | ~blk | data(128/1024) | CRC16
```

- `SOH` (0x01)：128B 数据块
- `STX` (0x02)：1K 数据块
- `blk`：块号（1..255 循环）
- `~blk`：块号取反
- `CRC16`：XModem-CRC (poly 0x1021)

控制字符：
- `NAK` (0x15)：请求重发
- `ACK` (0x06)：确认
- `EOT` (0x04)：传输结束
- `CAN` (0x18)：取消
- `C`   (0x43)：请求 CRC 模式

## 3. 会话流程（XModem-CRC）

```
接收端 -> 发送 'C'
发送端 -> 发送数据帧
接收端 -> ACK / NAK
...
发送端 -> EOT
接收端 -> ACK
```

最小实现：
- 只支持 CRC 模式（忽略 checksum 模式）
- 超时重发 `C`，超时后放弃
- 允许重发同一块（blk 相同则覆盖）

## 4. YModem 头帧（可选扩展）

YModem 在发送数据前先发送 **块号 0** 的头帧：

```
SOH | 0x00 | 0xFF | "filename\0size\0" | CRC16
```

规则：
- 文件名 ASCII
- size 为十进制字符串
- 其余填充 0

完成：
- 发送端发送 EOT，接收端 ACK
- 若批量传输，下一文件继续发送头帧

最小实现建议：
- 先只支持单文件（只识别头帧一次）

## 5. 错误与重试

- 每块最大重试次数：建议 10
- 超时：建议 1s~3s（UART 波特率相关）
- 接收端检测 `blk` 与 `~blk` 不匹配 → NAK
- CRC 错误 → NAK
- 连续失败 → CAN 取消

## 6. 与 Bootloader 的对接

最小输入：
- `read_byte(timeout)`：从串口读取
- `write_byte()`：发送 ACK/NAK/C
- `write_data()`：发送响应

输出：
- 每块数据写入 Flash（或写入临时缓冲）
- 记录总长度

## 7. 实施建议（最小）

阶段 A：
- 只实现 XModem-CRC
- 固定 1K 块（STX）

阶段 B：
- 支持 SOH 128B
- 支持 YModem 头帧

## 9. Charm 落地点建议

- 模块路径：`Modules/io/proto/xmodem/`
- 只实现 XModem-CRC（最小闭环）
## 8. 参考常量

```
SOH = 0x01
STX = 0x02
EOT = 0x04
ACK = 0x06
NAK = 0x15
CAN = 0x18
C   = 0x43
```
