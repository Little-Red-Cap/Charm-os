# VFS 挂载与多盘规则（草案）

目标：统一 VFS 路径解析与多盘规划，避免上层协议出现隐式耦合。

## 1. 当前规则

- VFS 通过 `add_mount(prefix, mount)` 注册前缀，按“最长前缀匹配”选中挂载点。
- FatFs 目前仍是 **单盘活跃**（`g_fatfs_pdrv`），但已支持配置 pdrv。

## 2. 建议的多盘前缀规则

推荐路径规则（保持最简单的一致性）：
- `"/"`：默认挂载（单盘）
- `"/d0"`：磁盘 0
- `"/d1"`：磁盘 1
- 以此类推

例如：
- `add_mount("/d0", fat0.mount_point())`
- `add_mount("/d1", fat1.mount_point())`

上层使用：
- `vfs_open("/d0/music/track.flac")`
- `vfs_open("/d1/logs/run.log")`

## 3. FatFs 的 pdrv 对接

当前 FatFs 注册函数支持 pdrv：
- `fatfs_register_block_device(dev, pdrv)`

短期约束：
- 仍是 **单盘活动**（单个 `g_fatfs_pdrv`）
- 若要多盘并存，需要把注册表扩展为数组或 map

## 4. 未来扩展（不强制）

可选的多盘注册表：
- `fatfs_register_block_device(dev, pdrv)` -> 写入 `g_fatfs_devices[pdrv]`
- `disk_*` 依 pdrv 查询对应设备

这样 VFS 的 `/dN` 前缀能与 FatFs 多盘一一对应。
