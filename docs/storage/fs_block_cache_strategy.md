# Block cache 实现状态

## 文档状态

- `status`: `supporting`
- `scope`: `fs_mal_cache`、`block.cache` 与 FatFs 可选 cache
- `source`: `Modules/io/fs/fs_mal_cache.cppm`、`Modules/io/block/block.cache.cppm`、`Modules/io/fs/fs_fatfs.cppm`

## MAL 与 BlockDevice cache

`fs::CachedMal<MaxEntries>` 是 `MalDevice` 的 non-owning cache wrapper。caller 提供 cache
buffer，wrapper 自己保存固定数量的 entry metadata；实际 entry 数量取 `MaxEntries` 与 buffer
可容纳 block 数的较小值。

`bind()` 要求有效 geometry、read/write callback，以及至少能容纳一个 block 的 buffer。读写只接受
完整 block，并检查 LBA 范围。

当前行为：

- read-through：命中时从 cache 返回，未命中时读取 backend 并填充 cache；
- write-through：先写 backend，成功后更新 cache；
- entry 满时按 stamp 选择最久未使用的 entry；
- erase 成功后使对应范围失效；
- flush 直接转发给 backend；backend 不支持时返回 `Errc::nosys`。

`block::CachedDevice<MaxEntries>` 通过 `MalBlock` 与 `CachedMal` 包装可写 `BlockDevice`，输出设备带
`Caps::cached`。它不拥有底层 device 或 cache buffer。缺少 read 返回 `Errc::nosys`，缺少 write
返回 `Errc::rofs`。

## FatFs cache

`FatFsMount` 的 cache overload 通过 `fatfs_set_cache()` 接收 caller-owned buffer。当前 diskio
adapter 只缓存单 block read/write；多 block write 会使重叠的单 block cache entry 失效。

FatFs cache 与 `CachedMal` / `CachedDevice` 可以同时接线，源码没有自动互斥门禁。双层 cache 会增加
内存占用和一致性分析成本，装配时需要显式评估；这不是当前 module 强制执行的“一层 cache”契约。

## 未提供

- write-back、dirty entry 或延迟刷写；
- transaction、journaling 或 power-fail guarantee；
- thread safety、并发仲裁或异步 IO；
- 按文件语义的预读、合并写或 FS-level cache policy；
- cache ownership、生命周期管理或自动装配策略。

验证入口：`Examples/fs/fs_block_vfs_demo`、`Examples/fs/fs_fatfs_demo` 及相关 FS tests。
