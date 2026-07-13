# 板级资料入口

> status: `supporting`
>
> 本目录记录 SoC/板级事实与 handoff 前置条件，不定义通用 Boot 策略。

| 主题 | 入口 |
|---|---|
| RK3506 SoC、板型与 bring-up 事实 | [`rk3506/README.md`](rk3506/README.md) |
| RK3506 post-DDR handoff | [`rk3506/post_ddr_handoff_contract.md`](rk3506/post_ddr_handoff_contract.md) |
| ARMv7-A 平台与 Boot handoff 边界 | [`armv7a_platform_contract.md`](../system/armv7a_platform_contract.md) |

- 区分 SoC 事实、板级变体和当前 U-Boot/Linux 配置。
- 缺少 TRM、原理图或实板证据时保留 unknown，不用推测补齐。
- 通用镜像选择、加载和 Boot policy 从 [`boot/README.md`](../boot/README.md) 进入。
