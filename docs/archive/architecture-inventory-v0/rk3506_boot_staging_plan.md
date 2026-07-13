# RK3506 Boot Staging Plan

> status: archived
>
> 这是 RK3506 启动阶段的早期完整分析，保留 SDK 线索、阶段职责和当时的推进建议。
> 它不是当前实现状态或现行契约；当前入口见
> [`../../board/rk3506/boot_staging_plan.md`](../../board/rk3506/boot_staging_plan.md)。

这份文档用于把 RK3506 的真实物理引导阶段和 Charm 仓库内部的逻辑阶段拆开描述。

结论先写在前面：

- RK3506 物理上很可能必须多阶段启动
- 但仓库架构不应该因此变成“到处都是 stage1/stage2 宏和心智”
- 当前仓库里的 `rk3506-baremetal-skeleton` 应被视为一个 `post-DDR normal image` 占位，而不是 SRAM-only 早期阶段

## 1. 来自 SDK 的关键信号

下面这些事实直接来自当前 SDK，可作为分层依据：

- `rkbin/RKBOOT/RK3506MINIALL.ini`
  - `FlashData=bin/rk35/rk3506_ddr_750MHz_v1.06.bin`
  - `FlashBoot=bin/rk35/rk3506_spl_v1.11.bin`
  - `LOAD_ADDR=0x3f00000`
  - `CREATE_IDB=true`
- `device/rockchip/.chips/rk3506/package-file`
  - `bootloader` 对应 `MiniLoaderAll.bin`
- `device/rockchip/common/configs/Config.in.loader`
  - 明确存在“force using U-Boot SPL instead of Rockchip MiniLoader binary”开关
- `u-boot/make.sh`
  - `idblock.bin` 打包形式是 `TPL_BIN:SPL_BIN`
- `u-boot/include/configs/rk3506_common.h`
  - `CONFIG_SYS_TEXT_BASE = 0x00200000`
  - `CONFIG_SYS_INIT_SP_ADDR = 0x00600000`
  - `CONFIG_SPL_TEXT_BASE = 0x03f00000`
  - `CONFIG_SPL_MAX_SIZE = 0x40000`
  - `CONFIG_SPL_BSS_START_ADDR = 0x03fe0000`
  - `CONFIG_SPL_STACK = 0x03f00000`
- `u-boot/drivers/ram/rockchip/sdram_rk3506.c`
  - 在 `CONFIG_TPL_BUILD` 下没有给出可用的开源 DDR 初始化实现，`sdram_init()` 直接返回失败

这些信息组合起来的含义很明确：

- RK3506 当前 SDK 主路径本身就是分阶段的
- DDR bring-up 不是一个可以轻描淡写忽略掉的小步骤
- 现成 SDK 明显更依赖 Rockchip 自己的 loader / DDR binary 体系，而不是一个完全开源、完全由 U-Boot TPL 承担的 DDR 早期路径

## 2. 推荐的三阶段模型

建议把 RK3506 的 bring-up 和后续真实上板工作，按下面三段理解：

```text
BootROM / boot media ingress
    -> Stage A: SRAM early stage
    -> Stage B: DDR init / relocation stage
    -> Stage C: post-DDR normal image
```

这三段是物理阶段，不等于都必须变成仓库里的公开 target。

## 3. Stage A: SRAM Early Stage

这是最早、最脏、最受限的一段。

### 3.1 目标

- 在片内 SRAM / IRAM 的严格限制下接住 CPU
- 完成最小的时钟、复位、介质入口、调试入口准备
- 为 DDR bring-up 或下一阶段 loader 创造前提

### 3.2 典型限制

- 可用内存极小
- 地址通常固定，重定位空间极差
- 大 BSS、大表、复杂日志、动态分配都不合适
- 很多“正常 C/C++ 代码习惯”在这里都不安全
- 外设也未必完全 ready，甚至串口都可能需要极小初始化

### 3.3 适合做的事

- 最小入口汇编和栈建立
- Boot source 判定
- 最小复位原因采样
- 极小串口输出
- 进入 vendor DDR binary 或下一阶段 loader
- 必要时把下一阶段搬到早期执行地址

### 3.4 不适合做的事

- 大量 C++ 运行时假设
- 复杂异常框架
- GIC/generic timer 烟测
- 完整 MMU / cache / TLB 实验
- 高层 Boot 策略、slot、verify、rollback

### 3.5 对仓库的含义

这部分即使后续自行实现，也应隐藏在 `targets/rk3506/` 私有层里。

它不应该反向塑形：

- 顶层 CMake
- 公共 `boot_*` 模块
- QEMU 上的 ARMv7-A 公共 bring-up 契约

## 4. Stage B: DDR Init / Relocation Stage

这一层负责从片上 SRAM 过渡到 DDR。

### 4.1 目标

- 初始化并训练 DDR
- 验证基本内存可用
- 把执行体搬到更合理的地址
- 切换到更大的栈、更正常的 BSS 和镜像布局

### 4.2 从 SDK 看到的现实

当前 SDK 明显存在这类过渡层：

- `RK3506MINIALL.ini` 里 `FlashData` 和 `FlashBoot` 是两段
- `FlashBoot` 的 `LOAD_ADDR=0x03f00000`
- `CONFIG_SPL_TEXT_BASE`、`CONFIG_SPL_STACK`、`CONFIG_SPL_MAX_SIZE` 也都对这段给出非常紧的早期约束

在 Rockchip 当时的主路径中，这不是可选优化，而是实际存在的物理分层。

### 4.3 适合做的事

- DDR init / training / size 探测
- 迁移到 post-DDR 运行地址
- 清理更大的 `.bss`
- 建立更稳定的栈和镜像布局
- 重新确认 UART、向量基地址和最小异常入口

### 4.4 仍然不建议过早塞进来的东西

- 高层 boot policy
- 文件系统、镜像管理和下载协议状态机
- 大量平台无关模块
- 把这一层抽象成全仓公开的通用 staged boot 模型

## 5. Stage C: Post-DDR Normal Image

当前仓库应优先稳定这一层。

### 5.1 它是什么

DDR 可用且镜像完成迁移后，这一层承载 Cortex-A bring-up 逻辑。

### 5.2 适合放进来的内容

- 向量表和异常框架
- GIC + generic timer
- IRQ/FIQ 烟测
- MMU 属性切换
- cache / TLB / barrier 维护
- 后续更完整的裸机内核或 Bootloader 主逻辑

### 5.3 当前仓库映射

当前的：

- `targets/rk3506/rk3506-baremetal-skeleton`
- `targets/rk3506/startup.S`
- `targets/rk3506/rk3506_platform.cpp`

更适合被理解成 Stage C 的最小占位，不是 Stage A 的成品。

这也是为什么它现在使用了这些地址和假设：

- `image text base = 0x00200000`
- `stack top = 0x00600000`
- UART0 / GIC / timer 基地址公开暴露

这套假设天然更接近“DDR 已经起来后的正常镜像”，而不是“片内 SRAM 里的极小早期引导”。

## 6. QEMU 公共路径应该落在哪一层

`Examples/kernel/armv7a/qemu` 现在承担的角色，最接近 Stage C 的公共验证底座。

它优先验证的是：

- reset 早期钩子
- 向量表安装
- 异常入口
- GIC 和 timer 语义
- MMU / cache / TLB 操作顺序

因此更健康的关系应该是：

- QEMU ARMv7-A 公共路径负责 Stage C 的语义验证
- RK3506 板级叶子负责把这些语义映射到真实地址和真实外设
- Stage A / Stage B 的芯片特定约束，尽量只停留在 RK3506 私有层

## 7. 如果以后重新引入多阶段，建议怎么落仓库

如果后续 DDR bring-up 真要进入 Charm 主线，建议组织方式如下：

- 公开目标仍然优先保持 `rk3506-baremetal-skeleton` 这类 Stage C 叶子
- Stage A / Stage B 作为 RK3506 私有的隐藏 leaf image
- 顶层 CMake 不默认把 staged boot chain 暴露成全仓主模型

可以接受的方向是：

- `targets/rk3506/sram/`
- `targets/rk3506/ddr/`
- `targets/rk3506/normal/`

不建议的方向是：

- 把整个工程都抽象成 stage1/stage2/stage3 通用框架
- 让公共 `boot_*` 直接知道 RK3506 的 FlashData / FlashBoot / DDR binary 细节
- 为某一颗芯片的早期约束，把公共层重新拖回宏地狱

## 8. 对当前工作的直接指导

基于上面的分层，当前最合理的推进顺序是：

1. 继续把 Stage C 打稳
2. 让 RK3506 的 Stage C 先接住真实的 `CRU/GRF/UART0`
3. 接 `GIC + generic timer`
4. 对齐你现在 QEMU ARMv7-A 主线里的异常/向量/平台契约
5. 等到这些边界都稳定后，再回头决定是否要显式实现 Stage A / Stage B

## 9. 当前仍不确定的部分

下面这些点仍然需要 TRM、板卡原理图、实板验证或更深的 vendor 资料：

- 片内 SRAM / OCRAM 的准确容量与可用布局
- DDRC / DDRPHY 的明确 MMIO 与完整初始化路径
- BootROM 对各启动介质和下载模式的精确行为
- 是否存在必须保留的 vendor secure / OTP / reset gating 前置条件
- 如果未来替换 vendor loader，自研 Stage A / Stage B 的最小可行边界到底在哪里

这些地方先保留为待确认事实，不猜。
