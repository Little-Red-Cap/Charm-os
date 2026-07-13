# 参考资料入口

本目录收纳外部框架对照、既有讨论摘录和可借鉴材料。它们用于比较、启发和校准，不直接充当 Charm 当前实现的契约入口。

## 当前状态

- VSF 相关材料主要形成于 Charm 早期借鉴阶段，现在更多承担“历史来源 / 概念对照 / 偶发考古”职责。
- 如果某条结论已经被吸收到 [`../architecture/README.md`](../architecture/README.md)、[`../system/README.md`](../system/README.md) 或其它专题入口中，应优先以上位入口为准。
- 当某份第三方参考只剩历史价值、且不再服务当前决策时，可以继续降级或归档，不必长期保留成专题首页。

如果你是第一次进入仓库，先读：

1. [`../overview.md`](../overview.md)
2. [`../architecture_overview.md`](../architecture_overview.md)
3. [`../README.md`](../README.md)

## 按任务进入

### 我想看 VSF 对照

先读：

- [`vsf/README.md`](vsf/README.md)

### 我想看早期音频架构取舍

原始对话转储已去除；driver binding、pull/DMA、graph 与 reconfigure 的独立取舍压缩在
[`audio_design_retained_notes.md`](../archive/audio-v0/audio_design_retained_notes.md)。

### 我想看 FileX / MAL 对照

- [`filex_charm_map.md`](filex_charm_map.md)

## 使用提醒

- 这里的材料默认是“参考 / 对照 / 讨论”，不是“现行入口 / 稳定契约”。
- 如果与 [`../architecture/README.md`](../architecture/README.md)、[`../system/README.md`](../system/README.md) 或 [`../audio/README.md`](../audio/README.md) 冲突，应以上位入口及对应契约为准。
