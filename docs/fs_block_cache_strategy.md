# Block Cache 位置与策略（决定稿）

目标：明确 block cache 放置层级，避免重复缓存与一致性问题。

## 结论（当前决策）

- **首选：MAL 层封装缓存（cached_mal 风格）**
- VFS/FS 层默认不再引入额外 block cache
- 只有在需要“按文件语义优化”的场景才考虑 FS 层缓存（另开模块）

理由：
- MAL 是统一 block/flash/file 的抽象层，在此处缓存收益最大、耦合最小。
- 避免 FatFs/BlockFs 内部再做一层 block cache 造成重复与一致性复杂化。

## 层级划分

```
[Device Driver] -> [MAL cached wrapper] -> [FS/VFS] -> [App]
```

- Driver 可能已有硬件缓存（由驱动自行管理）
- MAL 缓存提供统一策略（LRU/直写/回写）
- FS/VFS 只做语义层（open/rename/目录等）

## 缓存策略（建议默认）

- **写策略**：Write‑through（默认）
  - 简单、安全，适合 MCU。
- **读策略**：LRU + 单扇区缓存（默认）
  - 足够覆盖 FAT/FATFS 的热点访问。

## 与 FatFs 的关系

- FatFs 内部仍有自己的“扇区缓冲机制”，但它依赖 `disk_*` 的行为。
- 建议：
  - 当启用 MAL cache 时，**不要**再额外启用 FatFs 自定义 cache（避免双层缓存）。
  - 若必须使用 FatFs cache，则关闭 MAL cache（或仅保留最薄的 read‑through）。

## 决策速查（避免双层缓存）

| 场景 | 推荐缓存层 | 备注 |
| --- | --- | --- |
| MCU 默认 / 简单存储 | MAL | 统一策略，最低耦合 |
| 已依赖 FatFs 自带缓存行为 | FatFs | 关闭 MAL cache |
| 高并发小文件随机 IO | FS 层（独立模块） | 与 MAL cache 互斥或只保留极薄 read‑through |

## 明确约束（工程规范）

- 默认只启用 **一层** block cache。
- `MAL cache` 与 `FatFs cache` 不可同时启用（除非明确评估并记录）。

## 接口草案（仅作为规范说明）

```
MalDevice raw = ...;
CachedMal cache = make_cached_mal(raw, CachePolicy{.write_through=true, .capacity_sectors=16});
FatFsMount fat;
fat.mount(cache.device(), ...);
```

> 具体实现可后置，本文件只定义“落点与策略”。

## 何时需要 FS 层缓存

- 大量小文件随机读写
- 需要“按文件语义”做预读/合并写

在这些场景下，FS 层缓存应作为独立模块（例如 `fs_block_cache`），并明确与 MAL cache 互斥或协作。

## 后续动作

- 引入 `fs_mal_cache`（可选）作为 MAL 层缓存封装
- 在 `docs/fs_fatfs_demo.md` 中注明缓存策略选择
- 为 USB MSC / 网络盘场景准备可插拔缓存策略
