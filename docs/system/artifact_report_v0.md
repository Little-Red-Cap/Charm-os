# System Compiler Artifact Report v0

## 文档状态

- `status`: `supporting`
- `scope`: artifact report 的输入种类、投影边界与验证入口
- `authority`: [`CONSTITUTION.md`](../../CONSTITUTION.md) 与
  [`charm_core_contract.md`](../architecture/charm_core_contract.md)

Artifact report 是 system compiler 探索工具的结果物，不是 Charm Core、构建状态或运行事实来源。

## 事实源

| 内容 | 入口 |
|---|---|
| case report shape | [`system_compiler.artifact_report.v0.schema.json`](../../schemas/system_compiler.artifact_report.v0.schema.json) |
| root index shape | [`system_compiler.artifact_report_index.v0.schema.json`](../../schemas/system_compiler.artifact_report_index.v0.schema.json) |
| 导出 | [`export_system_compiler_artifact_report.ps1`](../../scripts/export_system_compiler_artifact_report.ps1) |
| 查询 | [`inspect_system_compiler_artifact_report.ps1`](../../scripts/inspect_system_compiler_artifact_report.ps1) |
| schema validation | [`validate_materialized_graph_artifacts.py`](../../scripts/validate_materialized_graph_artifacts.py) |

字段、required 关系、CLI 参数与默认路径只由这些入口定义，本文不复制。

## 输入与输出

Exporter 消费 `materialized_graph.export_bundle/v1` index。case kind 决定允许的输入：

| `case_kind` | 输入边界 |
|---|---|
| `materialized_graph` | graph、runtime sidecar 与 fact evidence |
| `runtime_only` | runtime observe sidecar，不伪造静态 graph |
| `fact_only` | declared/required/provided facts，不伪造 graph 或 runtime sidecar |

缺少 `case_kind` 时按 `materialized_graph` 处理；其它值被拒绝。每个 case 生成一份 report；
`-OutputRoot` 另外生成 root index，`-OutputPath` 只生成单份 report。Compare mode 必须取得显式
baseline/candidate 输入，exporter 不推断缺失 baseline。

Profile、Board、Facet 和 summary/diff override 只改变工具投影，不建立新的 Core 对象。

## 投影不变量

- `declared`、`materialized`、`published`、`observed`、`blocked`、`failed` 是 report 状态，不能直接
  解释为设备可用或结果正确。
- evidence 保留来源与失败原因；Host、QEMU 和真实板不能互相替代。
- resource/fact 内容只投影输入，不参与 binding、init 或运行时执法。
- root index 只索引 case report，不重新解释 case 内容。
- report 成功只证明输入可读取和投影，不证明系统可构建、bring-up 成功或 Capability Contract 已满足。

## 失败边界

Exporter、validator 或 inspector 拒绝不存在的路径/case、不支持的 schema 或 `case_kind`、无效 graph
shape，以及需要单 case 却得到不明确集合的查询。具体错误文本属于实现，不进入 report schema。

## 验证

基础导出与查询由
[`system_compiler_explain_surface_contract_smoke.ps1`](../../scripts/system_compiler_explain_surface_contract_smoke.ps1)
覆盖；compare/resource/fact 变体由对应 `materialized_graph_*_smoke.ps1` 维护。Inspector 语义见
[`explain_surface_v0.md`](explain_surface_v0.md)，历史路线只在
[`system_compiler_roadmap.md`](../architecture/system_compiler_roadmap.md) 的 exploration 范围内成立。
