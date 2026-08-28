# Charm 实现地图

## 文档状态

- `status`: `supporting`
- `scope`: 当前源码分区、公共入口与主要运行路径
- `authority`: [`CONSTITUTION.md`](../CONSTITUTION.md)

本文回答代码放在哪里、从哪里进入。它不定义 Charm Core，也不记录易失效的构建或测试进度。
旧版阶段盘点只从快照分支或 Git 历史追溯。

## 仓库结构

| 路径 | 用途 |
|---|---|
| `Modules/core/capability/relations.hpp` | 当前唯一保留的 Core 公共关系投影 |
| `Examples/system/charm_capability_relations` | 关系模型的最小 Host 证据 |
| `Examples/system/charm_capability_mvp` | Capability MVP 的 Host 证据与失败矩阵 |
| `docs/architecture` | Core 契约、审计和准入记录 |
| `scripts/check_charm_core_governance.ps1` | Canonical 文档和链接的治理检查 |

目录只表示当前所有权，不授予 Core 身份。

## 导入入口

OnlyCore 当前没有公共 C++ module 聚合入口。消费者直接包含
`Modules/core/capability/relations.hpp`；旧 `charm.core`、`semantic.core`、`init` 和其它 module
facade 已从 OnlyCore 移除。

## 当前证据路径

```text
Requirement / Provision / Binding
        -> Host 局部验证与解析
        -> Host relation evidence
```

OnlyCore 不包含 init graph、runtime、driver、backend、loader、UI、Audio 或板级装配路径。

表中路径只描述实现组织，不声明所有平台均已验证。

## 证据边界

- module、demo、schema 或脚本存在，只证明对应制品存在。
- Host、QEMU、real board、build-only 与 run evidence 不能互相替代。
- UI、Player、具体 board 和 backend 是消费者或实现压力，不反向定义 Core。
- 完整依赖 DAG、跨平台兼容和产品成熟度不能从本地图推导。

专题入口继续从 [`docs/README.md`](README.md) 进入；提纯规则见
[`project/only_core_distillation_sop.md`](project/only_core_distillation_sop.md)。
