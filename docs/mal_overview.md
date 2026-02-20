# MAL（Memory Abstraction Layer）概览

目标：统一 block/flash/file 三类后端的访问形状，让 FS/VFS 只面对一种“块设备能力”。

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
- `fs_mal` 作为统一层，提供双向适配

```cpp
MalDevice mal = make_mal_from_block(block_dev);
BlockDevice block{};
mal_to_block(mal, block);
```

这使得 FatFs / BlockFs 等现有实现可无缝迁移到 MAL。

## 4. 驱动接口模型（单入口规范）

借鉴 FileX 的“单入口驱动”模型，建议在 MAL 驱动侧提供可选规范：

```cpp
enum class MalRequest {
  read,
  write,
  flush,
  init,
  uninit,
  boot_read,
  boot_write,
  release,
};

struct MalDriverIo {
  void* driver_info{};  // 对应 MalDevice::ctx
  u64 lba{};
  u32 count{};
  span<u8> buffer{};
  bool system_write{};  // FAT/目录等系统扇区写
};

using MalDriverEntry = Status (*)(MalRequest, MalDriverIo&) noexcept;
```

说明：
- `driver_info` 对应 `MalDevice::ctx`，承载具体设备上下文（文件句柄、VHD、SPI 句柄等）。
- 单入口驱动适合统一设备栈（RAM/Flash/USB/远端）。
- `MalOps` 仍保留为高层稳定接口，可由 `MalDriverEntry` 生成或反向封装。

## 5. 适用场景

- File-backed block device（VHD/镜像文件）
- Flash-backed block device（SPI Flash）
- Remote block device（网络盘/USB MSC）

## 6. 下一步计划

- 引入 `mal_block` / `mal_file` 示例适配
- FatFs 挂载接口已支持 MAL 入口（保留 BlockDevice 入口）
