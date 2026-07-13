# Player H747 资源规划保留笔记

> status: `archived`
>
> scope: 早期 Player H747 双核、内存与压力验证假设

本文从旧 `docs/project/player_design.md` 提取仍有讨论价值的资源问题。原稿中的引脚表、已验证能力、
RAM/FLASH 占用和下一步排期没有可复验来源，不作为当前板级事实。当前状态必须检查 Player/H747
源码、linker map、CMake target 和当次实板日志。

## 双核 ownership 假设

早期方案曾考虑：

- CM7 承担 UI/渲染、音频解码、混音、文件系统控制面和产品状态机；
- CM4 承担 eMMC/SDIO 调度、DMA 服务、输入采集、功耗管理或部分通信协议。

该划分只是一种候选方案。真实实现需要先确认外设实例归属、共享内存、cache coherency、IPC、
中断路由、故障恢复和两核独立升级边界，不能按任务名称直接分核。

## 内存域约束

早期规划提出以下分区方向：

- DMA 可达区域承载 USB、I2S、eMMC 和网络 ring/buffer；
- DTCM/ITCM 只放实时小状态、ISR 数据和关键代码，不承载大缓冲；
- D1 RAM 承载 runtime 对象、状态机和控制结构；
- SDRAM 承载 framebuffer、图片/字体缓存和较大的音频缓存。

这不是固定布局。每个 buffer 必须依据具体 DMA master 可达性、MPU/cache 属性、alignment、峰值容量
和 ownership 决定位置；“放进 SDRAM”不能替代 coherency 与带宽验证。

## 历史容量估算

原稿曾用以下数量级检查方案是否可行：

- 48 kHz、16-bit、双声道 PCM 约 `192 KiB/s`；
- `800x480@16-bit` 单 framebuffer 约 `750 KiB`；
- 音频环形缓冲、eMMC read-ahead、USB MSC 缓冲和字体/图片缓存需要分别计入峰值；
- 双缓冲、并发 DMA 和 cache-line 对齐会增加实际占用。

这些是估算方法，不是当前配置值。当前预算应由 linker map、静态 admission、运行期 high-water mark
和具体 workload 共同证明。

## 仍有效的验证问题

- Audio 与 MSC/eMMC 并发时的吞吐、underrun/overrun 和延迟尾部；
- framebuffer 刷新、滚动和 alpha blend 对 SDRAM 带宽的影响；
- 网络、解码、字体和图片缓存同时活跃时的内存峰值；
- DMA buffer 所在内存域、cache maintenance 和 producer/consumer ownership 是否一致；
- 多核方案下 IPC backlog、共享 buffer 生命周期和单核故障后的恢复行为。

这些问题只有进入可重复的 workload、计数器和 Host/QEMU/实板分域证据后，才能成为当前结论。
