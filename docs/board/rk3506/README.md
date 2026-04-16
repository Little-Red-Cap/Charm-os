# RK3506 上板资料收口（初版）

本文用于收敛 RK3506 早期 bring-up / Bootloader / 平台叶子 target 接入时最常查的资料。

当前版本主要基于用户提供的 RK3506 SDK 中可公开读取的 DTS、U-Boot 配置与头文件，因此文中会刻意区分三类信息：
- SoC 级事实：在 DTS、绑定头文件、U-Boot 配置中交叉出现，可以直接作为 bring-up 起点
- 当前软件栈约定：例如 SPL boot order、text base、默认 console，这些适合参考，但不等于 SoC 唯一真相
- 待补项：需要 TRM、板卡原理图或实板验证后才能定稿

如果当前问题不是“寄存器和地址在哪里”，而是“前级最少要保证什么状态才能跳到我们的裸机镜像”，请配合阅读 `docs/board/rk3506/post_ddr_handoff_contract.md`。

## 1. 资料来源

以下路径均相对于当前使用的 RK3506 SDK 根目录：

- `u-boot/arch/arm/dts/rk3506.dtsi`
- `u-boot/arch/arm/dts/rk3506-u-boot.dtsi`
- `u-boot/arch/arm/dts/rk3506-evb.dts`
- `u-boot/arch/arm/dts/rk3506-evb-tb.dts`
- `u-boot/include/configs/rk3506_common.h`
- `u-boot/include/configs/evb_rk3506.h`
- `u-boot/include/dt-bindings/soc/rockchip,boot-mode.h`
- `u-boot/include/dt-bindings/clock/rockchip,rk3506-cru.h`
- `u-boot/arch/arm/include/asm/arch-rockchip/cru_rk3506.h`
- `u-boot/arch/arm/include/asm/arch-rockchip/grf_rk3506.h`
- `u-boot/arch/arm/include/asm/arch-rockchip/ioc_rk3506.h`
- `kernel-6.1/arch/arm/boot/dts/rk3506.dtsi`
- `kernel-6.1/arch/arm/boot/dts/rk3506-evb1-v10.dtsi`
- `kernel-6.1/arch/arm/boot/dts/rk3506-evb2-v10.dtsi`
- `kernel-6.1/arch/arm/boot/dts/rk3506-pinctrl.dtsi`

## 2. SoC 快照

- CPU：3 x `arm,cortex-a7`
- 中断控制器：`arm,gic-400`
- 核心定时器：`arm,armv7-timer`
- generic timer 计数频率：`24 MHz`
- 当前 DTS 中次级 CPU 使用 `enable-method = "psci"`，说明多核拉起与电源状态管理不能塞进通用 Boot 逻辑
- 当前最稳妥的早期串口默认值是 `UART0`

这和仓库里当前的 `platform.board.armv7a_stub` 骨架是对得上的：
- 需要显式表达 payload 落点
- 需要显式表达向量基址与页表基址
- 需要把中断屏蔽、映射切换、cache/TLB 维护、向量切换、同步屏障做成目标相关叶子实现

## 3. 关键地址速查

### 3.1 启动早期最常用

| 模块 | 基地址 / 信息 | 说明 |
| --- | --- | --- |
| GIC Distributor | `0xff581000` | U-Boot 也显式定义了 `GICD_BASE` |
| GIC CPU Interface | `0xff582000` | U-Boot 也显式定义了 `GICC_BASE` |
| GIC 额外寄存器块 | `0xff584000`, `0xff586000` | 来自 `gic` 节点 `reg` 元组，后续若涉及 virtualization / hyp 相关能力可继续深挖 |
| UART0 | `0xff0a0000` | 推荐早期 console，IRQ `34` |
| UART1 | `0xff0b0000` | IRQ `35` |
| UART2 | `0xff0c0000` | IRQ `36` |
| UART3 | `0xff0d0000` | IRQ `37` |
| UART4 | `0xff0e0000` | IRQ `38` |
| UART5 | `0xff4e0000` | IRQ `39` |
| GRF | `0xff288000` | 主 GRF |
| IOC_GRF | `0xff4d8000` | IO config / pinmux 相关入口之一 |
| OTP | `0xff4f0000` | 普通 OTP |
| Secure OTP | `0xff520000` | SPL DTS 中可见 |
| IOC1 | `0xff660000` | 另一组 IOC syscon |
| GRF_PMU | `0xff910000` | reboot mode 等 PMU 相关寄存器入口 |
| IOC_PMU | `0xff950000` | PMU 域 IO config |
| CRU | `0xff9a0000` | 时钟 / reset 主入口 |
| DMAC0 | `0xff000000` | IRQ `116`, `117` |
| DMAC1 | `0xff008000` | IRQ `118`, `119` |
| WDT0 | `0xff260000` | IRQ `107` |
| WDT1 | `0xff268000` | IRQ `108` |
| Mailbox0 | `0xff290000` | IRQ `138` |
| Mailbox1 | `0xff291000` | IRQ `139` |
| Mailbox2 | `0xff292000` | IRQ `140` |
| Mailbox3 | `0xff293000` | IRQ `141` |
| USB2 OTG0 | `0xff740000` | IRQ `74` |
| USB2 OTG1 | `0xff780000` | IRQ `79` |
| DSMC | `0xff8b0000` | IRQ `126` |

### 3.2 GPIO bank 与中断入口

| GPIO bank | 基地址 | GIC SPI |
| --- | --- | --- |
| GPIO0 | `0xff940000` | `0` |
| GPIO1 | `0xff870000` | `4` |
| GPIO2 | `0xff1c0000` | `8` |
| GPIO3 | `0xff1d0000` | `12` |
| GPIO4 | `0xff1e0000` | `16` |

### 3.3 定时器说明

- 当前公开 DTS 对早期 bring-up 最直接有用的是 ARM generic timer，而不是一个显式 MMIO `timer@...` 节点
- generic timer 中断号来自 PPI：`13 / 14 / 11 / 10`
- 计数频率在 DTS 与 U-Boot 配置里都落在 `24 MHz`
- 如果后续要走片上外设计时器而不是 generic timer，需要继续从 `CRU` 的 `PCLK_TIMER`、`CLK_TIMER0_CH0..CH5` 等时钟定义往下追

## 4. BootROM / SPL / 启动介质线索

### 4.1 当前软件栈默认顺序

- `rk3506-u-boot.dtsi` 中：
  - `stdout-path = &uart0`
  - `u-boot,spl-boot-order = &mmc, &spi_nand, &spi_nor`
- `rk3506-evb-tb.dts` 中把顺序改成了：
  - `&spi_nand, &spi_nor, &mmc`

这说明：
- 启动介质顺序确实是板级可变项，不应写死在 Charm 公共 Boot 逻辑里
- 叶子 target 或板级配置对象需要拥有这类差异

### 4.2 reboot mode 与下载模式线索

`grf_pmu.reboot_mode` 节点给出了很强的 BootROM / loader 流程线索：

- `mode-bootloader = <BOOT_BL_DOWNLOAD>`
- `mode-loader = <BOOT_BL_DOWNLOAD>`
- `mode-normal = <BOOT_NORMAL>`
- `mode-recovery = <BOOT_RECOVERY>`
- `mode-fastboot = <BOOT_FASTBOOT>`
- `mode-ums = <BOOT_UMS>`
- `mode-panic = <BOOT_PANIC>`
- `mode-watchdog = <BOOT_WATCHDOG>`
- `mode-charge = <BOOT_CHARGING>`

在 `u-boot/include/dt-bindings/soc/rockchip,boot-mode.h` 中可以看到：
- `BOOT_BL_DOWNLOAD = REBOOT_FLAG + 1`
- 注释语义是“enter bootloader rockusb mode”

这意味着：
- Rockchip 下载链路不只是“某个普通二级 boot mode”，而是 SoC 级 reboot-mode 协议的一部分
- 后续如果要对接真正的上板下载 / 恢复链路，`GRF_PMU + reboot mode + RockUSB/loader` 这条线必须单独整理

### 4.3 Secure OTP 线索

SPL DTS 里还能看到：

- `secure-otp@ff520000`
- `secure_conf = <0xff210100>`
- `cru_rst_addr = <0xff9a8080>`
- `mask_addr = <0xff528000>`

这些字段说明：
- RK3506 的早期启动链路里已经把 secure OTP / reset gating 纳入 SPL 视野
- 真正进入安全启动、下载限制、量产熔丝路径之前，需要再补一轮 TRM / Loader 文档确认

## 5. UART 与早期日志

当前最稳妥的早期串口策略可以先按下面收口：

- 先用 `UART0 @ 0xff0a0000`
- Linux EVB DTS 的 `bootargs` 也显式用了 `earlycon=uart8250,mmio32,0xff0a0000`
- `fiq-debugger` 配置里：
  - `rockchip,serial-id = <0>`
  - `rockchip,baudrate = <1500000>`
  - `interrupts = <GIC_SPI 115 IRQ_TYPE_LEVEL_HIGH>`

对裸机 bring-up 的含义是：
- 直接轮询 `UART0` 做最小输出是合理起点
- 当前仓库里的 `targets/rk3506` 已经开始把这条起点落成真实板级初始化：
  - `CRU_GATE_CON11` 打开 `PCLK_UART0` / `SCLK_UART0_SRC`
  - `CRU_CLKSEL_CON29` 把 `SCLK_UART0` 收口到 `XIN24M`
  - `GPIO0_IOC` 把 `GPIO0_C6/C7` 切到 `func1`
  - `UART0` 本地按 `115200 8N1` 初始化
  - 同时保留一份 `divisor + CRU/GPIO0C readback` 快照，便于把我们的默认写法和真板实际状态对照起来
- 现在还会额外做一轮只读型 `generic timer + GIC` smoke，帮助确认 `CNTFRQ/CNTPCT` 和 `GICD/GICC` 至少在当前 handoff 条件下可达
- `ttyFIQ0` 是 Rockchip/Linux 侧包装语义，不应被误当成裸机最小依赖
- 如果后续要复用 vendor 的 FIQ debugger 设计，需要额外处理 IRQ `115` 这条线

当前可见的 pinmux 线索：
- `uart0_xfer_pins`
  - RX: `GPIO0_C7`
  - TX: `GPIO0_C6`

## 6. 时钟 / 复位 / DDR 现状

### 6.1 CRU 切入点

- 主入口：`CRU @ 0xff9a0000`
- 关键资料：
  - `u-boot/include/dt-bindings/clock/rockchip,rk3506-cru.h`
  - `u-boot/arch/arm/include/asm/arch-rockchip/cru_rk3506.h`

当前已经能确认的结构信息：
- PLL：`GPLL`, `V0PLL`, `V1PLL`
- `clksel_con[62]`
- `softrst_con[23]`
- PMU 域也有独立 `pmuclksel_con[]` / `pmusoftrst_con[]`

对 bring-up 有直接价值的时钟 ID 示例：
- `PCLK_UART0..4`
- `SCLK_UART0..4`
- `PCLK_TIMER`
- `CLK_TIMER0_CH0..CH5`
- `PCLK_WDT0/1`
- `TCLK_WDT0/1`
- `PCLK_DDRC`
- `HCLK_DDRPHY`
- `CLK_DDR`, `CLK_DDRC`, `CLK_DDRPHY`

### 6.2 DDR 资料现状

当前公开 SDK 材料里，已经能确认：

- `CONFIG_SYS_SDRAM_BASE = 0x00000000`
- `SDRAM_MAX_SIZE = 0xc0000000`
- SPL / U-Boot 常用地址：
  - `CONFIG_SYS_TEXT_BASE = 0x00200000`
  - `CONFIG_SYS_INIT_SP_ADDR = 0x00600000`
  - `CONFIG_SYS_LOAD_ADDR = 0x00008000`
  - `CONFIG_SPL_TEXT_BASE = 0x03f00000`
  - `CONFIG_SPL_BSS_START_ADDR = 0x03fe0000`

但当前还不能仅凭公开 DTS 直接下结论的点也要明确写出来：

- 没有在当前公开 `rk3506.dtsi` 里直接看到一个稳定可引用的 `ddrc@...` / `ddrphy@...` MMIO 节点
- 因此“DDR 控制器 / DDR PHY 具体基址”这件事，仍需要从 TRM、vendor 初始化代码或更底层 BSP 材料中继续补

这一步不要猜。

## 7. 对 Charm 当前 ARMv7-A 路线的直接启发

结合当前仓库里的 `platform.board.armv7a_stub`，后续 RK3506 落地时建议直接按以下切面拆：

- Boot 公共层继续只关心：
  - image / slot / verify / rollback / handoff
- RK3506 平台层负责：
  - payload 实际落点解析
  - copy-to-RAM 或 XIP 映射切换
  - 关中断
  - cache / TLB 维护
  - 向量基址切换
  - 同步屏障
  - 最终 branch
- 多核相关先保持最小化：
  - 默认只保证 `cpu0` 单核 bring-up
  - `cpu1/cpu2` 的 PSCI / secondary bring-up 独立推进，不反压公共 boot 接口

这能避免我们一边前进，一边把仓库拖回“公共层懂一堆 RK3506 私货”的宏地狱。

## 8. 待补清单

- DDRC / DDRPHY 的明确 MMIO 基址与初始化时序
- BootROM 的 UART / USB 下载协议细节，以及 RockUSB/Loader 进入条件
- UART0 在目标板上的最终原理图连线、默认电平和量产下载口约定
- CRU / GRF 中与 UART0、GIC、DDR、PMIC、复位链直接相关的寄存器位说明
- 实板 SRAM / OCRAM / secure memory / reserved memory 的真实分布
- SMP / PSCI / secondary core release 的最小可控路径

## 9. HAL 目录侦察结论

### 9.1 它是什么

当前 SDK 的 `hal/` 不是零散附件，而是一套完整的 Rockchip HAL 仓，内部同时包含：

- `lib/CMSIS/Device/RK3506`：SoC 头文件、startup 模板、`system_rk3506.c`、`mmu_rk3506.c`
- `lib/bsp/RK3506`：BSP 设备描述，例如 `g_uart0Dev`、`g_uart4Dev`、`g_cruDev`
- `lib/hal/src/*`：CRU、pinctrl、UART、GIC 等驱动实现
- `project/rk3506`：Cortex-A7 AP core 工程
- `project/rk3506-mcu`：M0 侧工程

这说明它确实包含裸机代码，但它表达的是 Rockchip 自己的 HAL 参考模型，不等于我们要直接照搬的 Boot 模型。

### 9.2 对我们最有价值的已确认项

- `soc.h` 明确给出了 `GIC_DISTRIBUTOR_BASE = 0xff581000` 与 `GIC_CPU_INTERFACE_BASE = 0xff582000`
- `soc.h` 把 RK3506 AP 侧定义为 `__CORTEX_A = 7`、`HAL_GIC_V2 = 1`，并给出 `PLATFORM_CORE_COUNT = 3`
- `rk3506.h` 明确给出了 `UART0..4 @ 0xff0a0000..0xff0e0000`、`UART5 @ 0xff4e0000`
- `rk3506.h` 明确给出了 `CRU @ 0xff9a0000`、`SCRU @ 0xff9a8000`、`GRF @ 0xff288000`、`GRF_PMU @ 0xff910000`
- `rk3506.h` 明确给出了 `TIMER5 @ 0xff255000`，而 `project/rk3506/src/hal_conf.h` 把 `SYS_TIMER` 指到 `TIMER5`
- `hal_bsp.c` 给出了 `g_uart0Dev` / `g_uart4Dev` 的 clock gate、pclk gate 和 IRQ 绑定，可作为后续真实 UART bring-up 的参考入口
- `hal_bsp.c` 里的 `g_cruDev` 也确认了 CRU/SCRU/CRU_PMU/SCRU_PMU 这四组 bank 的组织方式

这些内容适合吸收到我们的 `targets/rk3506` 平台常量、早期初始化顺序和文档里。

### 9.3 不能直接照搬的部分

`project/rk3506` 虽然是 AP 侧裸机工程，但它不是“从冷启动开始的首级程序”，而更像一个 post-DDR 的 AMP 固件：

- `GCC/Makefile` 默认把固件放到 `FIRMWARE_CPU_BASE = 0x03e00000`
- 同一个工程还定义了 `SHMEM_BASE = 0x03b00000` 与 `LINUX_RPMSG_BASE = 0x03c00000`
- `Image/amp_linux.its` 把镜像打成 FIT，并写明了 “linux on cpu0”
- `main.c` 里的 GIC 示例把 IRQ 路由到 `CPU_GET_AFFINITY(1, 0)`
- `main.c` 默认配置的是 `UART4`，而不是公开启动链更稳定的 `UART0`

这意味着：

- 它更适合作为“DDR 已可用之后，AP 固件如何接管”的参考
- 不适合直接替代我们的首级 bring-up 心智模型
- 更不应该把 Linux AMP / RPMsg 的共享内存布局抬成 Charm 的公共平台契约

### 9.4 startup / MMU 模板给我们的启发

`startup_rk3506.c` 和 `system_rk3506.c` 仍然很有参考价值，因为它们收敛了 Cortex-A7 最小启动序列：

- reset 入口先关中断、关 MMU、关 cache、清 high vectors，并把 `VBAR` 指到自己的向量表
- 为 `FIQ/IRQ/SVC/ABT/UND/SYS` 分别布栈
- 在 `SystemInit()` 中依次做 TLB / branch predictor / I-cache 失效、D-cache clean+invalidate、建页表、开 MMU、开 cache
- `mmu_rk3506.c` 明确把 `0xFF000000` 高地址外设窗映射成 device memory

所以我们后续应当：

- 继续保留“单镜像、post-DDR、板级叶子 target”的公开模型
- 借用 vendor startup/MMU 模板里的顺序意识，而不是照搬它的 AMP 内存布局
- 把早期 UART、CRU、GRF、GIC、timer 初始化逐步下沉到 `targets/rk3506`，而不是塞回公共 Boot 层

### 9.5 当前对仓库的直接落点

基于这轮 HAL 侦察，仓库里当前最稳妥的落点是：

- `targets/rk3506` 继续默认 `UART0` 作为最小早期 console
- 同时允许板级把 early UART 覆盖成 `UART4` 这类 demo/载板选择
- 但当前只有 `UART0` 默认路径已经具备本地 early init；其他 early UART 仍应视为前级负责
- `system SRAM @ 0xfff80000` 与 `size = 0x0000c000` 可以先作为 post-DDR handoff 线索保留下来
- 真正的 `CRU/GRF/UART/GIC` 早期初始化可以开始以 HAL 已确认的寄存器入口为锚点推进
