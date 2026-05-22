# Schema Surface Reduction Inventory v0

## 定位

这份清单是 [`schema_surface_reduction_governance_v0.md`](schema_surface_reduction_governance_v0.md) 的盘点侧车。

它只记录当前 schema 面的风险画像、重复结构候选和后续收敛优先级，不新增 schema、validator、exporter、smoke 或 compare brain。

## 盘点口径

- 只统计 Git 已跟踪的 `schemas/` 下 JSON 文件。
- 不统计当前隔离的未跟踪 `world_shelf_review` schema 文件。
- 不统计 `Examples/`、build output、`out/`、第三方工具或运行证据输出。
- 统计用于治理排序，不作为 CI gate。

## 当前基线

| 指标 | 数值 |
| --- | ---: |
| schema JSON 文件 | 89 |
| schema 总行数 | 45986 |
| 最大 schema | `schemas/system_compiler_summary.v0.schema.json` |
| 最大 schema 行数 | 2513 |

## Top Families

| Family | Files | Lines | Risk | Decision |
| --- | ---: | ---: | --- | --- |
| `system_compiler` | 39 | 21175 | structural | 先登记风险；后续治理 artifact envelope / report schema |
| `front_page.opening_flow` | 13 | 8222 | pilot | 第一批 shared definitions pilot 候选 |
| `minimal_kernel` | 10 | 4842 | medium | 暂不先动，避免影响 runtime evidence/session 证据链 |
| `front_page.opening_testimony` | 6 | 2943 | medium | 跟随 opening-flow pilot 之后再收 |
| `materialized_graph` | 6 | 2008 | medium | 后续可与 artifact report envelope 联合治理 |
| `fact_resolution_summary` | 3 | 1895 | medium | 非第一批 |
| `system_input_summary` | 3 | 1450 | medium | 非第一批 |
| `system_formation_summary` | 3 | 1315 | medium | 非第一批 |
| `bringup_order_summary` | 3 | 1200 | medium | 非第一批 |
| `binding_result_summary` | 3 | 936 | medium | 非第一批 |

## Largest Schemas

| Lines | Path | Note |
| ---: | --- | --- |
| 2513 | `schemas/system_compiler_summary.v0.schema.json` | root system compiler summary envelope 最大 |
| 2477 | `schemas/system_compiler.artifact_report.v0.schema.json` | artifact report 结构性风险，后续应与 report decomposition 对齐 |
| 1254 | `schemas/system_compiler.front_page_entry_landing_compare.v0.schema.json` | front-page compare envelope 重复候选 |
| 1244 | `schemas/fact_resolution_summary.v0.schema.json` | summary 族群 envelope 重复候选 |
| 1200 | `schemas/examples/system_compiler_summary.summary.v0.sample.json` | sample 体量较大，但本刀不动 samples |
| 1145 | `schemas/examples/system_compiler_summary.comparison.v0.sample.json` | compare sample 体量较大，本刀只登记 |
| 1093 | `schemas/minimal_kernel.runtime_evidence_bundle.summary.v1.schema.json` | minimal-kernel evidence anchor，暂不作为第一批 pilot |
| 997 | `schemas/examples/minimal_kernel.runtime_session_witness_inspect_compare_consumer.v0.sample.json` | runtime-session sample，暂不作为第一批 pilot |
| 962 | `schemas/system_compiler.front_page_entry_opening_flow_open_event.v0.schema.json` | opening-flow pilot 候选 |
| 940 | `schemas/system_formation_summary.v0.schema.json` | summary envelope 重复候选 |

## Shared Definition Candidates

只读扫描显示，以下字段在 schema 面重复出现，适合先登记为 shared definition candidate：

| Candidate | Files | Hits | Why it matters |
| --- | ---: | ---: | --- |
| `kind` | 78 | 329 | artifact identity 的共同字段 |
| `summary_path` | 39 | 208 | summary/report/check 路径族群可共用 shape |
| `result` | 45 | 162 | smoke/export/validator result 语义需要统一 vocabulary |
| `schema` | 68 | 131 | artifact schema identity 的共同字段 |
| `status` | 33 | 129 | ready/blocked/standing/collapsed 等状态应回指 contract |
| `surface_id` | 8 | 22 | front-page / route surface 共同候选 |
| `verdict` | 7 | 16 | compare verdict 不得只由字段名解释 |
| `artifact_ref` | 3 | 15 | opening / witness artifact target 共同候选 |
| `evidence_refs` | 2 | 4 | witness evidence projection 候选 |
| `selected_surface` | 2 | 3 | route / explain selection 共同候选 |

这些候选在 v0 不抽成 `$defs`。下一步只允许在保持现有 JSON shape 的前提下做 schema 内部复用。

## 第一批 Pilot 建议

第一批 schema 收敛建议选择 `front_page.opening_flow`，原因是：

- 它与刚完成的 opening-flow script harness pilot 对齐。
- 族群内 `open_event / open_event_witness / compare / workspace` 重复结构明显。
- 它主要是上层 artifact/projection/compare schema，风险低于 minimal-kernel runtime evidence chain。

第一批只允许：

- 抽 root identity、path ref、status/result、surface/ref 这类低语义 shared defs。
- 保持 artifact JSON 输出完全兼容。
- 保持 validator、exporter、smoke 入口不变。

第一批禁止：

- 不改 compare verdict。
- 不改 opening judgment / selection policy。
- 不改 route / explain / handoff 语义。
- 不新增 schema kind。
- 不把 `world_shelf_review` 未跟踪 schema 纳入本轮治理。

## 为什么不先动 minimal-kernel schema

`minimal_kernel` schema 族群当前支撑 runtime evidence bundle、kernel runtime session、witness 和 compare 证据链。

第一批不先动它，原因是：

- 它是运行证据与 session witness 的下层证明面。
- 近期刚完成 `arch_ingress_seam` 到 session 的投影，不宜同时改 schema 复用结构。
- opening-flow schema 更适合验证“抽 shared defs、不改语义”的方法。

## 验收边界

本 inventory v0 完成时应满足：

- 只新增文档和索引，不修改 schema 文件。
- `Examples/` 不发生任何变更。
- 当前隔离的未跟踪 `world_shelf_review` 文件不纳入统计基线。
- 后续 schema pilot 有明确候选，但本刀不执行 pilot。
