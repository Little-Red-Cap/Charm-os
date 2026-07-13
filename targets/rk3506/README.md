# RK3506 Target

## 文档状态

- `status`: `supporting`
- `scope`: RK3506 bare-metal target 边界与 bring-up 路由
- `authority`: 本目录 source、[`CharmTargetConfig.cmake`](CharmTargetConfig.cmake) 与
  [`sources.cmake`](sources.cmake)

板级寄存器观察与前级交接条件分别见：

- [RK3506 板级资料](../../docs/board/rk3506/README.md)
- [post-DDR handoff contract](../../docs/board/rk3506/post_ddr_handoff_contract.md)

## 模型边界

本 target 是单核、ARM state、little-endian、normal-world PL1 的 post-DDR 单镜像 payload。它负责异常模式
栈、BSS、低向量、UART0 early console、只读状态观测和一次性 generic timer IRQ smoke。

BootROM/RockUSB、DDR 训练、SPL/U-Boot、PSCI、次级核拉起、最终 MMU/cache policy、周期 tick 和完整中断
框架不属于该 target。

## Bring-up stage

`CHARM_RK3506_BRINGUP_STAGE` 当前接受：

- `minimal`：只建立 startup、向量、breadcrumb 与 UART，适合首次串口存活验证；
- `observe`：增加 GIC/generic timer 只读观测，不触发 IRQ smoke；
- `irq-smoke`：执行一次 timer IRQ、EOIR 并返回。

默认值、preset 与 target 名称以根 [`CMakePresets.json`](../../CMakePresets.json) 和
`CharmTargetConfig.cmake` 为准，不在 README 复制。

## 诊断

startup 与 vector breadcrumb 的数值/名称由 [`startup.S`](startup.S)、
[`rk3506_platform.cpp`](rk3506_platform.cpp) 和
[`rk3506_exceptions.cpp`](rk3506_exceptions.cpp) 定义。fatal exception 会输出 breadcrumb、处理器状态、
return PC 与关键寄存器；IRQ 只在 timer smoke 窗口内允许返回，其余 IRQ 进入 fatal 路径。

默认 handoff 期望 non-secure physical timer INTID `30`。bring-up smoke 同时识别 `29` 与 `30`：
识别 `29` 只证明 physical timer IRQ 链路可达，不证明前级安全态符合默认 handoff contract。

## 配置事实

地址、时钟、镜像跨度、栈大小和 provisional 标记属于 `CharmTargetConfig.cmake`；编译映射与 linker
script 生成属于 `sources.cmake`。README 不把这些实现值复制为永久 SoC 事实。
