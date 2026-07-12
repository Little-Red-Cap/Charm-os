# Dependency Contract

## 文档角色

本文是 supporting 依赖契约，描述当前可以由源码和 CMake 检查证明的边界。它不从目录名推导 Charm Core，也不宣称仓库已经具备完整模块 DAG verifier。

Core 准入以 [`../../CONSTITUTION.md`](../../CONSTITUTION.md) 为准；入口分类以 [`entry_surface_contract.md`](entry_surface_contract.md) 为准。

## 当前事实

仓库不能准确表示为单一的：

```text
Foundation -> Runtime -> IO -> Domain
```

实际 module imports 中：

- `Modules/system` 会消费 block、HAL、input、IO 和 platform，以完成 bring-up 与系统装配；
- `Modules/io` 会消费 kernel、ModuleX 和 service，以实现 reactor、POSIX、USB 等运行时能力；
- media/UI 会消费 IO、out、service 和算法；
- `charm.*` 聚合入口会 re-export 多个叶子模块。

因此 `Foundation/Runtime/IO/Domain` 只能作为历史实现分区，不是已被工具证明的严格层级。

## 必须保持的边界

### Core

- `Modules/core` 保持平台无关，不依赖具体 system、IO、media、UI、board 或 project 实现。
- 目录位于 `Modules/core` 不代表概念自动进入 Charm Core；仍需通过 Constitution 准入。
- 业务、backend、driver 和 project 事实不得反向定义 Core 词汇。

### 入口

- 新代码使用稳定聚合入口或更窄的叶子模块。
- first-party 代码不得新增 `charm.foundation`、`charm.runtime`、`charm.domain` import。
- 新增 `Modules/**/charm.*.cppm` 必须进入入口台账，不能靠文件名成为公共 API。
- 稳定聚合入口不得 re-export internal/bridge/compat/alias 表面。

### 所有权

- platform/board 细节停留在平台与 binding 层，不进入 App 或 Core 契约。
- 静态硬件能力通过 `init.graph` 装配；动态设备通过 discovery/export 路径进入消费面。
- kernel core 不承担 text/JSON/CSV presentation；导出逻辑放在明确的 export/adapter 模块。
- UI、media 和 project 可以消费底层能力，但不得要求底层依赖具体 UI、Player 或板级业务。

这些边界允许真实的 system/IO 双向协作，但要求依赖有明确所有者和消费者，而不是通过大 facade 隐藏。

## 当前自动检查

`CHARM_ENABLE_DEPENDENCY_WHITELIST=ON` 启用 [`../../cmake/DependencyWhitelist.cmake`](../../cmake/DependencyWhitelist.cmake)。它当前检查：

- first-party tree 的退役/兼容 facade import；
- `charm.*.cppm` 入口台账与稳定入口卫生；
- kernel core 对 `out.*` 的 presentation 边界。

它当前不检查：

- 所有 module import 组成的完整 DAG；
- 每个叶子模块的允许依赖列表；
- project/backend 是否违反业务所有权；
- runtime 行为、初始化顺序或 capability 唯一性。

该检查默认关闭，是 opt-in 配置证据，不得写成“所有构建都强制”。完整规则见 [`dependency_whitelist.md`](dependency_whitelist.md)。

## 变更规则

新增或调整依赖时：

1. 先确认消费者确实需要该依赖，而不是为了复用便利扩大聚合入口。
2. 优先 import 叶子模块；不要新增全仓 facade。
3. 如果跨越所有权边界，记录原因、替代方案和退出条件。
4. 更新对应目录 README 或专题契约；只有顶层边界变化才更新全局实现地图。
5. 运行受影响 target，并在涉及入口卫生时启用 dependency whitelist。

## 非目标

- 不要求所有对外 API 使用同一种错误类型。
- 不禁止 system 与 IO 的所有相互依赖。
- 不要求 Domain 复用 Core 中每一个已有容器或算法。
- 不用文档分层替代 CMake、module imports 和可运行消费者证据。
- 不把当前 opt-in whitelist 描述为完整 system compiler。
