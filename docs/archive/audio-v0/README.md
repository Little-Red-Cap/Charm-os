# Audio v0/v1 设计归档

> **状态：`archived`**

本目录保留音频子系统早期设计。固定水位、MCU 内存区域、Driver 草图、Actor 事件和 v1/v2/v3 路线均是历史讨论，不代表当前跨平台契约。

- [`charm_audio_architecture.md`](charm_audio_architecture.md)：早期 Pull Engine 总体架构；
- [`audio_design_v1.md`](audio_design_v1.md)：早期 MCU/PC 同构与水位策略；
- [`audio_design_full_draft.md`](audio_design_full_draft.md)：原 Modules 目录中的完整 L1/L2/L3 草案；
- [`audio_readme_legacy.md`](audio_readme_legacy.md)：早期音频文档路由。
- [`usb_uac_board_bringup_notes.md`](usb_uac_board_bringup_notes.md)：UAC descriptor、Windows cache 与板级时钟调试记录。

现行入口：

- [`../../system/charm_audio_architecture.md`](../../system/charm_audio_architecture.md)
- [`../../audio/audio_design_v1.md`](../../audio/audio_design_v1.md)
- [`../../../Modules/media/charm.media.audio.cppm`](../../../Modules/media/charm.media.audio.cppm)
