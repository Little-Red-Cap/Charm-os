# Block Device Route

## 适用场景

- block device 抽象
- 存储接线
- MAL / cache / mount 相关讨论

## 最短阅读顺序

1. [`../../storage/README.md`](../../storage/README.md)
2. [`../../storage/block_device_contract.md`](../../storage/block_device_contract.md)
3. [`../../storage/mal_overview.md`](../../storage/mal_overview.md)
4. [`../../storage/fs_vfs_mount_rules.md`](../../storage/fs_vfs_mount_rules.md)
5. [`../skills/charm-block-device/SKILL.md`](../skills/charm-block-device/SKILL.md)

## 先不要做什么

- 不要跳过 block 能力定义直接谈 FS 挂载。
- 不要把 cache、协议桥接和设备抽象混成一层。
- 不要忽略块大小、特性标志与错误模型。

## 完成前自检

- block 能力边界是否清楚。
- mount / cache / bridge 分层是否明确。
- 文档与示例入口是否同步。
