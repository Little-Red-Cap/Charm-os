# Charm Audio 架构

> **文档状态：`supporting`**

本文说明当前音频模块的实现边界。它不定义 Charm Core；Capability Contract、平台 backend 与产品播放器策略不由本文件统一。

## 源码入口

- [`charm.media.audio.cppm`](../../Modules/media/charm.media.audio.cppm)：media 音频聚合入口；
- [`audio_data_plane.cppm`](../../Modules/media/audio/audio_data_plane.cppm)：decode、frame queue、DSP、FIFO 与 pump；
- [`audio_player.cppm`](../../Modules/media/audio/audio_player.cppm)：控制状态、source/sink 生命周期与 refill；
- [`audio_pump.cppm`](../../Modules/media/audio/audio_pump.cppm)：实时侧 FIFO 消费与 underrun 统计；
- [`audio_sink_common.cppm`](../../Modules/media/audio/audio_sink_common.cppm)：fill 与补零共同规则。

`charm.media.audio` 当前公开基础格式、FIFO、decoder、resampler、pull simulator、spectrum 和 file source。
`audio.player` 与 SDL sink 仅在对应构建开关启用时由该聚合入口导出。I2S sink 属于具体项目/backend，不在公共聚合模块中假装跨平台实现。

## 当前数据路径

```text
StreamSource
  -> DecodePipe
  -> S32 FrameQueue
  -> DspGraph
  -> S32-to-S16 quantize
  -> PcmFifo
  -> AudioPump
  -> Sink callback / DMA fill
```

`AudioDataPlane` 在非实时路径解码、处理并写入 FIFO；sink callback/ISR 只从 FIFO 取完整 frame。
不足部分由 sink common 补零，同时记录 underrun。设备回调触发消费，不允许在回调中执行 decode、storage IO 或动态分配。

## 控制与实现边界

- `AudioPlayer` 持有播放状态和 refill 驱动，不把 UI 或产品策略放入数据面；
- `DspGraph` 当前是固定容量的进程内处理链，不承诺动态插件或通用路由；
- 当前 DSP/frame queue 使用 S32，PCM FIFO 输出使用 interleaved S16；其它输出格式不是既有承诺；
- FIFO、水位、period 和 chunk 由配置及 sink 能力决定，不存在跨平台固定毫秒值；
- D2 SRAM、SDRAM、DMA section 和 cache maintenance 是具体 board/backend 的资源决策，不属于音频公共契约。

## 证据边界

- [`sdl3_wav_demo`](../../Examples/audio/sdl3_wav_demo/README.md) 验证 Host SDL 与 pull simulator 路径；
- `Examples/system/player_playback_engine_smoke` 验证播放器状态、seek、stop 与 session 组合；
- H747 I2S 只能由对应板级构建和运行证据证明，不能从 Host smoke 或本文推断。

详细实时路径规则见 [`../audio/audio_design_v1.md`](../audio/audio_design_v1.md)。早期架构、参数建议与版本路线已归档到 [`../archive/audio-v0/`](../archive/audio-v0/)。
