# RK3506 Post-DDR Handoff Contract

> status: supporting
>
> 本文只约束前级 loader 跳入 `targets/rk3506` 时的机器状态。地址和构建默认值
> 以 [`../../../targets/rk3506/CharmTargetConfig.cmake`](../../../targets/rk3506/CharmTargetConfig.cmake)
> 为准。

## 公开入口模型

`targets/rk3506` 是 post-DDR bare-metal payload，不是 BootROM、DDR loader、SPL
或 U-Boot 的替代品。支持的入口状态为：

- `cpu0` 单核进入；
- ARM state、little-endian；
- normal-world PL1 特权态；
- MMU、I-cache、D-cache 和 branch prediction 关闭；
- 镜像入口符合当前 linker layout。

Hyp、Monitor、多核同时进入，以及携带既有 MMU/cache 状态进入，不属于当前契约。

## 前级责任

跳转前必须满足：

### 内存与镜像

- DDR 已初始化且目标范围可读、可写、可执行；
- 完整镜像已落到 linker 指定地址；
- text、data、BSS、各模式栈和向量表均在有效范围内；
- 前级不再修改或复用上述内存；
- 若镜像由 cacheable 路径搬运，数据已 clean 到 PoC，并完成必要屏障。

### CPU 与异常环境

- 当前模式允许修改 CPSR、SCTLR、VBAR 和各异常模式的 SP；
- 没有会在本地向量安装前立即触发的 IRQ、FIQ 或 asynchronous abort；
- GIC 不留 active interrupt；timer、watchdog 和 mailbox 不处于不可预测的活动状态；
- 不依赖前级页表、向量表、栈或异常 handler 在跳转后继续存在。

前级不必初始化 Charm 使用的 GIC 或 generic timer，但必须把它们留在可接管状态。

## Target 接管

[`startup.S`](../../../targets/rk3506/startup.S) 在入口后：

1. 屏蔽 asynchronous abort、IRQ 和 FIQ；
2. 建立 bootstrap stack，清 BSS；
3. 建立 UND、ABT、IRQ、FIQ、SVC 和 SYS 栈；
4. 记录入口 CPSR 与 MMU/cache/vector 状态；
5. 清除 high-vector 选择，写入本地 VBAR；
6. 以 SYS mode 进入 `rk3506_boot_main()`。

该序列不会关闭或清理前级留下的 MMU、cache、branch predictor 和页表，因此这些
状态必须在交接前符合上面的关闭条件。

## Early Console

默认 `UART0 @ 0xff0a0000` 路径由 target 本地完成 clock、GPIO0_C6/C7 pinmux
和 `115200 8N1` 初始化。若 `CHARM_RK3506_EARLY_UART_BASE` 覆盖为其它 UART，
前级必须先完成该 UART 的 clock、pinmux 和轮询发送配置。

## GIC 与 Generic Timer

默认 handoff 期望 non-secure physical timer，对应
`CHARM_RK3506_GENERIC_TIMER_EXPECTED_INTID=30`。bring-up smoke 同时识别：

- `29`：secure physical timer；
- `30`：non-secure physical timer。

命中 `29` 只表示 timer IRQ 链路可达，不表示交接状态符合默认契约。详细诊断字段
见 [`../../../targets/rk3506/README.md`](../../../targets/rk3506/README.md)。

## 不属于本契约

- BootROM 下载协议和 RockUSB loader mode；
- DDRC/DDRPHY 初始化与训练；
- SPL/U-Boot 内部阶段；
- secure boot、OTP 和量产烧录；
- PSCI、secondary core release、AMP/RPMsg 布局；
- 周期 tick、调度、完整中断框架和 MMU/cache 运行期策略。

这些能力若落地，应由各自的板级或 runtime 契约负责，不能隐式扩大本 handoff。

## 失败定位

- 无任何串口输出：先核对镜像地址、CPU state、DDR 与默认 UART0 路径；
- 有 fatal 输出：使用 startup/vector breadcrumb 和异常 frame 定位最后阶段；
- `minimal` 通过而 `observe` 失败：核对 GIC/timer MMIO 与前级电源/时钟状态；
- `observe` 通过而 `irq-smoke` 失败：核对 GIC PPI、security group 和 INTID；
- `recognized=1` 但 `matches expected intid=0`：交接安全态或 timer 选择与默认契约不一致。
