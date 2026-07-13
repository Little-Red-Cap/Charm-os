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

- first-party source 不得 import entry contract 标记的兼容或退役入口；匹配按完整 module 名边界执行。
- Kernel core 不得 import `out.*`；`*_export.cppm` 是 presentation/export 例外。
- 新增 `Modules/**/charm.*.cppm` 必须进入 stable 或 non-stable 台账。
- 稳定入口文件必须存在，不得 re-export 历史入口或名称含 `internal/bridge/compat/alias` 的过渡表面。

入口集合、分类和聚合边界只由
[`entry_surface_contract.md`](entry_surface_contract.md) 与 CMake 台账维护；检查通过不授予 Core 身份。

## 不负责的事

- 不证明完整 module DAG、runtime binding、CMake 条件分支或动态装配合法；
- 不统一不同领域的错误、lifecycle 或 interface；
- 不把稳定入口、Provider、Driver 或 Graph 提升为 Charm Core；
- 不替代真实 consumer、host/QEMU/board run 或行为测试。
