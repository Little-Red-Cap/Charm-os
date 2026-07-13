# POSIX / net modules-ts 构建解阻记录

> `status`: `archived`

本文保留一次 GCC `-fmodules-ts` 导入冲突的故障模式与修复取舍，不代表当前构建仍有相同问题。

## 症状与归因

`posix-qemu-demo` 曾在 `net.reactor`、`net.stack` 与 `posix.api` 的导入图中出现 `std::span`、
`std::array` 冲突和看似重复的默认构造声明。清理 `.gcm` 后仍能复现，因此不是单纯缓存污染。

根因判断是多个模块接口经不同路径重复暴露标准库模板类型，触发 GCC modules-ts 合并不稳定；
POSIX 语义本身不是故障域。

## 当时修复

1. `net.common` 用最小 `ByteView/MutByteView` 替代公开的 `std::span` 别名，只保留 data、size、迭代和
   subspan 语义。
2. `net.posix` 在 POSIX/net byte view 边界显式转换，不依赖隐式模板兼容。
3. `SocketService` 改为依赖 `SocketProviderRef`；兼容 `bind_stack()` 只提取 provider，避免接口单元
   直接导入 `net.stack`。
4. `posix.api` 删除不必要的 `net.socket` 直接导入，缩小高层导入图。

当时重新构建 POSIX QEMU target 并运行主 smoke 后恢复通过。该结果只说明这组修改解决了当时故障，
当前状态仍应由源码、构建和当次 QEMU 运行确认。

## 可复用规则

- 基础模块接口避免经多路导入暴露复杂标准库模板实例。
- 两个子系统共享语义但不共享稳定类型时，在边界显式桥接最小 View/Ref/Ops。
- 只需要 provider 能力时，不依赖更高层包装类型。
- modules-ts 报错应先检查导入图和接口暴露面；只有证据指向缓存时才按缓存故障处理。
