# Charm Schemas

## 文档状态

- `status`: `supporting`
- `scope`: `schemas/` 机器可读协议与 sample 路由
- `authority`: 各 `*.schema.json`、producer 与 validator

本页不复制字段定义。Schema 文件定义 wire shape；producer 定义字段来源；validator 定义当前
可执行校验。三者缺一时，文件存在不能证明协议已闭环或运行事实成立。

## 分组

| 文件前缀 | 范围 | 主要入口 |
|---|---|---|
| `materialized_graph.*` | case manifest、graph export bundle、diff、CI 与 report manifest | `scripts/validate_materialized_graph_artifacts.py` |
| `minimal_kernel.*` | runtime evidence、session witness 与 inspect/compare artifact | `docs/system/minimal_kernel_runtime_evidence_bundle_contract.md` |
| `system_compiler.artifact_report*` | case report 与 root index | `docs/system/artifact_report_v0.md` |
| `system_compiler.fact_evidence*`、`runtime_observe*` | fact/runtime sidecar | artifact exporter 与 validator |
| `system_compiler.front_page*`、`biography*` | 已停线工具链的局部 artifact | 同名 exporter/validator/smoke |
| `system_compiler.canonical_world*`、`witness_bundle*`、`world_compare*`、`world_shelf_review*` | witness/compare 工具 artifact | 对应 `scripts/validate_system_compiler_*.py` |
| `*_summary*`、`system_compiler_result_map*` | report/inspector 的 summary 与 comparison 投影 | producer script 与注册 validator |

准确支持范围以 validator 内的 schema registry 或 dedicated validator 为准。通用 materialized-graph
validator 未注册的 schema 不能因为位于本目录就由它自动校验。

## Samples

`schemas/examples/*.json` 是测试 fixture，不是运行证据、默认配置或产品 manifest。Sample 可以固定
正例 shape、静态引用和 consumer 输入，但不能证明真实 producer 已运行。

卫生检查：

```powershell
./scripts/schema_examples_hygiene_smoke.ps1
```

该 smoke 只检查 JSON 可解析、仓库静态引用存在且使用 forward slash；它不执行 JSON Schema
validation、compare 或 CI verdict。

## 增改规则

- 修改公开 shape 时同步 producer、validator、sample 和真实 consumer；
- 新字段必须有来源、消费方、失败语义与兼容策略，不能只在 README 中解释；
- projection 不重新判定上游 verdict，compare 不伪造缺失 baseline/candidate；
- `v0/v1`、schema 数量和 sample 数量都不授予 Core、稳定 ABI 或实现成熟度；
- 优先复用已有 envelope/definition，避免为每个 route、workspace 或阶段旁白新增 schema。

Schema 准入与缩面规则见
[`evidence_surface_governance_v0.md`](../docs/system/evidence_surface_governance_v0.md)。
