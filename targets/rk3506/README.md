# RK3506 Target

本目录是 RK3506 Cortex-A7 的 bare-metal 叶子 target。源码和 CMake 配置是当前
实现事实；板级寄存器资料与前级交接条件分别见：

- [`../../docs/board/rk3506/README.md`](../../docs/board/rk3506/README.md)
- [`../../docs/board/rk3506/post_ddr_handoff_contract.md`](../../docs/board/rk3506/post_ddr_handoff_contract.md)

## 模型边界

公开模型是单核、ARM state、little-endian、normal-world PL1 的 post-DDR
单镜像 payload。BootROM、RockUSB、DDR 训练、SPL/U-Boot 阶段、PSCI 和次级核
拉起不属于该 target。

该叶子负责：

- 建立各异常模式栈，清理 BSS，安装低向量并进入 `rk3506_boot_main()`；
- 初始化默认 `UART0 @ 0xff0a0000` early console；
- 记录 CPU、地址空间、CRU/GPIO、GIC 和 generic timer 状态；
- 提供只读观测与一次性、可返回的 generic timer IRQ smoke。

它尚不负责 DDR 初始化、MMU/cache/TLB 最终归一化、周期 tick 或完整中断框架。

## 构建入口

目标名为 `rk3506-baremetal-skeleton`。当前 preset：

| Preset | Profile |
|---|---|
| `rk3506-baremetal-image-debug` | 默认 `irq-smoke` |
| `rk3506-baremetal-image-uart0-minimal-debug` | `minimal` |
| `rk3506-baremetal-image-uart0-irq-smoke-debug` | `irq-smoke` |

`CHARM_RK3506_BRINGUP_STAGE` 只接受：

| 值 | 行为 |
|---|---|
| `minimal` | early UART、向量和 breadcrumb，不访问 GIC/timer smoke |
| `observe` | 在 `minimal` 基础上只读采样 GIC 与 generic timer |
| `irq-smoke` | 在 `observe` 基础上执行一次 timer IRQ、EOIR 并返回 |

首次上板先用 `minimal`；串口稳定后再用 `irq-smoke`。仅当需要区分 MMIO
可读性和中断路由问题时使用 `observe`。

## 诊断标记

`g_rk3506_startup_breadcrumb` 记录最后完成的启动步骤：

| 值 | 阶段 |
|---|---|
| `0x10` | startup entry |
| `0x20` | A/I/F masked |
| `0x30` | bootstrap stack ready |
| `0x40` | BSS cleared |
| `0x50` | mode stacks ready |
| `0x60` / `0x70` | reset hook enter / done |
| `0x80` | vectors installed |
| `0x90` | boot main entry |
| `0xa0` | early console ready |
| `0xb0` | read-only smoke done |
| `0xc0` | IRQ smoke done |

`g_rk3506_vector_breadcrumb` 记录异常入口：

| 值 | 异常 |
|---|---|
| `0x101` | undefined |
| `0x102` | prefetch abort |
| `0x103` | data abort |
| `0x104` | reserved |
| `0x105` | IRQ |
| `0x106` | FIQ |
| `0x107` | SVC |

fatal 异常输出 profile、两组 breadcrumb、SPSR/CPSR、return PC、LR、R0-R3
和 R12。IRQ 仅在 timer smoke 窗口内可返回，其余 IRQ 仍进入 fatal 路径。

## Timer IRQ 判定

默认 `CHARM_RK3506_GENERIC_TIMER_EXPECTED_INTID=30`，对应 normal-world PL1
的 non-secure physical timer。bring-up 实现同时识别 `29` 和 `30`，但分别输出：

- `timer source recognized`：中断来自已知 physical timer PPI；
- `matches expected intid`：中断同时符合默认 handoff 契约。

识别到 `29` 只能证明 IRQ 链路可达，不能证明前级安全态与默认契约一致。

## 配置事实

地址、时钟、镜像跨度和栈大小定义在
[`CharmTargetConfig.cmake`](CharmTargetConfig.cmake)，编译映射与 linker script
生成定义在 [`sources.cmake`](sources.cmake)。这些值中标为 `provisional` 的项仍需
TRM、前级配置或实板证据确认，README 不复制它们作为永久 SoC 事实。
