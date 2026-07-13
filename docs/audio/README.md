# 音频文档入口

> **文档状态：`supporting`**

本页只提供音频模块的最短路由，不定义产品播放器或 Charm Core。

| 问题 | 入口 |
|---|---|
| 模块、实时路径与 backend 边界 | [`audio_design_v1.md`](audio_design_v1.md) |
| Host 可运行示例 | [`../../Examples/audio/README.md`](../../Examples/audio/README.md) |
| 公开 C++ module 聚合入口 | [`../../Modules/media/charm.media.audio.cppm`](../../Modules/media/charm.media.audio.cppm) |
| 早期设计与参数讨论 | [`../archive/audio-v0/`](../archive/audio-v0/) |

行为与能力以模块源码、构建接线和当次 smoke 为准。Host、pull simulator 与 real-board I2S 是不同证据域，不能互相替代。
