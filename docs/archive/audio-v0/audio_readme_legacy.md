# 音频文档入口

> 状态：archived。本文是早期音频文档路由，已被更短的源码导向入口替代。

本目录收纳 Charm 音频播放链路的设计材料，重点是 pull engine、buffer 分层、实时路径约束，以及 MCU/PC 同构时要守住的边界。

如果你是第一次进入仓库，先读：

1. [`../../overview.md`](../../overview.md)
2. [`../../architecture_overview.md`](../../architecture_overview.md)
3. [`../../README.md`](../../README.md)

再回到这里按任务进入。

## 按任务进入

### 我想先看当前整体架构形态

先读：

- [`../../system/charm_audio_architecture.md`](../../system/charm_audio_architecture.md)

### 我想看音频子系统设计细化

先读：

- [`audio_design_v1.md`](audio_design_v1.md)

### 我想看可运行示例

回到：

- [`../../../Examples/audio/README.md`](../../../Examples/audio/README.md)

## 建议阅读顺序

1. `../system/charm_audio_architecture.md`
2. `audio_design_v1.md`
3. `../../Examples/audio/README.md`

## 使用提醒

- 这里偏设计与链路约束，不直接替代可运行示例或产品化播放器说明。
- 如果 callback/ISR 边界、buffer 策略、DSP graph 约束或输出后端行为变化，应同步更新这里的入口。
