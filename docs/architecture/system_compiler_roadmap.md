# System Compiler 探索路线

## 文档状态

- `status`: `exploration`（停线冻结）
- `scope`: system compiler 工具假设、已有证据与后续验证条件
- `authority`: 受 [`CONSTITUTION.md`](../../CONSTITUTION.md) 和
  [`charm_core_contract.md`](charm_core_contract.md) 约束

Compiler 已被裁决为 `Implementation / Tool`。本路线不能定义 Charm 身份、Capability
Contract 或应用模型。

## 保留的假设

System compiler 探索尝试验证两点：

1. 项目输入能否被规范化为可审查的 case、subject、fact、contract 和 binding 投影；
2. 构建与运行结果能否形成可比较、可追溯的 artifact 和 evidence，而不只存在于日志中。

这两点值得继续验证，但不要求存在一个统一 DSL、全局 World IR 或自动 codegen。

## 当前已有实现

| 能力 | 当前载体 | 能证明什么 |
|---|---|---|
| 静态装配投影 | `init.graph`、`init.materialize`、`init.observe` | 部分装配关系可导出 |
| export bundle | `export_materialized_graph.ps1` 与 bundle index | 多种 case 可进入统一工具入口 |
| artifact report | schema、exporter、inspector | 输入与结果可形成只读报告 |
| runtime observe | runtime sidecar | 部分动态状态可与静态结果并列观察 |
| fact evidence | fact sidecar、resource/fact resolution | 部分事实声明和证据可被审计 |
| compare | diff/report smokes | 已投影字段可以比较 |
| lifecycle summary | compiler lifecycle sidecar 脚本 | 历史 lifecycle 语言有一个只读投影 |

这些实现是工具证据，不证明“系统已被完整编译”，也不自动证明输入事实真实。

## 当前入口

- 结果物边界：[`../system/artifact_report_v0.md`](../system/artifact_report_v0.md)
- inspector 边界：[`../system/explain_surface_v0.md`](../system/explain_surface_v0.md)
- 局部词汇：[`system_compiler_vocabulary_v0.md`](system_compiler_vocabulary_v0.md)
- compiler 历史文档：[`../compiler/README.md`](../compiler/README.md)
- front-page 历史摘要：
  [`../archive/system-compiler-front-page-v0/README.md`](../archive/system-compiler-front-page-v0/README.md)

## 继续推进的准入条件

新增 compiler 概念、schema 或 sidecar 前必须回答：

1. 哪个真实 producer 产生它；
2. 哪个 consumer 需要它；
3. 现有 artifact 为什么不能表达；
4. 正例、反例和失败行为如何验证；
5. 它是否只复制了已有字段；
6. 删除 compiler 术语后，底层 Capability Contract 是否仍完整。

没有 producer、consumer 和 smoke 的概念只能留在 exploration，不进入默认路由。

## 建议验证顺序

1. 先稳定现有 bundle、report 和 inspector，减少重复 schema 与脚本入口。
2. 用两个不同 case 验证同一字段是否保持相同含义。
3. 分开静态 graph、runtime observation 与 fact evidence，禁止伪造缺失输入。
4. 对 compare 结果保留来源和缺失信息。
5. 只有手写投影长期稳定且重复劳动明确时，才评估生成器或更强 IR。

## 明确不成立的结论

- System compiler 不是 Charm Core，也不是 Charm 的唯一主线。
- Artifact report 不是系统真相，只是输入和工具规则的投影。
- Graph 不代表运行时、资源所有权和产品配置的统一世界。
- Schema 数量、文档数量和 smoke 数量不能证明架构成熟。
- `SystemSpec`、`BoardPackage`、`World`、`Witness` 等名称不因出现在旧文档中获得公共身份。

## Deferred

以下方向没有足够证据，保持 deferred：

- 新的系统描述 DSL；
- 全局 World IR 或 LLVM/MLIR 接入；
- 从 C++ 元数据自动推导完整资源契约；
- 自动 codegen、完整资源证明和全局确定性时间；
- 将所有板级、运行时和应用事实合并为单一 canonical graph。

历史路线中的“三个月、一年、传奇路线”等时间叙事已删除。仍有独立价值的取舍见
[`../archive/system-compiler-front-page-v0/README.md`](../archive/system-compiler-front-page-v0/README.md)。
