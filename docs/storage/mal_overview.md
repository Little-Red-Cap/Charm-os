# MAL 实现状态

## 文档状态

- `status`: `supporting`
- `scope`: `fs_mal*` 当前 implementation interface
- `source`: `Modules/io/fs/fs_mal*.cppm`

MAL 将 block/flash/file backend 投影为 LBA + geometry 接口。它是 FS implementation layer，
不是 Charm Core 或跨系统稳定 ABI。

## 接口

`MalDevice` 保存 non-owning context、read/write/erase/flush ops、geometry 和 kind metadata。Read/write
按 LBA 处理一个或多个完整 block，erase/flush 可选。

缺少 callback 时 wrapper 返回 `Errc::nosys`。基础 `mal_read/write` 不检查 buffer 是否为 block size
整数倍或 LBA 范围；backend 必须执行自己的校验。`MalKind` 只是 metadata，不改变 wrapper 行为。

## Adapter

当前 adapter 可包装现有 `BlockDevice`、打开 file-backed block storage，或在 caller-owned buffer 上
提供固定 entry 的 read-through/write-through cache。具体 module 拆分以源码为准。

`mal_to_block()` 可将 MAL 投影回 `BlockDevice`。转换不增加 ownership、locking、transaction 或
media discovery。

`CachedMal::bind()` 要求有效 geometry 和 read/write callback；cache buffer 至少容纳一个 block。
读写检查 block 对齐和 LBA 范围，write 成功后更新 cache，erase 成功后使范围失效，flush 直接转发。

## FatFs

`FatFsMount` 可以接收 `BlockDevice` 或 `MalDevice`，后者先通过 `mal_to_block()` 进入相同 diskio
路径。可选 cache buffer 由 caller 提供。mount、format-if-needed、file slot、UTF path 与多盘边界
见 [`fs_fatfs_demo.md`](fs_fatfs_demo.md)。

## 未提供

- `MalDriverEntry` 单入口 request ABI；
- 自动 device registry/discovery；
- transaction、journaling、wear leveling 或 power-fail guarantee；
- thread safety、async IO、cancel 或 timeout；
- 对 block/flash/file kind 的统一 erase/flush policy。

验证入口：`Examples/fs/fs_fatfs_demo`、`Examples/fs/fs_block_vfs_demo` 及相关 FS tests。
