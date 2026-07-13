# FatFs adapter 与示例状态

## 文档状态

- `status`: `supporting`
- `scope`: `fs_fatfs` adapter 与 file-backed host fixture
- `source`: `Modules/io/fs/fs_fatfs.cppm`、`Examples/fs/fs_fatfs_demo`

`CHARM_ENABLE_FATFS=ON` 时，Charm 从 `Modules/thirdparty/fatfs` 编译 FatFs，并启用
`fs::FatFsMount`。关闭时保留 stub，mount/unmount 返回 `Errc::nosys`。

## Adapter

`FatFsMount` 可接收 `BlockDevice` 或 `MalDevice`；MAL 路径先通过 `mal_to_block()` 进入相同
diskio adapter。mount 成功后，caller 通过 `fs::add_mount()` 或 `fs::vfs_mount_block()` 注册到
VFS。

当前固定资源默认值：

- file slots：8，可由 `CHARM_FATFS_MAX_FILES` 或 caller-owned slot span 覆盖；
- path buffer：两块、每块 256 TCHAR，可由 `CHARM_FATFS_MAX_PATH` 或 caller-owned span 覆盖；
- physical drive slots：4，可由 `CHARM_FATFS_MAX_PDRV` 覆盖；
- optional diskio cache：caller-owned 单 block buffer。

`format_if_needed` 只在 FatFs 返回 `FR_NO_FILESYSTEM` 时尝试 FAT32 mkfs；未启用 `FF_USE_MKFS`
则返回 `Errc::notsup`。FatFs error 通过 `err_from_fr()` 映射为 `fs::Errc`。

`pdrv` 已用于 `disk_*` device/cache 路由，但当前 mount/unmount 调用仍使用空 FatFs drive path。
因此不能仅凭 `CHARM_FATFS_MAX_PDRV` 宣称多盘 VFS mount 已闭环；显式 drive path 与多 mount
隔离仍需单独验证。

## Host fixture

`Examples/fs/fs_fatfs_demo` 强制启用 FatFs，打开 512-byte block 的 disk image，读取 MBR 的第一个
FAT32 partition offset，挂载 image 或该 partition，列出根目录并尝试读取 `/hello.txt`。fixture
只证明 file-backed read path，不定义产品 partition、format 或 write policy。

## 未提供

- filesystem 或 block device ownership；
- thread safety、async IO 或 hot-plug policy；
- journal、transaction 或 power-fail guarantee；
- 已验证的多盘 mount contract；
- close 时自动执行 mount flush。

cache 行为见 [`fs_block_cache_strategy.md`](fs_block_cache_strategy.md)，VFS 路由见
[`fs_vfs_mount_rules.md`](fs_vfs_mount_rules.md)。
