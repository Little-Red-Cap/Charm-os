# Agent Route Cards

本目录存放 Agent 的第二跳任务卡片。

使用顺序：

1. 先读仓库根目录的 [`AGENTS.md`](../../../AGENTS.md)
2. 按任务类型进入对应 route card
3. 再由 route card 决定要加载哪些 rules / skills / workflows / templates / 契约文档

这些卡片的目标不是替代详细规则，而是缩短“识别任务之后到拿到正确上下文”之间的路径。

## 当前卡片

- [`review.md`](review.md)
- [`codegen.md`](codegen.md)
- [`architecture.md`](architecture.md)
- [`docs.md`](docs.md)
- [`utf8.md`](utf8.md)
- [`init-graph.md`](init-graph.md)
- [`io.md`](io.md)
- [`capability.md`](capability.md)
- [`block-device.md`](block-device.md)
- [`build.md`](build.md)

## 使用提醒

- route card 只负责任务路由，不重复大段规则正文。
- 如果任务已经超出卡片覆盖范围，应继续进入对应的 skill / workflow / template。
- 如果任务类型不明确，先回到 [`../../../AGENTS.md`](../../../AGENTS.md) 重新判断，而不是一次性加载整个 `docs/agent/`。
