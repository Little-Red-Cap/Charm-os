# RK3506 Boot Staging SDK 证据

> `status`: `archived`

本文保留早期 RK3506 SDK 中支持多阶段启动判断的具体线索，不定义当前实现或推进顺序。现行物理阶段
边界见 [`boot_staging_plan.md`](../../board/rk3506/boot_staging_plan.md)，post-DDR 入口条件见
[`post_ddr_handoff_contract.md`](../../board/rk3506/post_ddr_handoff_contract.md)。

## SDK 线索

- `rkbin/RKBOOT/RK3506MINIALL.ini`
  - `FlashData=bin/rk35/rk3506_ddr_750MHz_v1.06.bin`
  - `FlashBoot=bin/rk35/rk3506_spl_v1.11.bin`
  - `LOAD_ADDR=0x3f00000`
  - `CREATE_IDB=true`
- `device/rockchip/.chips/rk3506/package-file` 将 bootloader 映射到 `MiniLoaderAll.bin`。
- `device/rockchip/common/configs/Config.in.loader` 提供“强制使用 U-Boot SPL 替代 Rockchip
  MiniLoader binary”的开关。
- `u-boot/make.sh` 将 `idblock.bin` 组织为 `TPL_BIN:SPL_BIN`。
- `u-boot/include/configs/rk3506_common.h`
  - `CONFIG_SYS_TEXT_BASE=0x00200000`
  - `CONFIG_SYS_INIT_SP_ADDR=0x00600000`
  - `CONFIG_SPL_TEXT_BASE=0x03f00000`
  - `CONFIG_SPL_MAX_SIZE=0x40000`
  - `CONFIG_SPL_BSS_START_ADDR=0x03fe0000`
  - `CONFIG_SPL_STACK=0x03f00000`
- `u-boot/drivers/ram/rockchip/sdram_rk3506.c` 在 `CONFIG_TPL_BUILD` 路径没有可用的开源 DDR
  初始化，`sdram_init()` 直接失败。

这些快照说明当时 SDK 主路径区分 DDR data 与 SPL/loader，DDR bring-up 是独立物理阶段，并明显依赖
Rockchip loader/DDR binary。它们不证明 vendor 版本永久固定，也不证明 Charm 已实现早期阶段。

## 历史阶段映射

```text
BootROM / boot media
  -> SRAM early stage
  -> DDR init / relocation
  -> post-DDR normal image
```

- SRAM early stage 只建立最小 stack、boot ingress、diagnostics 和下一阶段入口。
- DDR stage 负责 training、memory probe、image relocation、较大 stack/BSS 与 handoff state。
- Post-DDR image 承载 vector、exception、GIC/timer、MMU/cache/TLB 和 normal runtime。

这是芯片物理阶段，不要求形成公共 stage1/stage2/stage3 框架。Vendor DDR binary、BootROM、SRAM address
和 early memory limit 应留在 RK3506 leaf；image policy、slot、verify、rollback 和 filesystem 不进入 DDR
bring-up。

QEMU ARMv7-A leaf 最接近 post-DDR 语义验证，只证明可仿真的 exception/interrupt/MMU/cache 行为；
RK3506 leaf 仍需映射真实 address、clock、reset、memory 与 peripheral。

## 未确认事实

- SRAM/OCRAM 的准确容量与可用布局；
- DDRC/DDRPHY MMIO、training 流程与最小初始化；
- BootROM 对 boot media、download mode 和 image format 的精确行为；
- secure/OTP/reset gating 前置状态；
- PSCI/secondary-core ownership；
- 替换 vendor loader 时自研 early/DDR stage 的最小范围。

这些事实需要 TRM、原理图、vendor 资料或实板证据，不能从 SDK layout 反推。
