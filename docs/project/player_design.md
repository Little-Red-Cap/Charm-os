# Player Design

本文档是 Player 的“总览与约定”，用于沉淀架构、资源、外设与验证状态。
后续所有开发应以此为基线更新。

目录
- 目标与约束
- 双核职责划分
- 内存分区与预算
- 外设与引脚 Map
- 缓冲与数据路径
- 已验证能力
- 待验证清单
- 规划与里程碑

目标与约束
- 芯片：STM32H747
- 目标形态：高质量综合 Player（音频 + UI + 存储 + 外设）
- 关键约束：DMA 内存区限制，RAM_D1 资源紧张，Audio 稳定时基优先

双核职责划分（建议）
- CM7（主核）
  - UI/渲染、音频解码、混音/EQ、文件系统逻辑
  - USB 复合设备控制面（枚举/配置/类管理）
  - 业务逻辑与状态机
- CM4（协核）
  - EMMC/SDIO 读写调度与 DMA 服务
  - Input 采集/去抖/事件压缩
  - 功耗管理、蓝牙/网络协议栈（优先放这里）

内存分区与预算（强制约束）
- RAM_D2（DMA 友好区）
  - USB MSC 读写环、I2S DMA 双缓冲、EMMC DMA 缓冲、网络 RX/TX ring
- DTCM/ITCM（实时小状态）
  - ISR/调度队列头、时基统计，禁止大缓冲
- RAM_D1（通用对象区）
  - 框架对象、状态机、控制结构、少量中缓冲
- SDRAM（多媒体与缓存池）
  - GUI 帧缓冲/双缓冲、图片/字体缓存、音频大环形缓冲

结论
- 没有 SDRAM 很难同时满足 UI + 音频 + 网络 + 字体
- SDRAM 必须作为多媒体与缓存池，RAM_D1 只留核心对象

外设与引脚 Map（待补齐）
- UART：USART1 (PA9/PA10)
- USB：OTG_FS (PA11/PA12)
- SD/MMC：SDMMC1 (PC8/PC9/PC10/PC11/PC12 + PD2)
- I2S：I2S1 (PA4/PA5/PA7/PC4)
- SPI：SPI5 (PK0/PK1/PJ10)
- GPIO：LED/KEY/Encoder/Display Control
- 备注：具体引脚与极性请同步更新

缓冲与数据路径（初版）
- 音频环形缓冲：>= 256 KB（48k/16bit/2ch 约 192 KB/s）
- I2S DMA 双缓冲：2 x 8~16 KB
- EMMC 读缓存：>= 128 KB
- USB MSC 缓冲：64~128 KB
- GUI 帧缓冲：800x480@16bit 约 750 KB（双缓冲 >= 1.5 MB）
- 字体/图片缓存：>= 512 KB

已验证能力（基线）
- USB MSC：可读写
- USB Audio：可播放
- I2S 时钟：PLL2Q 支持 48k

待验证清单
- Audio + MSC 并发稳定性压力测试
- EMMC 读写速率与抖动评估
- GUI 帧缓冲与刷新带宽
- 网络协议栈内存峰值

架构草案 v0.1（合并版）
- 目标：给双核与内存分区一个可执行边界，避免功能堆叠返工
- 范围：STM32H747 + EMMC + USB + GUI + Audio
- 约束：RAM_D1 接近上限；DMA 有内存区限制
- 风险：音频 underrun、GUI 带宽、USB 并行吞吐、网络峰值内存

规划与里程碑
- 下一步：落地内存分区与缓冲配置表（不动功能）
