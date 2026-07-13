# Block Device Route

## 文档状态

- `status`: `supporting`
- `scope`: block device、MAL、cache 与 VFS mount 阅读路由

## 最短路径

1. [Storage 入口](../../storage/README.md)
2. [BlockDevice contract](../../storage/block_device_contract.md)
3. [MAL](../../storage/mal_overview.md)
4. [VFS mount](../../storage/fs_vfs_mount_rules.md)
5. [Block device skill](../skills/charm-block-device/SKILL.md)

先确定 block 行为与 ownership，再讨论 cache、bridge 和 filesystem mount；后者不能反向定义 block。
