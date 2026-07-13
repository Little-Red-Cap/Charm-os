# Storage 文档入口

## 文档状态

- `status`: `supporting`
- `scope`: block、MAL、VFS、FatFs 与 cache 路由
- `authority`: 当前 `Modules/io/block`、`Modules/io/fs` 源码

| 任务 | 入口 |
|---|---|
| BlockDevice / registry | [`block_device_contract.md`](block_device_contract.md) |
| MAL implementation | [`mal_overview.md`](mal_overview.md) |
| VFS mount | [`fs_vfs_mount_rules.md`](fs_vfs_mount_rules.md) |
| block/MAL cache | [`fs_block_cache_strategy.md`](fs_block_cache_strategy.md) |
| FatFs + BlockDevice | [`fs_fatfs_demo.md`](fs_fatfs_demo.md) |
| FatFs + MAL | [`mal_fatfs_demo.md`](mal_fatfs_demo.md) |
| 示例 | [`Examples/fs/README.md`](../../Examples/fs/README.md) |

FileX 对标属于外部参考，不是当前接口契约：
[`filex_charm_map.md`](../reference/filex_charm_map.md)。

文档与实现冲突时，以 module、CMake source collection 和当次测试为准。
