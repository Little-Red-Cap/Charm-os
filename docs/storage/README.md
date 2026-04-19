# 存储文档入口

本目录收纳 Charm 当前与块设备、MAL、VFS、FatFs 挂载和缓存策略相关的材料。

如果你是第一次进入仓库，先读：

1. [`../overview.md`](../overview.md)
2. [`../architecture_overview.md`](../architecture_overview.md)
3. [`../README.md`](../README.md)

再回到这里按任务进入。

## 先怎么判断这里的文档

- `*_contract.md`
  优先视为现行接口或行为约束。
- `*_overview.md`
  优先视为主题入口或总览。
- `*_demo.md`
  优先视为示例说明、验证路径或使用样例，不自动等同于长期契约。
- `*_map.md` / `*_strategy.md`
  优先视为对标材料、映射材料或专题策略说明。

## 按任务进入

### 我想知道块设备的现行规则

先读：

- [`block_device_contract.md`](block_device_contract.md)

这篇更偏当前最小契约，适合回答：

- `block.device` / `block.registry` 怎么命名和注册
- VFS 怎么从 `block.registry` 挂设备
- 缓存代理怎么进入系统

### 我想理解 MAL（Memory Abstraction Layer）

先读：

- [`mal_overview.md`](mal_overview.md)

再按需要继续：

- [`mal_fatfs_demo.md`](mal_fatfs_demo.md)

### 我想看 VFS / 挂载规则

先读：

- [`fs_vfs_mount_rules.md`](fs_vfs_mount_rules.md)

如果你同时关心缓存策略，再读：

- [`fs_block_cache_strategy.md`](fs_block_cache_strategy.md)

### 我想看 FatFs 的当前验证路径

先读：

- [`fs_fatfs_demo.md`](fs_fatfs_demo.md)
- [`mal_fatfs_demo.md`](mal_fatfs_demo.md)

如果你想回到对应示例，再进：

- [`../../Examples/fs/README.md`](../../Examples/fs/README.md)

### 我想看参考映射和对标材料

读：

- [`filex_charm_map.md`](filex_charm_map.md)

这篇更偏“借鉴什么、映射到哪里”，默认不是现行行为契约。

## 当前建议阅读顺序

- 看现行规则：
  `block_device_contract.md` → `fs_vfs_mount_rules.md`
- 看统一抽象：
  `mal_overview.md`
- 看落地示例：
  `fs_fatfs_demo.md` / `mal_fatfs_demo.md`
- 看策略与参考：
  `fs_block_cache_strategy.md` / `filex_charm_map.md`

## 使用提醒

- 如果这里的说明和当前代码、`docs/system/*` 或 `docs/io/*` 冲突，优先回到更上位入口复核。
- 当块设备能力命名、VFS 挂载方式、MAL 接口或缓存策略变化时，应同步更新本目录入口。
