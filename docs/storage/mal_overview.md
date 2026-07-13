# MAL 实现状态

## 文档状态

- `status`: `supporting`
- `scope`: `fs_mal*` 当前 implementation interface
- `source`: `Modules/io/fs/fs_mal*.cppm`

MAL 将 block/flash/file backend 投影为 LBA + geometry 接口。它是 FS implementation layer，
不是 Charm Core 或跨系统稳定 ABI。

## 接口

`MalDevice` 保存非 owning `ctx`、`MalOps`、`block_size`、`block_count` 和 `MalKind`。

| op | 语义 |
|---|---|
| `read(ctx, lba, bytes)` | 读取一个或多个完整 block |
| `write(ctx, lba, bytes)` | 写入完整 block |
| `erase(ctx, lba, count)` | 可选 erase |
| `flush(ctx)` | 可选 flush |

缺少 callback 时 wrapper 返回 `Errc::nosys`。基础 `mal_read/write` 不检查 buffer 是否为 block size
整数倍或 LBA 范围；backend 必须执行自己的校验。`MalKind` 只是 metadata，不改变 wrapper 行为。

## Adapter

| module | 行为 |
|---|---|
| `fs_mal_block` | 通过 `make_mal_from_block()` 包装现有 `BlockDevice` |
| `fs_mal_file` | 打开 file-backed `BlockFile` 并暴露 `MalDevice` |
| `fs_mal_cache` | caller-owned cache buffer、固定 entry metadata、read-through/write-through cache |

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
