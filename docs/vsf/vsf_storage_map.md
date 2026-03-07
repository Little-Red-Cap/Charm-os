# VSF 存储体系映射（MAL / FS / SCSI）

目标：抽取 VSF 存储组件的结构与接口模式，为 Charm 的 FS/VFS/Block 层提供可迁移骨架。

## 1) VSF MAL（块设备抽象层）

核心文件：
- `Draft/vsf/source/component/mal/vsf_mal.h`
- `Draft/vsf/document/component/mal/README_zh.md`

关键特征：
- `vk_mal_t` 作为通用块设备对象
- `vk_mal_drv_t` 作为驱动接口（blksz/buffer/init/fini/erase/read/write）
- 统一的特性标志（读/写/擦/非统一块大小）
- 支持“可重入包装” `vk_reentrant_mal_t`（通过 mutex 包装共享设备）

驱动类型（driver/*）：
- mem_mal：内存块设备（buffer + size）
- file_mal：文件映射块设备（镜像文件）
- flash_mal：内部 flash
- sdmmc_mal：SDIO/SDMMC
- scsi_mal：SCSI 设备封装为块设备
- cached_mal：缓存层封装
- fakefat32_mal：模拟 FAT32

对 Charm 的启示：
- 我们的 `fs_block` 可以加入“特性标志 + 块大小查询”抽象
- 可重入包装思路可以映射为 `BlockDeviceGuard` / `BlockDeviceProxy`
- `cached_mal` 对应于 VFS 层的 block cache 策略

## 2) VSF FS（VFS + 文件系统驱动）

核心文件：
- `Draft/vsf/source/component/fs/vsf_fs.h`

关键特征：
- `vk_fs_op_t` = mount/unmount/rename + fop/dop
- 文件操作拆分为：
  - fop: close/read/write/setsize/setpos
  - dop: lookup/create/unlink/chmod
- 文件对象 `vk_file_t` 自带 attr/size/pos/parent
- VFS 以 `vk_vfs_file_t` 扩展节点（挂载、child list、子 FS）
- 强依赖 EDA 子调用（async 模型）

驱动类型（driver/*）：
- fatfs / littlefs / linfs / memfs / romfs / winfs / malfs

对 Charm 的启示：
- VSF 的 “fop/dop 分离” 与我们 `NodeOps` 的职责接近
- `vk_vfs_file_t` 的子 FS 绑定思路可参考我们 mount 节点结构
- 依赖 EDA 子调用模型说明：FS 操作天然异步

## 3) VSF SCSI

核心文件：
- `Draft/vsf/source/component/scsi/vsf_scsi.h`

驱动类型：
- mal_scsi：基于 MAL 的 SCSI 设备
- virtual_scsi：虚拟 SCSI 设备

对 Charm 的启示：
- SCSI 可以作为 USB MSC / 网络存储的中间层
- `scsi_mal` 是“协议设备 -> 块设备”的典型桥梁

## 4) Charm 映射建议（骨架层次）

建议结构：

- `io/fs/block`：基础块设备接口（read/write/erase/blksz/feature）
- `io/fs/block_cache`：缓存封装（对应 cached_mal）
- `io/fs/vfs`：VFS 节点与 mount
- `io/fs/driver/*`：fatfs/lfs/memfs/romfs
- `io/fs/bridge/*`：scsi -> block、file -> block

## 5) 迁移优先级建议

1) `MAL -> BlockDevice` 抽象对齐（特性标志 + blksz + reentrant 包装）
2) `cached_mal` 对应的 block cache（落在 VFS 层或 block 层）
3) `scsi_mal` 对应的“协议设备 -> block 适配器”

---

参考来源：
- `Draft/vsf/source/component/mal/vsf_mal.h`
- `Draft/vsf/source/component/fs/vsf_fs.h`
- `Draft/vsf/source/component/scsi/vsf_scsi.h`
- `Draft/vsf/document/component/mal/README_zh.md`
