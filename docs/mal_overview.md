# MAL（Memory Abstraction Layer）概览

目的：统一 block/flash/file 这三类后端的访问形状，让 FS/VFS 只面对一种“块设备能力”。

## 1. 设计目标

- 上层只关心 LBA + block_size + block_count
- 后端可替换（文件、虚拟盘、SPI Flash、网络盘）
- 与现有 `fs_block` 兼容，逐步迁移

## 2. 核心接口

入口模块：`fs_mal`

```cpp
enum class MalKind { block, flash, file };

struct MalOps {
  Status (*read)(void* ctx, u64 lba, span<u8>) noexcept;
  Status (*write)(void* ctx, u64 lba, span<const u8>) noexcept;
  Status (*erase)(void* ctx, u64 lba, u64 count) noexcept;
  Status (*flush)(void* ctx) noexcept;
};

struct MalDevice {
  void* ctx{};
  MalOps ops{};
  u64 block_size{};
  u64 block_count{};
  MalKind kind{MalKind::block};
};
```

## 3. 与 fs_block 的关系

- `fs_block` 仍是当前最小块设备抽象
- `fs_mal` 作为统一层，提供双向适配：

```cpp
MalDevice mal = make_mal_from_block(block_dev);
BlockDevice block{};
mal_to_block(mal, block);
```

这使得 FatFs / BlockFs 等现有实现可无缝迁移到 MAL。

## 4. 适用场景

- File-backed block device（VHD/镜像文件）
- Flash-backed block device（SPI Flash）
- Remote block device（网络盘/USB MSC）

## 5. 下一步计划

- 引入 `mal_block`/`mal_file` 示例适配
- FatFs 挂载接口已支持 MAL 入口（保留 BlockDevice 入口）
