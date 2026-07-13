# Audio 模块与实时路径契约

> **文档状态：`supporting`**

本文记录音频模块 ownership 与实时路径不变量。具体类型、默认值和错误返回以
[`Modules/media/audio`](../../Modules/media/audio/) 下的源码为准。

## 模块边界

| 模块 | 责任 |
|---|---|
| [`charm.media.audio`](../../Modules/media/charm.media.audio.cppm) | 格式、FIFO、decoder、resampler、pull simulator、spectrum 与 file source 聚合入口 |
| [`audio.data_plane`](../../Modules/media/audio/audio_data_plane.cppm) | decode、frame queue、DSP、量化、FIFO 与 pump |
| [`audio.player`](../../Modules/media/audio/audio_player.cppm) | 播放状态、source/sink 生命周期与 refill |
| [`audio.pump`](../../Modules/media/audio/audio_pump.cppm) | 实时侧 FIFO 消费与 underrun 统计 |
| [`audio.sink.common`](../../Modules/media/audio/audio_sink_common.cppm) | fill 与补零规则 |

`audio.player` 与 SDL sink 只在对应构建开关启用时由聚合入口导出。I2S sink 属于 project/board
backend，不在公共聚合模块中声明为跨平台能力。UI 与产品播放器策略不进入 AudioDataPlane。

## 数据格式

- decoder 输出进入 S32 interleaved frame queue；
- `DspGraph::process()` 原地处理 S32 samples；
- `AudioDataPlane` 在 FIFO 写入边界量化为 S16；
- `PcmFifo` 按 byte 存储，但所有读写必须按 `AudioFormat::frame_size()` 对齐；
- 当前路径不承诺 float、planar、S24 或 S32 sink 输出。

## 生产与消费

非实时生产侧：

```text
decode -> frame queue -> DSP -> quantize -> PCM FIFO
```

实时消费侧：

```text
sink callback / DMA IRQ -> AudioPump -> PCM FIFO -> fill_and_pad
```

实时侧只允许读取 FIFO、补零和更新计数。不得执行 decoder、DSP、storage IO、阻塞等待、日志或动态分配。
underrun 表示本次请求未获得足够 PCM；sink 补零并累计状态，它不自动等于播放器 fatal error。

## 容量与水位

- FIFO storage、frame queue、decode scratch 和 sink scratch 均为固定容量；运行期不扩容；
- `fifo_capacity`、`low_water`、`high_water`、`period_frames` 与 `chunk_frames` 来自配置；
- 配置必须满足 storage 容量、frame 对齐和各模块上限，否则 configure/open 返回失败；
- 文档不规定统一的 `40ms/150ms` 或特定 SRAM/SDRAM 区域，这些属于 profile/backend 调优。

生产侧在 FIFO 低于目标水位时 refill；消费侧按设备请求读取。water/underrun/callback stats 是观测值，只有当次测试日志才能证明某组参数稳定。

## 生命周期

- source open/seek/close 会清理 decode 与 frame queue 状态；
- seek、stop 或 reconfigure 必须先阻止 sink 继续访问即将失效的 buffer，再清理 FIFO/source；
- sink format、period 和 callback storage 在 `open()` 成功后才可用于运行；
- `AudioPlayer` 状态和错误转移以 `audio_player.cppm` 为准，UI 不直接驱动实时数据结构。

## Backend 边界

- SDL3 sink 是 Host 实现；
- pull simulator 复用 fill callback 语义，但不证明真实设备时序；
- I2S/DMA sink 是项目或 board backend，负责 DMA buffer、cache 和 IRQ 约束；
- Null/PumpedNull sink 用于无设备构建和语义 smoke，不代表音频输出成功。

## 验证入口

- [`Examples/audio/sdl3_wav_demo`](../../Examples/audio/sdl3_wav_demo/README.md)
- `scripts/audio_sdl3_wav_demo_smoke.ps1`
- `Examples/system/player_playback_engine_smoke`
- 对应 real-board I2S capture

通过 Host demo 只证明 Host 路径；通过 pull simulator 只证明可重复的消费语义；真实板 DMA/I2S、内存布局和 cache 一致性必须由板级证据单独证明。

旧接口草图、Mermaid、推荐参数和版本路线见 [`../archive/audio-v0/`](../archive/audio-v0/)。
