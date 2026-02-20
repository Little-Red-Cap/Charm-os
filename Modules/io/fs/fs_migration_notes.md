# FS 迁移记录（Draft）

## 目标
- 内核层走 node + stream 风格（类似 VSF），上层可做 POSIX facade。
- 外部适配优先覆盖：FatFs + file-backed block device（PC 端验证）。

## 已迁移模块
- fs_errno：错误码
- fs_stream：统一流概念
- fs_core：node/file/mount 基础结构
- fs_path：轻量 path 辅助
- fs_block：块设备抽象
- fs_mal：统一 block/flash/file 的 MAL 抽象（可选）
- fs_ramfs：RAMFS 简化实现
- fs_blockfs：BlockFs 简化实现
- fs_fatfs：FatFs 适配入口（支持 Block/MAL）

## 已完成特性
- LFN 列目录支持（lfname 优先，UTF‑16 → UTF‑8）
- open 语义收敛（只读不创建）
- file-backed block device 64-bit seek（2GB+ 镜像）
- FatFs 外部缓存与路径缓冲入口
- FatFs 自定义文件槽（可配置最大打开数）
- FatFs 多盘注册（pdrv 多实例）
- VFS 调度全链（open/close/read/write/seek/flush）
- Block cache 策略落点确认（见 docs/fs_block_cache_strategy.md）

## 示例
- `Examples/fs/vsf_fs_fatfs_demo`

## 待办
- Block cache（可选，位于 MAL 或 VFS 层）
- POSIX facade 桥接
- RAMFS 完整实现（多块/目录）
- 后续适配层：ROMFS/FlashFS
