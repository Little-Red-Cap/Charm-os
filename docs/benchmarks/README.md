# Benchmark 文档入口

本目录收纳性能、吞吐、延迟与 escape hatch 讨论相关的 benchmark 落点，用来沉淀“为什么值得为某条路径做特化”的证据。

如果你是第一次进入仓库，先读：

1. [`../overview.md`](../overview.md)
2. [`../architecture_overview.md`](../architecture_overview.md)
3. [`../README.md`](../README.md)

## 当前文档

- [`spi_transfer.md`](spi_transfer.md)

## 建议阅读顺序

1. `spi_transfer.md`

## 使用提醒

- benchmark 如果要作为设计决策依据，至少应记录平台、编译器、基线、优化实现、测量方法与样本规模。
- 如果当前仍是占位页，应明确写出“暂无正式数据”，避免被误读为已经验证完成。
