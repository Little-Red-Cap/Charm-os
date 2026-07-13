# Audio 早期设计保留说明

> **状态：`archived`**
>
> 本文从早期 Pull Engine、Audio v1 和 L1/L2/L3 草案中保留尚有独立价值的技术取舍。
> 当前行为见 [`charm_audio_architecture.md`](../../system/charm_audio_architecture.md)、
> [`audio_design_v1.md`](../../audio/audio_design_v1.md) 与源码。

## 原始目标

Host callback 与 MCU DMA half/full IRQ 被建模为同一种 pull 请求：设备给出目标 buffer，实时侧只从
FIFO 取完整 frame、不足补零并更新 underrun 计数；读取文件、解码、DSP 和 refill 留在非实时侧。
这种同构只覆盖填充语义，不证明两端具有相同调度抖动、cache、IRQ 或时钟行为。

早期主数据路径选择 S32 interleaved 作为处理域，在写入 PCM FIFO 前量化为 S16。该选择后来进入
当前实现，但本文不冻结未来格式。

## Buffer 与 DSP

- 容量、请求和水位应以 frame 为主要单位；换算到 byte 时必须使用完整 `frame_size`，读写不得产生
  半帧。
- PCM FIFO 的 producer 与 consumer ownership 必须唯一；具体 atomic 和 memory order 以当前实现为准。
- DSP graph 使用固定容量、固定 block 和拓扑顺序。节点只有显式声明支持时才能原地处理，不能因
  输入输出类型相同就假设 alias 安全。
- Frame queue 用于隔离 decode/DSP 与 sink 消费节奏；它不是通用跨线程消息队列，也不自动解决
  backpressure。
- 水位、period、chunk 和 FIFO 容量应由格式、sink 请求和实测抖动推导，不保留统一的
  `10ms/40ms/150ms` 默认值。

## Reconfigure 事务

早期设计把 format change 视为非实时事务：

```text
prepare new format/resources
-> stop sink consumption
-> clear old frame queue and FIFO
-> configure/open sink
-> prefill
-> start sink
-> commit
```

任何步骤失败都必须让实时侧停止访问即将失效的 buffer。只有旧资源仍完整时才能回滚；否则进入
Idle/Error，并保持 sink 停止、队列已清理。禁止在半重配状态继续播放。

## 声道转换

早期 MVP 只讨论 mono/stereo：

- `1 -> 2` 复制每个 mono sample；
- `2 -> 1` 使用 64-bit 中间和计算 `(L + R) / 2`，再按目标格式 clamp；
- 转换位于非实时 S32 域，并在最终量化之前完成；
- `>2` channels 没有显式 mixer/downmix policy 时返回 `not_supported`，不能静默截断。

这些规则描述最小退化策略，不代替产品 mixer、响度或声道布局决策。

## DMA/backend 映射

- SDL callback 与 DMA half/full callback 都可以消费同一个 fill contract；DMA backend 仍独立负责对齐、
  section、cache maintenance、IRQ ownership 和启动/停止顺序。
- 双缓冲大小从 sample rate、channels、sample size 与 period frames 推导；固定地址和 SRAM/SDRAM
  区域属于 board profile。
- underrun counter 是观测值，不自动等于 fatal error；持续 underrun 的处置由 player/backend policy
  决定。

旧文档中的接口代码、伪实测日志、推荐毫秒值、Player 状态机、TODO、版本路线和固定 MCU 参数已删除。
需要追溯原文时使用 Git 历史，不能恢复为当前默认值。
