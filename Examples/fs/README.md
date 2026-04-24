# FS 示例入口

本目录收纳文件系统、块设备到 VFS、FatFs 挂载等最小验证示例。

如果你想先看现行规则，再看示例，建议先读：

- [`../../docs/storage/README.md`](../../docs/storage/README.md)

## 当前示例

### `fs_demo`

最基础的 FS 示例目录，带独立 `CMakePresets.json`。

适合：

- 先确认最小文件系统示例能否独立配置和构建
- 用最窄路径验证当前 FS 基础链路

### `fs_vfs_demo`

偏向 VFS 视角的最小示例。

适合：

- 结合 [`../../docs/storage/fs_vfs_mount_rules.md`](../../docs/storage/fs_vfs_mount_rules.md) 一起看

### `fs_block_vfs_demo`

有自己的入口文档：

- [`fs_block_vfs_demo/README.md`](fs_block_vfs_demo/README.md)

更适合看：

- `block.device -> vfs -> out` 的最小验证链
- invalid MBR / no FAT / successful mount 等基础 case

### `fs_fatfs_demo`

偏向 file-backed block device + FatFs 的示例目录。

建议和下面这篇一起看：

- [`../../docs/storage/fs_fatfs_demo.md`](../../docs/storage/fs_fatfs_demo.md)

## 建议阅读顺序

1. [`../../docs/storage/block_device_contract.md`](../../docs/storage/block_device_contract.md)
2. [`../../docs/storage/fs_vfs_mount_rules.md`](../../docs/storage/fs_vfs_mount_rules.md)
3. 本目录对应示例

如果你重点是 FatFs，再加上：

4. [`../../docs/storage/fs_fatfs_demo.md`](../../docs/storage/fs_fatfs_demo.md)
5. [`../../docs/storage/mal_fatfs_demo.md`](../../docs/storage/mal_fatfs_demo.md)

## 使用提醒

- 本目录里的示例主要回答“链路最小能不能跑起来”，不是完整的长期产品形态。
- 如果某个示例没有自己的 README，默认按目录名和 `CMakeLists.txt` / `main.cpp` 进入即可。
