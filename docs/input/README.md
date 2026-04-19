# 输入文档入口

本目录收纳 Charm 输入链路的分层决策与协议映射，其中 `input_protocol_map.md` 主要保留为历史协议对照。

如果你是第一次进入仓库，先读：

1. [`../overview.md`](../overview.md)
2. [`../architecture_overview.md`](../architecture_overview.md)
3. [`../README.md`](../README.md)

再回到这里按任务进入。

## 当前文档

- [`input_layering_decision.md`](input_layering_decision.md)
- [`input_protocol_map.md`](input_protocol_map.md)（历史对照：位段编码与字段映射）

## 建议阅读顺序

1. `input_layering_decision.md`
2. `input_protocol_map.md`（仅在需要看协议编码 / 历史映射时）

## 使用提醒

- 这里更偏输入链路的分层和映射，不承担总架构入口职责。
- `input_protocol_map.md` 更适合拿来对照编码形状，不默认等同于当前输入主契约。
- 如果输入语义、HAL 边界或 UI 输入链路变化，应同步更新本目录文档。
