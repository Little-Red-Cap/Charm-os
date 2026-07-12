# Script Surface Reduction Inventory v0

> 状态：archived。本文是一次工作区脚本数量与 pilot 候选快照，不是当前事实或 CI gate。

## 定位

这份清单是 `docs/system/script_surface_reduction_governance_v0.md` 的盘点侧车。

它只记录当前脚本面的风险画像、优先级和第一批收敛候选，不新增 schema、脚本、validator、smoke 或 compare brain。

## 盘点口径

- 只统计 Git 已跟踪的 `scripts/` 下 `.ps1` 与 `.py` 文件。
- 不统计当前隔离的未跟踪 `world_shelf_review` 文件。
- 不统计 build output、`out/`、第三方工具或历史归档外部材料。
- 统计用于治理排序，不作为 CI gate。

## 当前基线

| 指标 | 数值 |
| --- | --- |
| 脚本文件总数 | 259 |
| PowerShell 文件 | 179 |
| Python 文件 | 80 |
| 脚本总行数 | 102018 |
| `>= 500` 行脚本 | 54 |
| `>= 1000` 行脚本 | 9 |
| 最大脚本 | `scripts/inspect_system_compiler_artifact_report.ps1` |
| 最大脚本行数 | 10063 |
| system-compiler/front-page/witness/biography/world 相关脚本 | 194 |
| system-compiler/front-page/witness/biography/world 相关行数 | 68474 |

## Top Families

| Family | Files | Lines | Risk | Decision |
| --- | ---: | ---: | --- | --- |
| `system_compiler_artifact` | 2 | 14335 | structural | 只登记风险；不作为第一刀拆分对象 |
| `materialized_graph` | 4 | 3059 | medium | 后续可抽公共 bundle/report helper |
| `system_compiler_front_page_entry_opening_flow_open_event` | 7 | 2955 | pilot | 第一批治理候选 |
| `system_compiler_front_page_entry_opening_flow_open_event_witness` | 6 | 2537 | pilot | 第一批治理候选 |
| `system_compiler_front_page_entry_opening_flow_consumer_plan_action` | 7 | 2408 | high | 暂不先动语义层，后续跟进 |
| `system_compiler_front_page_entry_opening_flow_consumer_selector` | 7 | 2280 | high | 暂不先动语义层，后续跟进 |
| `system_compiler_biography_index` | 5 | 2261 | medium | 非第一批 |
| `system_compiler_front_page_entry_opening_flow_consumer_plan` | 7 | 2230 | high | 暂不先动语义层，后续跟进 |
| `system_compiler_front_page_entry_opener` | 7 | 2183 | pilot-adjacent | 可借鉴 pilot helper |
| `system_compiler_front_page_entry_opening_testimony_explain_entry` | 5 | 1951 | medium | 非第一批 |
| `system_compiler_front_page_entry_opening_testimony_explain_entry_handoff` | 5 | 1885 | medium | 非第一批 |
| `system_compiler_front_page_entry_opening_testimony_landing` | 5 | 1803 | medium | 非第一批 |

## 第一批 Pilot

第一批治理目标锁定 opening-flow / open-event / open-event-witness 的 smoke 与 workspace wrapper 层。

优先候选：

- `system_compiler_front_page_entry_opening_flow_open_event_smoke.ps1`
- `system_compiler_front_page_entry_opening_flow_open_event_workspace_smoke.ps1`
- `system_compiler_front_page_entry_opening_flow_open_event_witness_smoke.ps1`
- `system_compiler_front_page_entry_opening_flow_open_event_witness_workspace_compare_smoke.ps1`
- `compare_system_compiler_front_page_entry_opening_flow_open_event_workspace.ps1`
- `compare_system_compiler_front_page_entry_opening_flow_open_event_witness_workspace.ps1`

允许的改动：

- 新增一个小型 shared harness helper。
- 抽公共路径解析、进程调用、JSON 加载、validator 调用、基础断言。
- 保持 PowerShell 作为人工/CI 入口。

## Fixture Runway Stabilization

在正式抽取 shared harness helper 之前，front-page / opening-flow smoke 需要先稳定一条可复用输入跑道。

本批 `Front-Page Opening-Flow Smoke Fixture Stabilization v0` 只允许作为 harness 地基修复：

- 当默认 front-page workspace 缺失时，生成或定位通用 fixture workspace。
- fixture 只复用既有 exporter、validator、review 和 route 入口。
- sample 中缺失的 report/check/doc ref 只能落到 fixture-local 文件或真实存在的 contract 文档。
- 不新增 artifact kind、schema、compare verdict、opening policy 或 selected-surface 语义。
- 不把 fixture bootstrap 发展成新的语义脚本家族。

禁止的改动：

- 不改 schema。
- 不改 summary 文件名和输出目录约定。
- 不改 exit code 语义。
- 不改 compare verdict。
- 不改 selection policy。
- 不改 opening judgment / route / explain / handoff 判决。

## Opening-Flow Open-Event Harness Pilot

本批 `Opening-Flow Open-Event Harness Pilot` 是第一刀脚本面收敛试点。
它只把 open-event / open-event-witness 相关 smoke 与 workspace compare wrapper 中重复的 PowerShell 编排收进 `front_page_entry_opening_flow_harness.ps1`：

- 工具解析：`Resolve-PythonExe`、`Resolve-PowerShellExe`。
- 路径与输出根：`Assert-RequiredPaths`、`Initialize-SmokeOutputRoot`、`Assert-CleanPath`。
- 子脚本调用：`Invoke-PowerShellScript`。
- plan-action workspace / compare bootstrap：`Ensure-OpeningFlowConsumerPlanActionWorkspaceSmoke`、`Ensure-OpeningFlowConsumerPlanActionCompareSmoke`。
- summary path resolver：open-event 与 open-event-witness workspace summary 定位。

本 pilot 的边界：

- 不新增 artifact kind。
- 不新增 schema、validator、compare verdict 或 opening policy。
- 不改变 open-event / open-event-witness JSON 字段形状。
- 不把 compare / selection / opening judgment 语义迁入 PowerShell helper。
- 不触碰 `Examples/`。
- 不纳入当前隔离的未跟踪 `world_shelf_review` 文件。

伴随修复：

- `consumer_plan_action_compare_smoke` 的 fixture 断言对齐当前 compare summary：`changed_field_count=30`，并承认 projection summary / question drift。
- `open_event_witness_workspace_compare_smoke` 可复用真实 open-event smoke summary；fixture 缺失时不再把 `_fixture-open-events` 作为唯一输入来源。

## 为什么不先拆最大脚本

`scripts/inspect_system_compiler_artifact_report.ps1` 是当前最大结构性风险，但它覆盖大量 artifact report query、display、aggregation 和 comparison view。

第一刀不拆它，原因是：

- 影响面大，容易把治理试刀变成行为迁移。
- 当前目标是阻止脚本家族继续增殖，而不是一次性修复所有历史债。
- opening-flow/front-page wrapper 更适合验证“抽公共 harness、不动语义”的方法。

后续应单独规划 `Artifact Report Decomposition v0`，专门处理该脚本和 `export_system_compiler_artifact_report.ps1`。

## 验收边界

第一批 pilot 完成时应满足：

- touched smoke 的输出路径、summary 名称、exit code、validator 调用保持兼容。
- 行为不变，重复 PowerShell 编排减少。
- 不新增 artifact kind。
- 不新增 compare brain。
- 不触碰 ARMv7-A QEMU lower-half smoke。
- 不纳入当前隔离的未跟踪 `world_shelf_review` 文件。
