# AV 中间件草图：Stream/Pipeline 形状对齐

> **状态：`archived`**
>
> 该草图早于当前 `Modules/media/stream` 实现，其中“尚缺少统一 stream 接口”等判断已经失效。
> 保留本文只用于追溯 VSF stream/pipeline 对照与最初迁移设想。

## 分层映射

```text
VSF:   stream/source -> filter -> sink
Charm: Source -> (Demux) -> Decoder -> (Resample/Mix) -> FIFO -> Sink
```

| VSF 概念 | 当时 Charm 对应项 | 说明 |
|---|---|---|
| stream source | IDataSource | read/seek/tell |
| stream filter | IDecoder / Resampler / Mixer | 解码与变换 |
| stream sink | IAudioSink | pull callback |
| stream buffer | IPcmFifo | SPSC FIFO |

当时提案认为 audio 已具备 pipeline 形状，但还需要抽出通用 AV stream 接口。

## 提议的接口形状

```text
Source: read / seek / tell / size / read_at
Filter: process / flush / reset / query
Sink:   configure / start / stop / pull

Source -> Filter* -> FIFO -> Sink
```

扩展点包括 demux、decoder、resample/mix 和 quantize。实时路径只执行 FIFO pull、补零与计数；
文件读取、解码、变换和 FIFO 写入留在非实时上下文。

## 原迁移设想

1. 抽出 `media.stream` 接口，不改 audio 实现。
2. 逐步让 audio source/decoder/sink 实现通用接口。
3. 让 player 只依赖 `media.pipeline` 与 `media.stream`。

该迁移设想不再代表当前计划。现状必须从 `Modules/media/stream`、audio 契约与实际 consumer 核对。
