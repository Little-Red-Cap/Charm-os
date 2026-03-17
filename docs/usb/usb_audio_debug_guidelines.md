# USB Audio (UAC1) 调试建议性规约

本文是 **建议性规约**，用于减少 USB Audio 枚举/播放失败的反复试错。
目标是保证 “能进入 alt=1 并开始 streaming” 作为最小成功标准。

## 1. 先定义最小成功判据

必须同时满足：

- 收到 `SET_INTERFACE alt=1`（接口切换到流式模式）
- `cmd_calls` 增长（AudioCmd 被调用）
- `bytes/pkts` 递增（EP1 OUT 有数据）

这三项齐全，才算 “可用”。

## 2. 描述符一致性（最容易出错）

改描述符必须**全链路同步**，否则 Windows 往往停在 alt=0：

- `bNrChannels` / `wChannelConfig`
- Feature Unit 长度与 `bmaControls` 数量
- Format Type 的 `bNrChannels`
- `USB_AUDIO_CONFIG_DESC_SIZ` 与 `wTotalLength`
- `AUDIO_OUT_PACKET` 与 `wMaxPacketSize`

> 只改其中一项，经常导致 “枚举正常但不切 alt=1”。

## 3. Windows 缓存问题（必须绕开）

每次改描述符，**必须更换 PID**（或清设备缓存）。

建议方式：

- `USBD_PID_FS` 改一个新值
- 用新 PID 强制 Windows 重新枚举

否则可能出现：描述符已修，但 Windows 仍按旧配置判定失败。

## 4. 先用 ST 原版作基线

正式自研前，先保证 **ST 原版 usbd_audio.c/h** 在当前板子可切到 alt=1。

成功后再替换为自研实现，否则问题定位会被放大。

## 5. 必备观测日志

建议最少保留三类日志：

- EP0 Setup 请求（至少记录 `SET_INTERFACE` / `GET/SET_CUR`）
- `set_if / alt` 统计
- `cmd_calls / bytes / pkts` 统计

日志在确认可用后可关，但保留回归入口。

## 6. 常见失败特征与定位

- **只出现 alt=0**：描述符不自洽或 Windows 缓存旧配置
- **cmd_calls=0 且 bytes/pkts=0**：没有进入 streaming
- **alt=1 但 bytes/pkts=0**：EP1 OUT 收不到数据，检查端点/中断/FIFO

## 7. 推荐调试顺序（最短路径）

1. 回到 ST 原版描述符  
2. 换 PID  
3. 观察是否进入 alt=1  
4. 进入 alt=1 后再调数据流与播放  
5. 最后再替换为自研协议栈

## 8. 结果记录（建议）

每次成功打通，记录以下三项：

- 描述符版本（是否 ST 原版 / 自研）
- PID 值
- 最小成功日志片段（含 alt=1 与 bytes/pkts）

这能避免“同一个坑反复踩”。

## 9. UAC 最小回归检查点（已验证可用）

以下条件同时满足，视为 UAC 播放链稳定：

- `set_if` 出现 `alt=1`
- `ring` 稳定在 30KB+ 且不持续下降
- `ring_ovf=0`
- `i2s kern_clk` 接近 `98.304MHz`

## 10. 推荐时钟配置（25MHz HSE）

为保证 48kHz 采样率稳定，推荐使用 PLL2 输出 SPI123 内核时钟：

- 目标：`98.304MHz`（48k * 2048）
- 参数：`M=5, N=39, FRACN=2634, P=2`

该组合已经在本项目中验证可用。
