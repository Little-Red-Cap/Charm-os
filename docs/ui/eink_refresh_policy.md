# EInk Refresh Policy 候选边界

> `status`: `exploration`

仓库当前没有 EInk refresh policy、partial/full refresh backend 或 dirty-tile 决策实现。本文只保留
未来真实 panel backend 需要回答的问题，不定义默认参数或 Vivid 已有能力。

## Ownership

UI/widget 只提交 invalidation 与 DrawCmd；executor/backend 产生 clipped dirty region；display policy 决定
full/partial refresh、ghosting control、pacing 和 panel waveform。Policy 不改写 layout、widget state 或
semantic action。

```text
UI invalidation -> DrawCmd/backend dirty region -> display policy -> panel refresh
```

## 候选决策

真实实现至少需要区分 full、partial 与 automatic policy，并明确：

- dirty area 的计算、合并、clipping 和空区域行为；
- 连续 partial refresh、elapsed time、ghosting 与强制 full refresh 条件；
- panel LUT/waveform、temperature、pixel format 和 dithering 对策略的影响；
- refresh in-flight 时新 dirty region 的 queue/coalesce/drop 行为；
- timeout、panel fault、cancel 和 shutdown 后的状态恢复。

阈值、计数、时间窗口和 LUT 都是 panel/profile 实测结果，不在公共文档给出伪默认。

## 准入证据

提升为 supporting/contract 前，必须有真实 backend consumer、明确状态机与失败返回、Host policy fixture，
以及至少一块真实 panel 的 full/partial refresh capture。Host dirty-region 测试不能证明 waveform、ghosting、
timing、power 或视觉质量。
