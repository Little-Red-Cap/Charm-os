# Boot 文档入口

本目录收纳 Charm 的 bootloader 总览、传输方式和启动主线材料。

如果你是第一次进入仓库，先读：

1. [`../overview.md`](../overview.md)
2. [`../architecture_overview.md`](../architecture_overview.md)
3. [`../system/README.md`](../system/README.md)

再回到这里按任务进入。

## 当前文档

- [`bootloader_overview.md`](bootloader_overview.md)
- [`bootloader_xymodem.md`](bootloader_xymodem.md)

## 建议阅读顺序

1. `bootloader_overview.md`
2. `bootloader_xymodem.md`

## 使用提醒

- 这里主要服务 bootloader 规划和下载/启动链路，不直接替代板级 bring-up 文档。
- 如果 boot handoff、Stage1/Stage2、传输协议或存储映射变化，应同步更新这里的材料。
