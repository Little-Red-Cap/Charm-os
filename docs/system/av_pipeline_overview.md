# Audio/Video 中间件：Stream/Pipeline 形状对齐

目的：对标 VSF 的 stream/pipeline 接口形状，把现有 Audio 设计抽象成可复用的 AV 中间件骨架。

## 1. 分层映射（对标 VSF）

```
VSF:  stream/source -> filter -> sink
Charm: Source -> (Demux) -> Decoder -> (Resample/Mix) -> FIFO -> Sink
```

| VSF 概念 | Charm 现有 | 说明 |
| --- | --- | --- |
| stream source | IDataSource | 只负责读取/seek/tell |
| stream filter | IDecoder / Resampler / Mixer | 解码与变换 |
| stream sink | IAudioSink | pull 模式回调 |
| stream buffer | IPcmFifo | SPSC FIFO |

> 结论：Charm 已经具备完整 pipeline 形态，但缺少“抽象的 AV stream 统一接口层”。  
> 目标是把这些接口抽到 `media/stream` 里，而不是让 audio 独占。

## 2. 通用 Stream 形状（草案）

### 2.1 Source

```
read(out) / seek / tell / size / read_at
```

### 2.2 Filter

```
process(in -> out) / flush / reset / query
```

### 2.3 Sink

```
configure(fmt) / start / stop / pull(callback)
```

## 3. Pipeline 形状（草案）

```
Source -> Filter* -> FIFO -> Sink
```

扩展位点：
- Demux：把 container 拆分成 packet/帧流
- Decoder：压缩 -> PCM/Raw
- Resample/Mix：域内处理（S32）
- Quantize：S32 -> S16（写入 FIFO 前）

## 4. 运行模型（与音频一致）

### 实时路径（回调/ISR）

- 只做 FIFO 拉取 + 补零 + 计数

### 非实时路径（事件线程）

- 读文件 / 解码 / 变换 / 写 FIFO

## 5. 对齐后的目标

### 接口层

- `media.stream`：统一 Source/Filter/Sink 接口
- `media.pipeline`：标准管线编排（与 audio.player 同构）

### 实现层

- audio.decoder.wav/flac/mp3 变为 media.filter 子类
- audio.sink.sdl3 变为 media.sink 子类
- audio.source.file 变为 media.source 子类

## 6. 迁移策略（渐进）

1. 只抽接口，不动实现
2. audio 层实现逐步改为实现 media.stream 接口
3. player 只依赖 `media.pipeline` 与 `media.stream`

## 7. 收敛标准

- AV 中间件可独立复用
- audio 仅是 media 的一个领域实现
- 与 VSF stream/pipeline 的形状对齐，但不引入 VSF 宏与实现细节
