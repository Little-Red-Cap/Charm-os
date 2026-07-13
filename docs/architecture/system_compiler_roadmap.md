# System Compiler 探索路线

> `status`: `exploration`（停线冻结）

System compiler 已被裁决为 `Implementation / Tool`。它不能定义 Charm 身份、Capability Contract、
应用模型或唯一主线；该边界受 [`CONSTITUTION.md`](../../CONSTITUTION.md) 和
[`charm_core_contract.md`](charm_core_contract.md) 约束。

## 保留问题

探索只保留两个问题：

1. 项目输入能否规范化为可审查的 case、subject、fact、contract 和 binding 投影；
2. 构建与运行结果能否形成可比较、可追溯的 artifact 和 evidence，而不只存在于日志中。

现有 graph、bundle、report、runtime/fact sidecar 和 compare 工具证明了部分输入可被导出、查询与比较。
它们不证明“系统已被完整编译”，也不证明输入事实、运行状态或板级资源真实有效。

## 准入条件

新增 compiler 概念、schema 或 sidecar 前，必须同时明确：

- 真实 producer、consumer 和无法复用的现有 artifact；
- 字段来源、兼容策略、正反例与失败行为；
- 静态 graph、runtime observation 和 fact evidence 的独立来源；
- 删除 compiler 术语后，底层 Capability Contract 仍然完整。

缺少上述条件的概念只能留在讨论或 archive。只有手写投影长期稳定且重复劳动明确时，才评估生成器
或更强 IR。

## Deferred

新的系统描述 DSL、全局 World IR、LLVM/MLIR 接入、自动 codegen、完整资源证明、全局确定性时间，
以及把板级、运行时和应用事实合并为单一 canonical graph，均没有足够证据，保持 deferred。

## 入口

- 结果物：[`../system/artifact_report_v0.md`](../system/artifact_report_v0.md)
- inspector：[`../system/explain_surface_v0.md`](../system/explain_surface_v0.md)
- 局部词汇：[`system_compiler_vocabulary_v0.md`](system_compiler_vocabulary_v0.md)
- 当前 compiler 实验：[`../compiler/README.md`](../compiler/README.md)
- 历史取舍：[`../archive/system-compiler-front-page-v0/README.md`](../archive/system-compiler-front-page-v0/README.md)
