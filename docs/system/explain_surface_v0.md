# System Compiler Explain Surface v0

## 文档状态

- `status`: `supporting`
- `scope`: artifact report inspector 的只读查询边界
- `source`: [`inspect_system_compiler_artifact_report.ps1`](../../scripts/inspect_system_compiler_artifact_report.ps1)
- `report contract`: [`artifact_report_v0.md`](artifact_report_v0.md)

Explain surface 是 host 工具接口，不是 Charm Core、App API 或运行时查询协议。CLI 参数、默认值与
错误文本以脚本为准。

## 输入选择

Inspector 可读取单份 report、artifact root 下的全部 report，或 root 中按 case 选择的子集。
Report schema 必须是 `system_compiler.artifact_report/v0`；graph 查询还要求 supporting graph 存在且
通过 materialized graph shape 检查。

## 查询范围

| 查询 | 单 report | artifact root | 多 case 子集 |
|---|---:|---:|---:|
| 总览 / list cases | 支持 | 支持 | 支持 |
| capability list / why capability | 支持 | 支持 | 拒绝不明确集合 |
| graph path | 支持 | 不支持 | 不支持 |
| recent transitions | 支持 | 不支持 | 不支持 |
| bring-up evidence / resource summary | 支持 | 支持 | 支持 |

Root 查询只聚合 report 已有字段，不跨 case 合成 graph path、transition history 或新 verdict。
Artifacts/transitions 的 show switches 只扩展总览，不建立独立查询模型。

## 输出与解释

- `missing`、`unresolved`、`violated` 只反映输入与投影规则。
- 空 runtime observe 不等于运行时不存在。
- recent transitions 是有限快照，不是完整事件历史。
- compare 只比较已提供的 baseline/candidate，不证明任一侧有效。
- inspector 不修改 report、构建结果、运行时或板级资源。
- human output 用于阅读；自动化消费使用 JSON 输出并执行 schema validation。

## 失败边界

不存在的 report/root/case、错误 schema、缺失 supporting graph、无效 graph shape，以及要求单 case
却得到不明确集合的查询均失败。失败不得生成替代事实或修改源 artifact。

## 验证

[`system_compiler_explain_surface_contract_smoke.ps1`](../../scripts/system_compiler_explain_surface_contract_smoke.ps1)
验证输入选择、查询组合、JSON 输出与失败路径；其它 compare 语义由其 producer/consumer smoke 负责。
