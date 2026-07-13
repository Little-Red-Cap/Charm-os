# Audio 早期设计保留说明

> `status`: `archived`

当前数据格式、实时路径与 backend ownership 见
[`audio_design_v1.md`](../../audio/audio_design_v1.md)。本文只保留 driver binding、DSP alias、重配事务和
声道转换的早期取舍。

## Driver Binding

- `ctx + function table` 适合运行期替换、C ABI 或跨语言边界，但会擦除类型并引入间接调用和手工表维护。
- concept/template 保留类型检查、内联和编译期裁剪，但固定 implementation，并可能增加实例化与构建耦合。
- Backend 内部优先 typed/static；只有真实替换或 ABI 隔离需求的 source/sink 边界才使用窄 type erasure。
- Audio 不定义仓库级通用 Driver 模型；binding 由 consumer、lifetime 与替换需求决定。

## Buffer 与 DSP

- 容量、水位和请求以 frame 为主单位；byte 换算必须按完整 `frame_size`，禁止半帧读写。
- PCM FIFO 的 producer/consumer ownership 唯一；atomic 与 memory order 以当前实现为准。
- DSP graph 使用固定容量、block 和拓扑顺序。节点只有显式声明支持时才能 in-place，不能从相同 sample
  type 推断 alias safety。
- Frame queue 隔离 decode/DSP 与 sink 节奏，不是通用消息队列，也不自动解决 backpressure。
- Watermark、period、chunk 和 FIFO 容量由格式、sink 请求与实测 jitter 推导，不冻结统一毫秒参数。

## Reconfigure Transaction

```text
prepare resources -> stop sink -> clear old queues -> configure/open sink -> prefill -> start -> commit
```

失败时实时侧必须停止访问即将失效的 buffer。仅当旧资源仍完整时才允许 rollback；否则进入 Idle/Error，
保持 sink 停止且队列已清理，禁止在半重配状态继续播放。

## Channel Conversion

早期最小策略只覆盖 mono/stereo：`1 -> 2` 复制 sample；`2 -> 1` 用 64-bit 中间和求平均后 clamp；转换
发生在非实时 S32 域并先于最终量化。`>2` channels 没有显式 mixer/downmix policy 时返回
`not_supported`，不能静默截断。

这些规则不替代产品 mixer、loudness 或 channel-layout policy。SDL callback 与 DMA IRQ 即使消费同一
fill contract，也仍由 backend 分别负责 timing、alignment、cache、IRQ 和 start/stop ownership。
