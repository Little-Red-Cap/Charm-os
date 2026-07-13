# System Compiler Artifact Report v0

## 文档状态

- `status`: `supporting`
- `scope`: artifact report 的现有 schema、生成入口与验证边界
- `authority`: 受 [`CONSTITUTION.md`](../../CONSTITUTION.md) 和
  [`charm_core_contract.md`](../architecture/charm_core_contract.md) 约束

Artifact report 是 system compiler 探索线的工具结果物，不是 Charm Core 模型，也不是构建、
运行或部署的事实来源。字段定义以 schema 和生成脚本为准，本文只提供稳定入口。

## 实现入口

- case report schema:
  [`system_compiler.artifact_report.v0.schema.json`](../../schemas/system_compiler.artifact_report.v0.schema.json)
- root index schema:
  [`system_compiler.artifact_report_index.v0.schema.json`](../../schemas/system_compiler.artifact_report_index.v0.schema.json)
- exporter:
  [`export_system_compiler_artifact_report.ps1`](../../scripts/export_system_compiler_artifact_report.ps1)
- inspector:
  [`inspect_system_compiler_artifact_report.ps1`](../../scripts/inspect_system_compiler_artifact_report.ps1)
- validator:
  [`validate_materialized_graph_artifacts.py`](../../scripts/validate_materialized_graph_artifacts.py)

## 输入

Exporter 消费 `materialized_graph.export_bundle/v1` 的 `index.json`。case 允许三种形态：

| `case_kind` | 可提供的主要输入 | 不应伪造 |
|---|---|---|
| `materialized_graph` | graph JSON/DOT、runtime sidecar、fact evidence | 无 |
| `runtime_only` | runtime observe sidecar | 静态 graph |
| `fact_only` | declared/required/provided facts、fact evidence | graph 和 runtime sidecar |

缺少 `case_kind` 时 exporter 按 `materialized_graph` 处理；其它值会被拒绝。

可选输入包括：

- `Profile`、`Board`、`Facet` override；
- CI summary、report manifest 和 diff JSON；
- `export_only` 或 `compare` 模式；
- bundle case 中的 subject、facts、contracts 和 sidecar 引用。

这些输入是工具投影，不建立新的 `SystemSpec`、`Profile` 或 `BoardPackage` Core 语义。

## 输出

每个 case 生成 `*.artifact_report.json`，schema 要求以下顶层分组：

| 分组 | 内容 |
|---|---|
| identity | `schema`、时间、generator、report kind、mode |
| `subject` / `system_input` | case 身份和解析后的输入投影 |
| `structure` | capability、fact 和 materialized structure 摘要 |
| `binding_result` | binding 条目与未解析结果 |
| `bringup_order` / `system_formation` | 顺序、形成状态与 blocker |
| `bringup_evidence` | bring-up 证据摘要 |
| `resource_contract` / `fact_resolution` | 声明、事实库存和解析结果 |
| `runtime_observe` | observed capability、publish/export 状态和近期 transition |
| `artifacts` | 输入与 supporting artifact 引用 |

`comparison` 和 `connection_summary` 是条件字段。准确字段、枚举和 required 关系必须查看 schema，
不要从本文复制一份平行定义。

### 投影语义

- `declared`、`materialized`、`published`、`observed`、`blocked` 和 `failed` 是报告状态；
  `published` 不表示设备可用，`observed` 也不表示结果正确。
- `bringup_evidence` 必须保留来源与失败原因。Host fixture、QEMU 和真实板证据不能互相替代。
- `resource_contract` 只投影声明、事实和违规结果，不是统一 C++ API，也不参与 binding、init
  或运行时执法。blocking、heap、IRQ 和 clock 限制仍由具体模块契约负责。

使用 `-OutputRoot` 导出时还会生成 `index.json`，用于列出 case、路径和 compiler headline。
使用 `-OutputPath` 导出单份 report 时不生成 root index。

## 最小操作

```powershell
./scripts/export_materialized_graph.ps1 `
  -Case materialize-observe-demo `
  -OutputRoot out/materialized-graph-bundle

./scripts/export_system_compiler_artifact_report.ps1 `
  -BundleRoot out/materialized-graph-bundle `
  -Case materialize-observe-demo `
  -OutputRoot out/system-compiler-artifact-report

python ./scripts/validate_materialized_graph_artifacts.py `
  ./out/system-compiler-artifact-report/materialize-observe-demo.artifact_report.json

./scripts/inspect_system_compiler_artifact_report.ps1 `
  -ArtifactRoot out/system-compiler-artifact-report
```

Compare 模式必须显式提供可用的 diff/summary 输入；exporter 不应凭空推断 baseline。

## 失败边界

Exporter 或 inspector 会拒绝：

- bundle/index/report 路径不存在；
- bundle 或 report schema 不匹配；
- 未支持的 `case_kind`；
- 请求不存在的 case；
- 需要单 report 的查询被用于不明确的多 case 集合；
- supporting graph 路径不存在或 graph shape 无效。

报告生成成功只说明工具能读取并投影输入。它不证明输入事实真实、系统可构建、板级 bring-up
成功、Capability Contract 已满足，或资源约束已被运行时执行。

## 回归入口

- [`system_compiler_explain_surface_contract_smoke.ps1`](../../scripts/system_compiler_explain_surface_contract_smoke.ps1)
- [`materialized_graph_system_formation_compare_smoke.ps1`](../../scripts/materialized_graph_system_formation_compare_smoke.ps1)
- [`materialized_graph_resource_contract_compare_smoke.ps1`](../../scripts/materialized_graph_resource_contract_compare_smoke.ps1)
- [`materialized_graph_required_fact_resolution_compare_smoke.ps1`](../../scripts/materialized_graph_required_fact_resolution_compare_smoke.ps1)

Inspector 查询范围见 [`explain_surface_v0.md`](explain_surface_v0.md)。历史路线和词汇只在
[`system_compiler_roadmap.md`](../architecture/system_compiler_roadmap.md) 与
[`system_compiler_vocabulary_v0.md`](../architecture/system_compiler_vocabulary_v0.md) 的 exploration
范围内成立。
