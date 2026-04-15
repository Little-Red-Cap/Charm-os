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

当前仓库的对应落地点已经分为三层：
- 协议层：`Modules/io/proto/modem/modem_xymodem.cppm`
- Bootloader 封装层：`Modules/system/boot/boot_xymodem.cppm`
- Stage2 会话层：`Modules/system/boot/boot_session.cppm`

其中 `boot_xymodem` 提供：
- `boot::XyModemFlashConfig`：目标分区、Flash 擦写参数、头帧要求与最大尺寸约束
- `boot::XyModemFlashState`：已写入字节数、头帧文件名、声明大小、CRC32 与错误标志
- `boot::XyModemFlashResult`：传输状态与 Bootloader 侧写入结果汇总
- `boot::XyModemFlashReceiver<MaxBlock>`：把 `modem::XyModem<MaxBlock>` 的回调直接绑定到 `boot::flash_write`

当前实现特性：
- 支持 YModem 头帧解析并记录文件名/大小
- 可配置是否要求头帧（`require_header`）
- 可按头帧声明长度裁剪最后一个块（`trim_to_header_size`）
- 会在写入过程中累计 payload CRC32，便于后续镜像校验链路复用
- 以 Bootloader 分区上限和 `max_size` 共同约束下载尺寸
- `boot_session` 可在传输完成后继续执行目标分区校验、写入 `BootInfo.pending`，再通过统一 `BootPlan` 完成槽位决策、跳转前回滚预备与成功确认
- `boot_session` 的结果对象已收敛到 `BootPlan + XyModemFlashResult + compact flags`，避免重复携带 `boot/info/loaded` 快照
- `boot_launch` 可把已决策的 `BootPlan` 进一步解析为目标分区、镜像头与 entry 偏移，便于后续板级跳转对接
- `boot_load` 可把 `BootTarget` 进一步解析为显式加载契约，统一表达 `copy_to_ram` 与 `xip`
- `boot_board_load` 进一步桥接 `platform::board::BootLoadDesc`，让真实板级代码只暴露 payload 基址解析与可选搬运 hook
- `boot_exec` 则只在镜像 ready 之后处理 pre-jump/jump，不再反向承担 payload 地址解析
- `boot_board_exec` 继续桥接 `platform::board::BootExecDesc`，让真实板级代码只暴露跳转准备与 jump hook
- 板级 load/exec hook 已改为 request 结构体入参，便于后续扩展 MMU/cache/TLB 等目标相关字段
- `boot_handoff` 把 `BootPlan -> BootTarget -> BootLoadPlan -> BootLoadedImage -> BootExecution -> rollback prepare` 进一步串成一个更轻的 handoff，并通过 accessor 暴露 plan/target/load/image
- `platform::board::BootBoardCaps` 已独立承载加载能力与 jump 能力，真实板级可以不必先依赖整板 `BoardCaps`
- `platform::board::with_boot_caps(...)` 可在需要时把独立 boot 能力拼回 `BoardCaps`
- `platform.board.armv7a_stub` 已提供 ARMv7-A 风格板级骨架，默认用固定 XIP window / RAM payload 基址解析加载落点，并把 copy / prepare / jump 留给 hook

这里同样要区分几类准备动作：
- Boot 元数据准备：`prepare_selected_boot()` 负责把 `pending_trial` 写回旧 `active`，形成失败自动回滚语义
- 加载准备：`prepare_boot_loaded_image()` 负责让目标镜像进入可执行态，包括 XIP 就绪或 copy-to-RAM 搬运
- 板级执行准备：`prepare_boot_execution()` 负责真正跳转前的机器状态切换

## 7. 实施建议（最小）

阶段 A：
- 只实现 XModem-CRC
- 固定 1K 块（STX）

阶段 B：
- 支持 SOH 128B
- 支持 YModem 头帧

## 8. 当前验证方式

- `Examples/io/xymodem_demo`：主机侧构造 YModem 头帧与数据帧，验证握手、文件名、文件大小与逻辑字节数。
- `Examples/boot/bootloader_demo`：先写入有效 Slot A 镜像，再通过 `boot::XyModemSession` 下载并暂存 Slot B，随后执行：
  - 传输结果收口
  - 策略校验与 `BootInfo.pending` 写入
  - 生成 `BootPlan`
  - 通过 `prepare_handoff()` 一次性完成目标解析、加载解析、执行解析与回滚预备
  - 通过独立的 `platform::board::BootBoardCaps` 与 `BootLoadDesc` / `BootExecDesc` mock hook 验证 load/jump 调用
  - 通过 `platform.board.armv7a_stub` 验证 ARMv7-A 风格 copy-to-RAM / XIP 两条板级骨架路径
  - 基于计划的成功确认
  - 缺失头帧失败路径验证
  - 非法 `entry_offset` 镜像拒绝验证
  - `xip_payload` 镜像旁路加载验证

这条示例链路对应当前 Bootloader 的最小闭环：`X/YModem -> Flash -> Verify -> Pending -> BootPlan -> Target -> Load -> RollbackPrepare -> Exec -> Confirm`。

## 9. Charm 落地点建议

- 模块路径：`Modules/io/proto/modem/`
- 模块名：`io.proto.modem_xymodem`
- 只实现 XModem-CRC + YModem 头帧解析（最小闭环）

## 10. 参考常量

```
SOH = 0x01
STX = 0x02
EOT = 0x04
ACK = 0x06
NAK = 0x15
CAN = 0x18
C   = 0x43
```
