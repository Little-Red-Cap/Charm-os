# Boot 原型

## 文档状态

- `status`: `supporting`
- `scope`: image selection、load、handoff 与 confirm 边界
- `authority`: [`Modules/system/boot`](../../Modules/system/boot) 与板级 `BootBoardCaps`

当前实现是可由 Host fixture 验证的 boot 组件，不是产品 bootloader，也不包含 SoC BootROM、DDR
初始化或 Stage1。

## 当前主链

```text
Storage + BootInfo + Policy
-> BootPlan
-> BootTarget
-> BootLoadPlan
-> BootLoadedImage
-> BootExecution
-> BootHandoff
-> board prepare / jump
-> confirm
```

- `BootPlan` 按 policy 选择有效候选，并记录 rollback prepare/confirm 要求；
- `BootTarget` 校验所选分区的 header/layout，解析 payload 与 entry offset；
- `BootLoadPlan/BootLoadedImage` 区分 `copy_to_ram` 与 `xip`，地址和 payload preparation 由板级
  load capability 完成；
- `BootHandoff` 依次完成 target、load、execution 与 rollback prepare，任一步失败都保持
  `ready_to_jump=false`；
- App 成功后，`confirm_boot_plan()` 才提交 active/pending 状态。

## 所有权

| Owner | 责任 |
|---|---|
| Boot module | `ImageHeader/BootInfo`、A/B policy、校验、槽位选择、pending trial/fallback、copy/XIP 语义 |
| Board | media 访问、真实 payload 地址、copy/XIP mapping、cache/TLB、vector/interrupt preparation 与 jump |
| Transport | 将 bytes 写入目标 partition，不参与 slot policy 或机器状态切换 |

Boot module 通过 `platform::board::BootBoardCaps` 调用 board load/exec。当前 X/YModem 子集见
[`bootloader_xymodem.md`](bootloader_xymodem.md)。

## 已知限制

- `ImageHeader` 与 `BootInfo` 仍使用本机 trivially-copyable 布局；持久化 endian、
  packing 和兼容规则尚未冻结。
- 签名字段与 policy 骨架不构成 secure boot。
- Host fixture 只能证明状态机和调用顺序，不能证明 Flash 断电一致性或真实板 jump。
- Stage0/Stage1/Stage2 只可作为具体平台的物理分层，不是 Charm 公共类型体系。

## 验证入口

- [`../../Examples/boot/bootloader_demo`](../../Examples/boot/bootloader_demo)：A/B、
  download、verify、load、handoff、rollback prepare、jump mock 与 confirm。
- [`../../Examples/io/xymodem_demo`](../../Examples/io/xymodem_demo)：X/YModem 帧解析。
- [`../system/armv7a_platform_contract.md`](../system/armv7a_platform_contract.md)：
  ARMv7-A pre-jump 机器状态边界。
- [`../board/rk3506/post_ddr_handoff_contract.md`](../board/rk3506/post_ddr_handoff_contract.md)：
  RK3506 post-DDR 入口条件；它不等于 Boot 模块已在 RK3506 上闭环。
