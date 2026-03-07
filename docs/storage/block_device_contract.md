# block.device / block.registry 规范（最小契约）

目标：把所有存储后端统一为 block.device，并通过 block.registry 装配进 init.graph。

## 1) 设备能力（Caps）

block.device 的能力位用于“只读/擦写能力”判断：

- `read`：可读
- `write`：可写（无此能力视为只读）
- `erase`：支持擦除
- `flush`：支持 flush
- `cached`：该设备为缓存代理

规则：
- `write/erase` 缺失即视为只读
- `caps==0` 时允许通过函数指针自动推导

## 2) 注册与命名

block.registry 只接受**唯一 cap**，同名同 cap 不允许重复注册。

推荐命名：
- SD/TF：`block.sd0`
- 外部 Flash：`block.flash0`
- RAM disk：`block.ram0`

## 3) init.graph 依赖示例

核心链（CoreSystemChain）已包含：

- `block.registry`

板级链路示例：

```
platform.irq
  -> hal.sdmmc1
    -> block.sdmmc (provides block.sd0, requires block.registry + hal.sdmmc1)
```

## 4) VFS 挂载方式（统一入口）

上层只依赖 block.registry：

```cpp
fs::FatFsMount fat{};
auto st = fs::vfs_mount_block("/sd", block_registry, "block.sd0", fat);
```

## 5) 缓存代理

`block.cache` 提供最小缓存代理：

```cpp
block::CachedDevice<4> cache;
std::array<util::u8, 2048> buf{};
cache.bind(dev, buf);
auto& cached = cache.device();
```

缓存代理设备会带 `Caps::cached`，上层可识别。
