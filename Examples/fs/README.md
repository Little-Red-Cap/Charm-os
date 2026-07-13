# FS 示例入口

存储契约与分层见 [`docs/storage/README.md`](../../docs/storage/README.md)。

| 示例 | 覆盖 |
|---|---|
| `fs_demo/` | 最小 FS 独立配置与基础链路 |
| `fs_vfs_demo/` | VFS mount 入口 |
| [`fs_block_vfs_demo`](fs_block_vfs_demo/README.md) | block device -> VFS -> out；invalid MBR/no FAT/success |
| `fs_fatfs_demo/` | file-backed block device + FatFs |

相关规则：

- [`block_device_contract.md`](../../docs/storage/block_device_contract.md)
- [`fs_vfs_mount_rules.md`](../../docs/storage/fs_vfs_mount_rules.md)
- [`fs_fatfs_demo.md`](../../docs/storage/fs_fatfs_demo.md)

示例只证明对应 fixture，不定义产品 filesystem、partition 或 mount policy。
