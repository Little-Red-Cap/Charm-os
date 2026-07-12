# Compiler Lifecycle Summary Sidecar v0

## 文档状态

- `status`: `supporting`
- `scope`: lifecycle summary exporter、gate、report 和 wrapper 的现有边界
- `authority`: [`README.md`](README.md)

`compiler_lifecycle.summary.json` 是已有导出结果的只读摘要，不是 world truth、freeze receipt、
archive manifest 或 compare verdict。

## 实现入口

- `scripts/export_compiler_lifecycle_summary.py`
- `scripts/check_compiler_lifecycle_summary.ps1`
- `scripts/report_compiler_lifecycle_summary.ps1`
- `scripts/compiler_lifecycle_summary_sidecar.ps1`
- `scripts/compiler_lifecycle_summary_sidecar_smoke.ps1`
- `scripts/compiler_lifecycle_summary_runtime_evidence_bundle_hook_smoke.ps1`

## 输入

Exporter 只接受显式路径：

- artifact report index 或 case report；
- kernel runtime session summary；
- runtime ledger；
- witness bundle；
- world compare。

路径缺失或 JSON 无法解析会进入 `violations`。Exporter 不扫描源码、build tree 或 raw log 来
推断隐藏事实。

## 输出

- `schema`: `charm.compiler_lifecycle.summary/v0`
- `kind`: `charm.compiler_lifecycle.summary`
- 九个固定状态：`declared`、`materialized`、`proven`、`frozen`、`lowered`、
  `witnessed`、`observed`、`archived`、`compared`
- 每个状态记录 `status`、`coverage_strength`、`projection_kind`、`sidecar_gap`、
  `source_surfaces` 和 notes
- Markdown report 与 check text
- 顶层 result、violations 和 artifact paths

准确字段由 exporter 代码定义；当前没有独立 JSON Schema。

## 诚实性约束

- 没有 freeze receipt 时，`frozen` 必须保持 missing/interpretive。
- Artifact report 只能给 `lowered` 提供解释性投影。
- 没有 archive manifest 时，`archived` 不能被升级为强证明。
- Runtime observation 不修改语义输入。
- Compare source 只投影已有结果，不产生新事实。
- Sidecar 自己的存在不能提高任何状态的 coverage。

Gate 只检查 sidecar shape 和上述约束，不是通用 schema validator。Report consumer 只渲染，
wrapper 只编排 exporter/gate/report。

## 最小验证

```powershell
./scripts/compiler_lifecycle_summary_sidecar_smoke.ps1
./scripts/compiler_lifecycle_summary_runtime_evidence_bundle_hook_smoke.ps1
```

成功只证明 sidecar 投影和 honesty checks 通过，不证明完整 compiler lifecycle 已实现。
