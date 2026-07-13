# BlockDevice 与 registry 实现状态

## 文档状态

- `status`: `supporting`
- `scope`: `block.device`、`block.registry`、VFS block mount 与 cache adapter
- `source`: `Modules/io/block/*.cppm`、`Modules/io/fs/fs_vfs.cppm`

`block::Device` 是 `fs::BlockDevice` 的别名。它保存 non-owning `ctx`、read/write/erase/flush
callback、block geometry 和 capability bits；接口本身不提供 ownership、locking 或 media discovery。

## Capability

| `Caps` | 含义 |
|---|---|
| `read` | 提供 read callback |
| `write` | 提供 write callback |
| `erase` | 提供 erase callback |
| `flush` | 提供 flush callback |
| `cached` | device 是 cache adapter |

`caps_from_ops()` 只根据 callback 计算 bit mask，不修改 device。具体 node/adapter 可以在
`caps == 0` 时调用它补全能力；直接构造 `Device` 的 caller 仍需自行设置或推导。`is_read_only()`
只检查 `Caps::write`，不会检查 write callback 是否与 caps 一致。

## Registry

`block::Registry<MaxDevices>` 使用 caller-owned 固定数组，不分配内存。`register_device()` 要求：

- name 非空且 cap 非零；
- name 在 registry 中唯一；
- cap 在 registry 中唯一；
- 尚有可用 slot。

错误分别为 `invalid_arg`、`exist` 和 `buffer_overflow`。`replace_device()` 只替换 name 与 cap
同时匹配的既有 endpoint；`unregister_device()` 可按 name 或 cap 删除。`open_device()` 返回底层
device 指针或 `nullptr`，registry 不拥有该 device。

`cap_id()` 使用稳定的 32-bit FNV-1a hash，并将结果 `0` 映射为 `1`。registry 不处理 hash collision；
碰撞会表现为重复 cap。

`RegistryBinding` 将 registry 作为 `block.registry` capability 接入 init graph。具体 block node 在
自身 init callback 中注册 endpoint；稳定的 capability name 由装配层决定，例如 `block.sd0`。

## VFS 与 cache

`fs::vfs_mount_block()` 按 name 或 cap 从 registry 查找 device，再交给 filesystem mount adapter；
缺失 device 和 mount 失败沿现有 `fs::Status` 返回。

`block::CachedDevice<MaxEntries>` 是 non-owning、caller-buffered 的可写 block cache adapter，输出
device 带 `Caps::cached`。具体 cache 行为与边界见
[`fs_block_cache_strategy.md`](fs_block_cache_strategy.md)。

## 未提供

- registry 并发安全、引用计数或 device 生命周期管理；
- capability hash collision resolution；
- caps 与 callback 一致性的全局校验；
- hot-plug policy、持久命名或自动 discovery；
- transaction、filesystem 或 partition 语义。

验证入口：`Examples/fs/fs_block_vfs_demo`、`Examples/init/bringup_block_observe_demo`、
`Examples/system/device_runtime_block_slot_demo` 及 registry self-check。
