# RK3506 首次上板 Bring-up 档位

这份文档只回答一个问题: 真板第一次烧录时，应该先烧什么，看到什么，下一步再做什么。

当前 `targets/rk3506` 已经把首板 bring-up 切成三个显式档位，目的是把“串口是否活着”“只读外设是否可达”“真实 IRQ 是否打通”拆成三个可诊断阶段，而不是第一次就把所有风险绑在一起。

## 档位定义

| 档位 | CMake 变量 | 目标 | 会做什么 | 不会做什么 |
| --- | --- | --- | --- | --- |
| `minimal` | `CHARM_RK3506_BRINGUP_STAGE=minimal` | 先确认镜像进入、UART0 有输出、startup/vector breadcrumb 可读 | 本地 UART0 early init，基础启动日志，异常向量，breadcrumb | 不做只读 GIC/generic-timer 探测，不做真实 IRQ smoke |
| `observe` | `CHARM_RK3506_BRINGUP_STAGE=observe` | 在不打开真实中断路径的前提下，确认 GIC/generic timer 可读 | `minimal` 的全部内容，加只读 generic timer/GIC smoke | 不做真实 IRQ smoke |
| `irq-smoke` | `CHARM_RK3506_BRINGUP_STAGE=irq-smoke` | 确认 generic timer -> GIC -> IRQ handler -> EOI 这条链路真的通 | `observe` 的全部内容，加 one-shot timer IRQ smoke | 不做 MMU/cache/TLB 归一化，不做 DDR 初始化 |

默认档位仍然是 `irq-smoke`，这样不会破坏当前仓库已经具备的验证能力；但第一次上板建议从 `minimal` 开始。

## 推荐上板顺序

1. 先烧 `minimal`
2. 只要能稳定看到 UART0 日志，再切到 `irq-smoke`
3. 如果 `minimal` 正常、`irq-smoke` 失败，就优先看 breadcrumb 和异常日志，不要立刻怀疑整条启动链

`observe` 更适合在你怀疑 GIC 或 generic timer 映射有问题、但又不想马上打开发真实 IRQ 的时候单独插入。

## 直接可用的 Preset

PowerShell 下可以直接用这两组 preset:

```powershell
cmake --preset rk3506-baremetal-image-uart0-minimal-debug
cmake --build --preset rk3506-baremetal-image-uart0-minimal-debug

cmake --preset rk3506-baremetal-image-uart0-irq-smoke-debug
cmake --build --preset rk3506-baremetal-image-uart0-irq-smoke-debug
```

这两个 preset 都会:

- 打开 `CHARM_BUILD_TARGET_BOOTSTRAP=ON`
- 把 early console 固定到 `UART0 @ 0xff0a0000`
- 分别选择 `minimal` 或 `irq-smoke` bring-up 档位

## Startup Breadcrumb

`startup.S` 现在会把最早阶段写进 `g_rk3506_startup_breadcrumb`。这个变量放在 `.data`，不会被 `.bss` 清零覆盖。

| 值 | 含义 |
| --- | --- |
| `0x10` | startup entry |
| `0x20` | async abort / IRQ / FIQ masks applied |
| `0x30` | bootstrap stack ready |
| `0x40` | `.bss` cleared |
| `0x50` | per-mode stacks ready |
| `0x60` | platform reset hook enter |
| `0x70` | platform reset hook done |
| `0x80` | vectors installed |
| `0x90` | boot main entry |
| `0xa0` | early console ready |
| `0xb0` | read-only smoke done |
| `0xc0` | IRQ smoke done |

如果板上死得很早，但还能从异常路径或后续日志看到这个值，我们就知道最后一次确定执行到哪一步。

## Vector Breadcrumb

`vectors.S` 会把异常入口写进 `g_rk3506_vector_breadcrumb`。

| 值 | 含义 |
| --- | --- |
| `0x000` | none |
| `0x101` | undefined entry |
| `0x102` | prefetch abort entry |
| `0x103` | data abort entry |
| `0x104` | reserved entry |
| `0x105` | IRQ entry |
| `0x106` | FIQ entry |
| `0x107` | SVC entry |

异常打印现在会同时输出:

- bring-up profile
- startup breadcrumb 及其名字
- vector breadcrumb 及其名字
- 异常 frame 的 SPSR / CPSR / return PC / LR / R0-R3 / R12

这意味着第一次上板时，即使没有 JTAG，也能先靠串口把“死在启动前半段”与“死在某个具体异常向量”区分开。

## 这一步还没有解决什么

- 没有初始化 DDR
- 没有处理 BootROM/SPL 那种 SRAM 极早期多阶段启动
- 没有做 MMU/cache/TLB 的最终归一化
- 没有确认真板 generic timer 最终使用 PPI 29 还是 30 之外的特殊路由

所以这套档位的定位很明确: 它是“首次烧录前的可诊断性收口”，不是完整 boot chain。
