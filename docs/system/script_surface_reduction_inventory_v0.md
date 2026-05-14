# Script Surface Reduction Inventory v0

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

禁止的改动：

- 不改 schema。
- 不改 summary 文件名和输出目录约定。
- 不改 exit code 语义。
- 不改 compare verdict。
- 不改 selection policy。
- 不改 opening judgment / route / explain / handoff 判决。

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
