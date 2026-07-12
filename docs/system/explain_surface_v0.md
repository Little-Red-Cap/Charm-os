# System Compiler Explain Surface v0

## 文档状态

- `status`: `supporting`
- `scope`: `inspect_system_compiler_artifact_report.ps1` 的现有只读查询面
- `report contract`: [`artifact_report_v0.md`](artifact_report_v0.md)

Explain surface 是 artifact report inspector 的工具接口，不是 Charm Core、公共 App API 或
运行时查询协议。实际参数和行为以脚本与 smoke 为准。

## 输入选择

Inspector 支持三种输入方式：

- `-Report <path>`：读取一份 report；
- `-ArtifactRoot <path>`：读取目录中的全部 `*.artifact_report.json`；
- `-ArtifactRoot <path> -Case <name...>`：选择一个或多个 case。

Report 必须声明 `schema = system_compiler.artifact_report/v0`。引用静态 graph 的查询还会验证
`artifacts.sample_json` 指向的 materialized graph shape。

## 查询参数

| 参数 | 作用 |
|---|---|
| 无查询参数 | 输出 report 或 artifact root 总览 |
| `-ListCases` | 列出可用 case |
| `-CapList` | 列出 capability 及其已知状态 |
| `-WhyCapability <name>` | 汇总 capability 的提供、需求、事实和 blocker |
| `-GraphPath <name>` | 从单份 report 的静态 graph 查找路径 |
| `-ResourceSummary` | 输出资源契约与事实解析摘要 |
| `-RecentTransitions` | 输出单份 report 的近期 runtime transition |
| `-BringupEvidence` | 输出 bring-up 证据摘要 |
| `-ShowArtifacts` | 在默认总览附加 supporting artifact 引用 |
| `-ShowTransitions` | 在默认总览附加 transition 行 |
| `-AsJson` | 输出机器可读查询结果 |

`-ShowArtifacts` 和 `-ShowTransitions` 是总览附录，不是独立查询模型。

## 作用域

| 查询 | 单 report | artifact root | 多 case 子集 |
|---|---:|---:|---:|
| 默认总览 | 支持 | 支持 | 支持 |
| list cases | 支持 | 支持 | 支持 |
| cap list | 支持 | 支持 | 拒绝不明确的多 case 子集 |
| why capability | 支持 | 支持 | 拒绝不明确的多 case 子集 |
| graph path | 支持 | 不支持 | 不支持 |
| recent transitions | 支持 | 不支持 | 不支持 |
| bring-up evidence | 支持 | 支持 | 支持 |
| resource summary | 支持 | 支持 | 支持 |

Root 聚合只汇总 report 已经提供的字段，不跨 case 生成新的 graph path 或 transition 历史。

## 最小操作

```powershell
./scripts/inspect_system_compiler_artifact_report.ps1 `
  -ArtifactRoot out/system-compiler-artifact-report `
  -ListCases

./scripts/inspect_system_compiler_artifact_report.ps1 `
  -ArtifactRoot out/system-compiler-artifact-report `
  -Case materialize-observe-demo `
  -CapList

./scripts/inspect_system_compiler_artifact_report.ps1 `
  -ArtifactRoot out/system-compiler-artifact-report `
  -Case materialize-observe-demo `
  -WhyCapability io.uart1 `
  -AsJson
```

## 解释限制

- `missing`、`unresolved` 或 `violated` 只反映 report 输入与当前投影规则。
- 空 `runtime_observe` 表示没有 sidecar 或没有观察结果，不能解释为运行时不存在。
- `recent_transitions` 是有限快照，不是完整事件历史。
- compare 结果只比较已提供的 baseline/candidate，不证明任一侧真实有效。
- inspector 不修改 report、构建结果、运行时状态或板级资源。
- human 输出用于阅读；自动化消费应优先使用 `-AsJson` 和 schema validation。

## 回归

```powershell
./scripts/system_compiler_explain_surface_contract_smoke.ps1
```

更细的 compare、resource、formation 和 fact-resolution 行为由相应
`materialized_graph_*_smoke.ps1` 覆盖，不在本文复制测试清单。
