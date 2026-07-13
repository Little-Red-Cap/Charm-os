# VFS mount 实现状态

## 文档状态

- `status`: `supporting`
- `scope`: `fs_vfs` mount table、路径调度与 block mount adapter
- `source`: `Modules/io/fs/fs_vfs.cppm`、`Modules/io/fs/fs_path.cppm`

## Mount table

VFS 使用进程内固定表保存最多 8 个 `MountPoint`。`add_mount(prefix, mount)` 去掉 prefix 的前导
分隔符后，保存 non-owning `std::string_view` 和 `Mount*`；caller 必须保证两者生命周期覆盖注册期。

`add_mount()` 的当前失败语义：

- null mount：`Errc::inval`；
- mount table 已满：`Errc::busy`。

它不检查重复 prefix。`clear_mounts()` 只清空表，不调用 filesystem unmount。
`remove_mount()` 也只移除记录；`vfs_unmount()` 才会先调用可选的 `MountOps::unmount`。

## 路径匹配

`find_mount()` 去掉 path 的前导分隔符，并选择最长字符串前缀；空 prefix 可作为 fallback root mount。
当前匹配不检查路径组件边界，因此 prefix `d0` 也会匹配 `d01/file`。mount naming 与冲突规避仍由
装配层负责，源码没有规定 `/d0`、`/d1` 等产品命名。

选中 mount 后，VFS 去掉 prefix，再把剩余相对路径交给 `MountOps`。未找到 mount 或缺少对应 op
时，多数 file operation 返回 `Errc::nosys`；跨 mount rename 返回 `Errc::notsup`。

## Block adapter

`vfs_mount_block()` 可按 registry name 或 cap 查找 `block::Device`。device 缺失返回 `Errc::noent`；
filesystem mount 错误与 `add_mount()` 错误原样返回。

## Flush 与 dirty state

成功的 write/unlink/rename/truncate/mkdir 会标记 mount dirty。`vfs_flush(file)` 只调用 file node
flush；`vfs_flush(prefix)` 调用 mount flush，成功后清除 dirty。`vfs_close()` 只调用 node close，
不隐式 flush mount。

## 未提供

- mount table locking、动态扩容或 ownership；
- prefix 重复和路径组件边界校验；
- mount namespace、权限或 sandbox；
- 自动 mount discovery 或持久化 mount policy。

验证入口：`Examples/fs/fs_vfs_demo`、`Examples/fs/fs_block_vfs_demo`。
