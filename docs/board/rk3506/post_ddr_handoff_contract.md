# RK3506 Post-DDR Handoff Contract v0

这份文档用于明确当前 `targets/rk3506` 的公开模型到底是什么，以及前级加载器最少需要把板子交接到什么状态。

一句话版本：

当前 `targets/rk3506` 公开表达的是一个 `post-DDR bare-metal payload`，不是整套 Rockchip staged boot 流程。

也就是说：

- 前级负责把镜像送进可执行内存，并把 CPU 安全地跳转过来
- `targets/rk3506` 负责从这个 handoff 点开始继续 Cortex-A7 的后半段 bring-up
- BootROM、RockUSB、DDR 训练、SPL/U-Boot 内部分段，不进入当前公共启动模型

## 1. 前级最少需要保证什么

### 1.1 内存与镜像

前级至少需要保证：

- DDR 已可用
- 裸机镜像已经落到最终执行地址
- 镜像、BSS、栈和向量表所在区域都可访问
- 跳转后不会再有别的前级代码改写这片内存

当前 `rk3506` 叶子 target 的默认构建参数仍然是 bring-up 默认值，不冒充芯片最终真相：

- `CHARM_RK3506_IMAGE_TEXT_BASE = 0x00200000`
- `CHARM_RK3506_IMAGE_RAM_LENGTH = 0x00400000`
- `CHARM_RK3506_SDRAM_BASE = 0x00000000`
- `CHARM_RK3506_SYSTEM_SRAM_BASE = 0xfff80000`
- `CHARM_RK3506_SYSTEM_SRAM_SIZE = 0x0000c000`

### 1.2 CPU 入口状态

当前最稳的 handoff 目标是：

- 单核进入
- `cpu0` 进入
- ARM state
- little-endian
- PL1 特权态
- 默认按 `normal-world PL1` 理解

当前不在契约内的入口状态包括：

- Hyp 直接进入
- Monitor 直接进入
- 多核同时跳进来

不是说这些永远不支持，而是当前代码和文档还没有把它们作为稳定入口来收口。

### 1.3 中断与异常环境

前级跳转前最好保证：

- IRQ / FIQ 不处在会立刻抢占的状态
- 没有明显未清的活跃中断源
- 定时器、看门狗、mailbox 等不会在我们安装本地向量前突然打进来

当前 [`targets/rk3506/startup.S`](../../../targets/rk3506/startup.S) 会在 very-early 阶段重新屏蔽 `A/I/F`，然后再清 BSS、建 per-mode stack、安装本地向量。

这意味着：

- 前级不一定非得把一切都“清零”后再交接
- 但不能把系统留在一个“马上就会炸”的异常环境里

### 1.4 MMU / cache / branch predictor

当前最保守也最推荐的 handoff 是：

- MMU 关闭
- I-cache 关闭
- D-cache 关闭
- branch prediction 关闭

如果前级希望带着 MMU/cache 打开的状态直接跳过来，就至少还要保证：

- 当前映射下能正确取到我们的入口代码
- 镜像、BSS、栈和向量表地址在当前映射下都可访问
- 如果前级做过内存搬运，相关 dirty cache line 已经 clean 到 PoC
- 留下的 memory attribute 不会让 MMIO 访问或向量切换立刻出错

这条路径现在还没有被当作稳定默认路径验证，所以不建议拿它当第一版真板 handoff。

## 2. 早期串口契约

当前 `targets/rk3506` 的 early console 契约是：

- 默认 early UART 是 `UART0 @ 0xff0a0000`
- 当 `EARLY_UART_BASE` 仍是 `UART0` 时，平台代码会在本地完成最小 bring-up：
  - 打开 `CRU` 中与 UART0 相关的 gate
  - 把 `SCLK_UART0` 收口到 `XIN24M`
  - 把 `GPIO0_C6/C7` 切到 `func1`
  - 按 `115200 8N1` 初始化 UART0
- 同时记录 `divisor`、`CRU` 与 `GPIO0C` 的 readback，方便对照真板状态

如果板级把 `EARLY_UART_BASE` 覆盖到别的 UART，比如 `UART4`，那么当前仍默认由前级保证对应的：

- 时钟
- pinmux
- UART 基本可轮询输出

## 3. GIC / generic timer 契约

前级当前不必帮我们完成：

- GIC 初始化
- generic timer 初始化
- 最终中断路由策略
- 更高层的时钟或调度抽象

但前级需要避免：

- 留下会在 handoff 后立刻打进来的旧中断
- 让定时器或看门狗在我们接管前处于不可预测的活跃状态

### 3.1 当前默认 timer IRQ 契约

当前公开契约现在已经显式写进 CMake 配置：

- `CHARM_RK3506_GENERIC_TIMER_EXPECTED_INTID = 30`

默认含义是：

- handoff 目标是 `normal-world PL1`
- 因此平台默认期望使用 `non-secure physical timer`
- 对应 GIC `intid 30`

### 3.2 为什么实现仍接受 29/30

当前 timer IRQ smoke 仍然同时识别：

- `29`：secure physical timer
- `30`：non-secure physical timer

这是 bring-up 初期的策略性宽容，不是契约含糊。

日志里现在会同时区分：

- `timer source recognized`
- `matches expected intid`

因此：

- 如果看到 `recognized = 1`，说明 IRQ 链路已经活了
- 如果同时 `matches expected intid = 1`，说明它也符合当前默认 handoff 契约
- 如果只 `recognized = 1` 但 `matches expected intid = 0`，说明 bring-up 先成功了，但平台事实和默认契约还需要对齐

## 4. 当前明确不纳入公共模型的东西

下面这些内容现在仍然属于前级或专项研究范围，不进入当前公共 boot 模型：

- BootROM 下载协议
- RockUSB / loader mode
- vendor DDR 训练细节
- SPL / U-Boot 的内部阶段划分
- Linux AMP / RPMsg 共享内存布局
- PSCI / secondary core release 的完整流程

这些以后都可以继续研究，但不应该反向把仓库重新塑造成“必须 staged boot 才能理解”的结构。

## 5. 真板 bring-up 时优先看什么

如果第一轮真板行为不对，建议优先按这个顺序检查：

1. 前级 handoff 是否满足本文件描述的最小契约
2. early console 是不是走在当前最稳的 `UART0` 路径上
3. 启动日志里的 `CPSR/SCTLR/VBAR/CNTFRQ/GIC` 快照是否合理
4. timer IRQ smoke 是不是至少达到了 `timer source recognized = 1`
5. 如果已经识别到 timer，但 `matches expected intid = 0`，再回头判断 handoff 安全态与 timer 路由

## 6. 后续如何放宽契约

这份 v0 契约是故意保守的，目的是先把真板 bring-up 跑起来。

后续可以逐步放宽，例如：

1. 让 payload 主动归一化 MMU/cache/branch predictor 状态
2. 把本地 early init 从 `UART0` 扩到更多板级 UART 路径
3. 再引入 secondary cores、PSCI、Hyp/Monitor 等更复杂的 handoff

在这些切片真正落地之前，继续坚持这份保守的 `post-DDR` 契约会更稳。
