# 板级资料入口

本目录用于收纳“准备上某块板之前，必须先稳定下来的那批事实”。

目标很明确：
- 把 SoC / 板级寄存器、启动介质、时钟/复位、中断号这类高频查询项整理成项目内材料
- 区分 SoC 级事实、板级变体、以及当前 U-Boot/Linux 配置，不把它们混成一层
- 给 Bootloader bring-up、平台叶子 target、以及后续上板验证提供共同锚点

## 当前文档

- `docs/board/rk3506/README.md`：RK3506 上板前期资料收口
- `docs/board/rk3506/post_ddr_handoff_contract.md`：RK3506 前级到裸机镜像的 post-DDR handoff 契约
- `docs/system/armv7a_platform_contract.md`：ARMv7-A 平台层与 Boot handoff 的边界契约

## 维护约定

- 优先记录能从 SDK、DTS、头文件、板卡原理图稳定提取的事实
- 对“只是当前 U-Boot / Linux 选择”的值显式标注，不冒充 SoC 唯一真相
- 对缺失 TRM 或实板验证的部分保留待补清单，不用猜测补齐
- 板级材料优先为 bring-up 服务，不在这里展开通用 Boot 策略或协议设计
