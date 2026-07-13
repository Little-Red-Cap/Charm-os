# Audio v0/v1 设计归档

> **状态：`archived`**

本目录保留音频子系统早期设计中仍有独立价值的取舍，不代表当前跨平台契约。

- [`audio_design_retained_notes.md`](audio_design_retained_notes.md)：从旧 Pull Engine、v1 和 L1/L2/L3
  草案中保留的 DSP、重配、声道转换与 DMA 设计判断；
- [`usb_uac_board_bringup_notes.md`](usb_uac_board_bringup_notes.md)：UAC descriptor、Windows cache 与板级时钟调试记录。
- [`av_pipeline_overview.md`](av_pipeline_overview.md)：早期 VSF stream/pipeline 对照与 media 抽取设想。

现行入口：

- [`../../audio/audio_design_v1.md`](../../audio/audio_design_v1.md)
- [`../../../Modules/media/charm.media.audio.cppm`](../../../Modules/media/charm.media.audio.cppm)
