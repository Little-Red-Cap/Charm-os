# Dependency Whitelist

## 文档状态

- `status`: `supporting`
- `scope`: `DependencyWhitelist.cmake` 的 opt-in 配置期检查
- `source`: [`DependencyWhitelist.cmake`](../../cmake/DependencyWhitelist.cmake)

这不是完整依赖图编译器，也不是默认构建门禁。它只检查 first-party source tree 中几类已知入口
和 presentation 边界。

## 启用

```powershell
cmake -S . -B cmake-build-whitelist-check -G Ninja -DCHARM_ENABLE_DEPENDENCY_WHITELIST=ON
```

检查覆盖 `Modules`、`Examples`、`Draft` 中的 C/C++ 和 module source，排除 `cmake-build-*` 与
`Modules/thirdparty`。配置失败会报告文件、行号和匹配行。

## 检查项

### 历史入口

first-party source 不得 import：

- `charm.foundation`：兼容迁移 facade；
- `charm.runtime`：退役 tombstone；
- `charm.domain`：历史入口。

应按依赖意图使用稳定聚合或叶子 module。边界匹配不会把 `charm.runtime_extra` 误判为 `charm.runtime`。

### Kernel presentation

`Modules/system/kernel/*.cppm` 不得 import `out.*`；`*_export.cppm` 是 presentation/export 例外。
这只约束 kernel core 的导出边界，不规定所有 port adapter 的 IO 组织。

### Entry inventory

新增 `Modules/**/charm.*.cppm` 必须在 CMake 台账中分类。当前稳定聚合入口为：

- `charm.core`
- `charm.system`
- `charm.io`
- `charm.net`
- `charm.media`
- `charm.media.audio`
- `charm.ui.ink`
- `charm.ui.vivid`

当前非稳定分类为：`charm.foundation`、`charm.runtime`、`charm.core.event` 和
`charm.ui.vivid_internal`。分类只说明入口治理，不授予 Core 身份。

### Stable entry hygiene

稳定入口文件必须存在，不得 re-export 历史入口，也不得 re-export 名称包含 `internal`、`bridge`、
`compat` 或 `alias` 的过渡表面。稳定入口的分类与聚合边界见
[`entry_surface_contract.md`](entry_surface_contract.md)。

## 不负责的事

- 不证明完整 module DAG、runtime binding、CMake 条件分支或动态装配合法；
- 不统一不同领域的错误、lifecycle 或 interface；
- 不把稳定入口、Provider、Driver 或 Graph 提升为 Charm Core；
- 不替代真实 consumer、host/QEMU/board run 或行为测试。

入口分类和历史 facade 语义见 [`entry_surface_contract.md`](entry_surface_contract.md)。
