# FS 迁移记录 (Draft)

## 目标
- 内核层走 node+stream 风格（类似 VSF），上层可做 POSIX fa?ade。
- 现阶段不做外部适配（FatFS/FlashFS），先完成 VFS + RAMFS + Block 抽象。

## 已迁移模块
- fs_errno: 错误码
- fs_stream: 统一流概念
- fs_core: node/file/mount 基础结构
- fs_path: 轻量 path 辅助（占位）
- fs_block: 块设备抽象
- fs_ramfs: RAMFS 占位实现（简化版）
- fs_fatfs: FatFs 适配入口（需 CHARM_ENABLE_FATFS + FatFs 源码）

## 示例
- Draft/Examples/vsf_fs_demo

## 待办
- 完善 VFS 调度：mount/open/close/read/write/seek/flush 走 NodeOps
- Block cache（可选）
- POSIX fa?ade 桥接
- RAMFS 完整实现（多块/目录）
- 后续适配层：ROMFS/FlashFS
