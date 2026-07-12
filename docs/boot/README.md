# Boot

本目录说明 Charm 当前 boot 原型的边界。实现事实以
[`../../Modules/system/boot`](../../Modules/system/boot) 和板级 capability 为准。

当前代码是可在 host 验证的镜像选择、加载与 handoff 组件，不是产品 bootloader，
也不包含任何 SoC 的 BootROM、DDR 初始化或 Stage1。

## 当前主链

```text
Storage + BootInfo + Policy
-> BootPlan
-> BootTarget
-> BootLoadPlan
-> BootLoadedImage
-> BootExecution
-> BootHandoff
-> board prepare/jump
-> confirm
```

- `BootPlan` 选择 `pending_trial / active / pending / fallback` 槽位并记录是否需要
  rollback prepare 与成功确认。
- `BootPlan` 的 policy 校验覆盖候选镜像；`BootTarget` 再解析所选分区的镜像头、
  布局、payload offset 和 entry offset。
- `BootLoadPlan` 区分 `copy_to_ram` 与 `xip`。
- `BootLoadedImage` 由板级 load capability 解析地址并准备 payload。
- `BootExecution` 只处理 pre-jump 与 jump。
- `BootHandoff` 按顺序完成 target、load、execution 和 rollback prepare；任何步骤
  失败都保持 `ready_to_jump=false`。
- App 确认成功后，`confirm_boot_plan()` 才更新 active/pending 状态。

## 所有权

Boot 模块拥有：

- `ImageHeader`、`BootInfo`、A/B partition 与 policy；
- 镜像校验、槽位选择、pending trial 与 fallback；
- XIP/copy-to-RAM 加载语义；
- 与 `platform::board::BootBoardCaps` 的 load/exec 桥接。

板级实现拥有：

- 存储介质读写与真实 payload 地址；
- copy、XIP 映射、cache/TLB、向量和中断状态准备；
- 最终 jump。

传输协议只负责把 bytes 写入目标 partition，不参与槽位 policy 或机器状态切换。
当前 X/YModem 子集见 [`bootloader_xymodem.md`](bootloader_xymodem.md)。

## 已知限制

- `ImageHeader` 与 `BootInfo` 仍使用本机 trivially-copyable 布局；持久化 endian、
  packing 和兼容规则尚未冻结。
- 签名字段与 policy 骨架不构成 secure boot。
- Host mock 能证明状态机和调用顺序，不能证明 Flash 断电一致性或真实板 jump。
- Stage0/Stage1/Stage2 只可作为具体平台的物理分层，不是 Charm 公共类型体系。

## 验证入口

- [`../../Examples/boot/bootloader_demo`](../../Examples/boot/bootloader_demo)：A/B、
  download、verify、load、handoff、rollback prepare、jump mock 与 confirm。
- [`../../Examples/io/xymodem_demo`](../../Examples/io/xymodem_demo)：X/YModem 帧解析。
- [`../system/armv7a_platform_contract.md`](../system/armv7a_platform_contract.md)：
  ARMv7-A pre-jump 机器状态边界。
- [`../board/rk3506/post_ddr_handoff_contract.md`](../board/rk3506/post_ddr_handoff_contract.md)：
  RK3506 post-DDR 入口条件；它不等于 Boot 模块已在 RK3506 上闭环。
