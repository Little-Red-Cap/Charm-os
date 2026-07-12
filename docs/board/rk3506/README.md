# RK3506 板级资料

> status: supporting
>
> 本文整理用户提供的 RK3506 SDK、DTS、U-Boot 配置和 vendor HAL 观察值。
> 当前仓库不能独立复验该 SDK 快照；数值用于 bring-up 起点，不替代 TRM、
> 原理图或实板证据。

前级交接条件见 [`post_ddr_handoff_contract.md`](post_ddr_handoff_contract.md)，当前
target 实现见 [`../../../targets/rk3506/README.md`](../../../targets/rk3506/README.md)。

## 资料来源

以下路径相对于该 SDK 根目录：

- `u-boot/arch/arm/dts/rk3506*.dts*`
- `u-boot/include/configs/rk3506_common.h`
- `u-boot/include/configs/evb_rk3506.h`
- `u-boot/include/dt-bindings/{soc,clock}/*rk3506*`
- `u-boot/arch/arm/include/asm/arch-rockchip/{cru,grf,ioc}_rk3506.h`
- `kernel-6.1/arch/arm/boot/dts/rk3506*.dts*`
- `hal/lib/CMSIS/Device/RK3506`
- `hal/lib/bsp/RK3506`
- `hal/project/rk3506*`

## SoC 快照

- CPU：`3 x arm,cortex-a7`
- 中断控制器：`arm,gic-400`
- 核心定时器：`arm,armv7-timer`，计数频率 `24 MHz`
- DTS 次级核启用方式：`psci`
- SDK 默认 early console：UART0

## 地址速查

| 模块 | 基地址 / IRQ | 来源备注 |
|---|---|---|
| GIC Distributor | `0xff581000` | DTS、U-Boot、HAL `soc.h` |
| GIC CPU Interface | `0xff582000` | DTS、U-Boot、HAL `soc.h` |
| GIC 额外寄存器块 | `0xff584000`, `0xff586000` | DTS `gic` reg 元组，具体用途待 TRM 确认 |
| UART0 | `0xff0a0000`, IRQ `34` | 默认 early console |
| UART1 | `0xff0b0000`, IRQ `35` | DTS/HAL |
| UART2 | `0xff0c0000`, IRQ `36` | DTS/HAL |
| UART3 | `0xff0d0000`, IRQ `37` | DTS/HAL |
| UART4 | `0xff0e0000`, IRQ `38` | vendor AP demo 默认串口 |
| UART5 | `0xff4e0000`, IRQ `39` | DTS/HAL |
| GRF | `0xff288000` | DTS/HAL |
| IOC_GRF | `0xff4d8000` | IO/pinmux syscon |
| OTP | `0xff4f0000` | DTS |
| Secure OTP | `0xff520000` | SPL DTS |
| IOC1 | `0xff660000` | IOC syscon |
| GRF_PMU | `0xff910000` | reboot mode 所在 PMU syscon |
| IOC_PMU | `0xff950000` | PMU IO/pinmux syscon |
| CRU | `0xff9a0000` | DTS/HAL |
| SCRU | `0xff9a8000` | HAL |
| DMAC0 | `0xff000000`, IRQ `116/117` | DTS |
| DMAC1 | `0xff008000`, IRQ `118/119` | DTS |
| WDT0 | `0xff260000`, IRQ `107` | DTS |
| WDT1 | `0xff268000`, IRQ `108` | DTS |
| Mailbox0..3 | `0xff290000..0xff293000`, IRQ `138..141` | DTS |
| USB2 OTG0 | `0xff740000`, IRQ `74` | DTS |
| USB2 OTG1 | `0xff780000`, IRQ `79` | DTS |
| DSMC | `0xff8b0000`, IRQ `126` | DTS |
| TIMER5 | `0xff255000` | HAL `rk3506.h`; AP demo `SYS_TIMER` |

### GPIO

| Bank | 基地址 | GIC SPI |
|---|---|---|
| GPIO0 | `0xff940000` | `0` |
| GPIO1 | `0xff870000` | `4` |
| GPIO2 | `0xff1c0000` | `8` |
| GPIO3 | `0xff1d0000` | `12` |
| GPIO4 | `0xff1e0000` | `16` |

ARM generic timer 的 DTS PPI 为 `13/14/11/10`，频率为 `24 MHz`。这些是
device-tree interrupt specifier，不应直接当作 GIC INTID；target 当前使用的 INTID
约定见 handoff 契约。

## Boot 与下载线索

SDK 配置体现的启动顺序是板级选择：

- `rk3506-u-boot.dtsi`：`mmc -> spi_nand -> spi_nor`
- `rk3506-evb-tb.dts`：`spi_nand -> spi_nor -> mmc`

`grf_pmu.reboot_mode` 提供 normal、recovery、fastboot、UMS、panic、watchdog、
charge 和 bootloader/loader mode；后两者映射 `BOOT_BL_DOWNLOAD`，其头文件注释为
进入 RockUSB bootloader mode。该观察不能替代 BootROM 下载协议说明。

SPL DTS 还包含：

- `secure-otp@ff520000`
- `secure_conf = <0xff210100>`
- `cru_rst_addr = <0xff9a8080>`
- `mask_addr = <0xff528000>`

这些值仅证明 vendor SPL 涉及 secure OTP/reset gating，不证明当前仓库已支持
secure boot、熔丝或量产下载。

## UART0 线索

- Linux EVB bootargs 使用 `earlycon=uart8250,mmio32,0xff0a0000`。
- pinmux `uart0_xfer_pins`：TX=`GPIO0_C6`，RX=`GPIO0_C7`，`func1`。
- Rockchip `fiq-debugger` 使用 serial-id `0`、`1500000` baud 和 IRQ `115`；
  `ttyFIQ0` 是 Linux/vendor 包装，不是 bare-metal UART0 的必要条件。

当前 target 的 UART0 初始化、readback 字段和其它 UART 的前级责任由
[`../../../targets/rk3506/README.md`](../../../targets/rk3506/README.md) 定义。

## 时钟与 DDR

CRU 资料显示 `GPLL/V0PLL/V1PLL`、`clksel_con[62]`、`softrst_con[23]`，PMU
域另有 clksel/softrst bank。与早期 bring-up 直接相关的 clock ID 包括 UART、
timer、watchdog、DDRC 和 DDRPHY 各组 PCLK/HCLK/SCLK。

SDK 内存配置观察值：

| 项 | 值 |
|---|---|
| `CONFIG_SYS_SDRAM_BASE` | `0x00000000` |
| `SDRAM_MAX_SIZE` | `0xc0000000` |
| `CONFIG_SYS_TEXT_BASE` | `0x00200000` |
| `CONFIG_SYS_INIT_SP_ADDR` | `0x00600000` |
| `CONFIG_SYS_LOAD_ADDR` | `0x00008000` |
| `CONFIG_SPL_TEXT_BASE` | `0x03f00000` |
| `CONFIG_SPL_BSS_START_ADDR` | `0x03fe0000` |

公开 DTS 未给出可稳定引用的 DDRC/DDRPHY MMIO 节点。控制器地址、PHY 地址、
训练流程和容量不能从上述 U-Boot layout 反推。

## Vendor HAL 适用范围

HAL 交叉确认了 GIC、UART、CRU/SCRU、GRF/GRF_PMU 地址以及三核 Cortex-A7
配置。其 AP 工程不是冷启动首级程序：

- firmware base 为 `0x03e00000`；
- shared memory 为 `0x03b00000`，Linux RPMsg 为 `0x03c00000`；
- FIT 描述包含 `linux on cpu0`；
- 示例 GIC 路由和 UART4 配置服务于 AMP demo。

因此 HAL 可用于核对寄存器和启动操作顺序，不能把其 AMP 内存布局、CPU 分工或
UART4 选择提升为 Charm 平台契约。vendor startup/MMU 模板提供的 cache/TLB、
页表和 device-memory 顺序也只作为实现参考。

## 未确认事实

- DDRC/DDRPHY 基址、训练流程和初始化时序；
- BootROM UART/USB 下载协议与 RockUSB 进入条件；
- 目标板 UART0 连线、电平和下载口约定；
- UART、GIC、DDR、PMIC 与复位链的完整 CRU/GRF 位定义；
- SRAM/OCRAM、secure memory 和 reserved memory 的实板布局；
- PSCI、secondary core release 与多核所有权。
