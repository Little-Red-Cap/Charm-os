# OnlyCore 提纯清单

## 文档状态

- `status`: `supporting`
- `scope`: OnlyCore 当前边界、迁移记录和证据债务
- `authority`: [`../../CONSTITUTION.md`](../../CONSTITUTION.md) 与
  [`../architecture/charm_core_contract.md`](../architecture/charm_core_contract.md)

本清单记录提纯后的仓库边界，不授予任何概念 Core 身份，也不替代源码、CMake target 或当次验证结果。
OnlyCore 的提交历史保持不改写；未列出的历史文件不因此获得当前准入。

## 已移出 OnlyCore 活动树

| 范围 | 当前处理 | 后续证据或所有者 |
|---|---|---|
| Vivid、Player、UI、Audio 与产品资源 | 从当前活动树移出 | 独立 Vivid / 产品工程承接 |
| Backend、provider、板级 SDK、BSP、startup、linker 与 vendor 工程 | 从当前活动树移出 | Project/BareWorkspace 与真实板工程承接 |
| init graph、RTE、Spine、旧 topology、System Compiler、IR、Graph 与 runtime facade | 旧探索退役 | 仅从快照分支或 Git 历史追溯 |
| Resident ELF、ModuleX、部署和产品生命周期接线 | 从当前活动树移出 | 独立 deployment / 产品工程承接 |
| 旧 `charm.core`、`charm.foundation`、`semantic.core` 聚合 facade | 删除，不保留兼容入口 | 消费方直接使用当前公共关系投影 |

## 已重建或保留的参考证据

| 证据 | 当前范围 | 不覆盖 |
|---|---|---|
| Host relations | Clang/GCC、三组编译期 key 负例 | QEMU 或真实板运行 |
| Host MVP | 正例、完整 failure matrix、Clang/GCC、ASan/UBSan | 跨域固件启动 |
| QEMU MVP | `virt/cortex-a15` 同一 app 的正例与 `missing_binding` | 完整 failure matrix、真实时钟、持久存储、真实板 |
| ARM freestanding | `arm-none-eabi-g++` compile-only | 链接、启动和运行时行为 |
| Installed consumer | `find_package(CharmCore)` 的独立 Host 消费 | ABI 与版本策略 |

QEMU reference consumer 位于 [`Examples/system/charm_capability_mvp/qemu`](../../Examples/system/charm_capability_mvp/qemu)，
只复用共享 `mvp_app.hpp`；平台启动、PL011、链接和内存 BlockDevice 属于该 supporting 证据目录。

## Rejected / Deferred

- 公共 resolver、`ResolvedBinding`、`ResolutionFailure`、Profile、Provider base、Manager、Registry、ContextView 和 Evidence collector。
- Backend、Driver、Loader、Compiler、IR、Graph、Runtime、RTE、Spine、topology 和 init graph 的统一 Core 抽象。
- `TextSink`、`Clock`、`BlockDevice` 等具体 Contract 的全局 canonical 化。
- 关系投影已删除默认构造；项目 enum 的 `0` 值仍由各 consumer 自行定义，不进入公共语义。
- 真实板跨环境 MVP 证据；需要独立板级工程、配置和可重复运行记录。

## 待独立项目承接

- Vivid UI runtime、Player 页面、主题和产品资产。
- Backend/provider 实现、BSP、板级启动与硬件证据。
- IO、存储、系统 runtime、Resident ELF、部署格式和产品 CI。
- 旧探索中的 topology、RTE、Spine、System Compiler、IR 与 Graph 工具。

## 当前允许的公共表面

- [`Modules/core/capability/relations.hpp`](../../Modules/core/capability/relations.hpp)：仅 `Requirement`、`Provision`、`Binding`。
- 根 `Charm::core` CMake target 及其安装导出。
- supporting 级别的 Host/QEMU/ARM/安装消费证据；这些证据不反向增加 Core 类型。

## 维护规则

1. 新增条目必须指向真实 consumer、源码路径和当次验证；目录名或历史调用量不能作为准入依据。
2. 每次只处理一个边界问题；停止大批量删除，保留可审查的小提交。
3. 主线只选择性借鉴已验证的契约、代码、测试和构建结果，不直接合并 OnlyCore 分支。
