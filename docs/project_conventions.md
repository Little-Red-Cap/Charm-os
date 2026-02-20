# 项目规范与注意事项

本文件用于说明 Charm 项目内的统一约定，避免重复造轮子与语义分裂。

## 1) 格式化与日志

- 统一使用 `out` 作为格式化/日志能力。
- 禁止直接使用 `std::printf`/`sprintf`/`snprintf`。

替换规则：

- `std::printf` -> `out::printf`
- `sprintf`/`snprintf` -> `out::format` 或 `out::printf`

原因：
- `out` 是当前项目统一的格式化与诊断能力，便于后续移植与替换。

位置：
- `Modules/out/*`

## 2) 容器与动态内存

- 默认使用 ETL 容器替代 STL 容器。

替换规则：

- `std::vector` -> `etl::vector`
- `std::string` -> `etl::string`（如需动态字符串）
- `std::deque` -> `etl::deque`

原因：
- MCU 约束下可控容量与行为，避免隐式动态分配。

位置：
- `Modules/thirdparty/etl/*` (通过 CMake 引入)

例外：
- PC 端验证工具或仅用于 Debug 的代码可保留 STL，但需在注释中说明。

## 3) Span 与 byte

- 默认使用 `std::span` 与 `std::byte`。
- 如需在 MCU 上替换为 ETL，请使用 `Modules/core/util` 提供的别名或包装。

位置：
- `Modules/core/util/*`

## 4) 错误处理与返回约定

- 使用 `Status/Err`（或 `std::expected` 约定）表达失败原因。
- 不使用异常进行控制流。

位置：
- `Modules/core/util/*`（expected 风格）
- `Modules/io/fs/*`（Status/Err）

## 5) 模块分层与依赖

- Foundation：`core/*`、`out/*`
- Runtime：`kernel/*`、`io/*`、`fs/*`、`modulex/*`
- Domain：`media/*`、`ui/*`、`shell/*`

约束：
- 低层模块不得依赖高层模块。
- 依赖需要写入“依赖白名单”文档并在 CMake 层校验。

## 6) 示例与测试

- 示例工程仅用于验证主线能力，不引入新的基础设施。
- 新能力落地后，至少提供一个最小可复现实例或测试。

## 7) 命名与导出

- 模块导出统一写入 `charm.*.cppm` 聚合入口。
- 新模块请同步更新 `docs/architecture_overview.md` 或对应目录文档。

---

如需新增规范，请提交修改本文件并说明动机与收益。
