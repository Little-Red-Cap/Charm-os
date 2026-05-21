# Compiler World Lifecycle Projection v0

本文是 `Compiler World Lifecycle v0` 的只读投影合同。

它回答的是：现有 artifact、report、witness、ledger、compare 产物如何用 lifecycle 语言自我描述。它不创建新 truth，不定义新的 machine schema，不新增 exporter、validator、smoke、IR 类型、canonical identity 或 observation import pass。
投影覆盖强弱、直接/解释性投影与未来 sidecar 缺口由 [`compiler_world_lifecycle_projection_coverage_v0.md`](compiler_world_lifecycle_projection_coverage_v0.md) 审计。

## 1. 定位

`Compiler World Lifecycle v0` 定义 world lifecycle law。

本文只定义现有产物到 lifecycle states 的 projection：

```text
existing artifacts / reports / witnesses / ledgers / compares
  -> lifecycle state projection
```

Projection 的职责是说明：

- 哪些 artifact 支撑哪些 lifecycle states。
- 哪些 lifecycle states 当前没有直接投影。
- 哪些 local states 只属于某条 evidence pipeline，不应被强行升格。

Projection 不得：

- 解析 raw logs。
- 重跑 lower brain。
- 修改 verdict。
- 创建 source facts。
- 生成 new semantic truth。
- 替代 artifact report、runtime ledger、witness bundle 或 world compare 的原有判决模型。

## 2. v0 Projection Map

v0 固定六类现有对象到 lifecycle states 的映射：

| Existing object | Lifecycle states | Projection meaning |
| --- | --- | --- |
| `artifact_report` | `declared`, `materialized`, `lowered` | system input、binding result、bringup order 与 report surface 的只读解释面 |
| `bringup_evidence_pipeline` | `declared`, `materialized`, `observed` | bringup facts 从声明、物化到稳定观察的 evidence projection |
| `kernel_runtime_session.summary.json` | `proven`, `witnessed` | session verdict 与既有 witness entry 支撑的 runtime session proof projection |
| `runtime_ledger.json` | `observed` | exporter 已消费 summary facts 的顺序观察，不替代 session verdict |
| `witness_bundle` | `witnessed`, `archived` | paired artifact 的证词承载与可复盘交付入口 |
| `world_compare` | `compared` | exported witness/surfaces 的比较结果，不重建 truth |

这个表不是 schema。它是文档级 projection law。

## 3. `artifact_report`

`artifact_report` 投影：

- `declared`
- `materialized`
- `lowered`

它可以说明：

- system input 是什么。
- binding result 如何被解释。
- bringup order 如何进入 report surface。
- artifact report 作为 lowered explanation surface 如何出现。

它不能说明：

- legality proof 已经完整成立。
- runtime observation 已经发生。
- witness 已经回指到 world facts。
- compare 已经证明世界 standing / drifted / collapsed。

## 4. `bringup_evidence_pipeline`

`bringup_evidence_pipeline` 投影：

- `declared`
- `materialized`
- `observed`

其中 `published / failed / blocked` 保留为 bringup-local evidence states。它们当前不强行升格为 world lifecycle primary states。

这条 projection 不改变 `bringup_evidence_pipeline_v0.md` 的原有语义。它只说明 bringup evidence 是 lifecycle 的现有胚胎之一。

## 5. `kernel_runtime_session.summary.json`

`kernel_runtime_session.summary.json` 投影：

- `proven`
- `witnessed`

`proven` 以 session verdict、runtime facts、machine witness、semantic witness 与既有 session exporter 规则为准。

`witnessed` 以 system compiler witness bundle 中的 `kernel_runtime_session` witness entry 为准。

Projection 不得：

- 用 runtime ledger 替代 session verdict。
- 重新解析 QEMU / host / session raw logs。
- 重建 session failure taxonomy。
- 让上层重新判断 runtime/session/world compare 原始证据。

## 6. `runtime_ledger.json`

`runtime_ledger.json` 投影：

- `observed`

它只表示 session exporter 已消费的 summary facts 的顺序观察。

它不能：

- 替代 `kernel_runtime_session.summary.json` 的 verdict。
- 新增 compare verdict、drift rule 或 collapse rule。
- 成为 raw log parser。
- 成为 scheduler trace 或 runtime profiler。

## 7. `witness_bundle`

`witness_bundle` 投影：

- `witnessed`
- `archived`

`witnessed` 表示 canonical world 的 witness entries 已作为证词出口。

`archived` 表示 witness bundle 可以作为 paired artifact 的一部分保存，用于复盘交付事实。

这不表示 witness bundle 拥有修改 world truth 的权限。它仍然只能消费已导出的 artifacts 和 witness plan。

## 8. `world_compare`

`world_compare` 投影：

- `compared`

它只比较 exported witness bundles 或合法 surfaces。

它不得：

- 重跑 builder / deriver / verifier。
- 读取 raw runtime/session/world evidence 来绕过 witness。
- 创建新的 semantic truth。
- 修复 drift 或 collapse。

当前 `world_compare` 文档在归档目录中，但其 compare 语义仍可作为已有 lifecycle projection 对象引用。本刀不恢复旧 front-page 大索引。

## 9. Missing Projection

某些 lifecycle states 在 v0 中可能没有直接 artifact projection。

这不是错误。Projection 可以显式说明：

- state exists in lifecycle law
- current evidence world has no direct projection
- future sidecar may add projection

尤其是 `frozen`：当前 v0 主要由 constitution 与 freeze boundary law 定义，不要求现有 artifact 已经导出独立 frozen marker。

## 10. Future Sidecar Direction

未来可以考虑导出只读 sidecar，例如：

```text
compiler_lifecycle.summary.json
```

但本刀不定义该文件、schema、字段、validator 或 exporter。

如果未来实现 sidecar，它必须仍然遵守：

- read-only projection
- no raw log parsing
- no verdict mutation
- no new source facts
- no canonical identity definition by accident

## 11. 非目标

本 v0 不做：

- 不新增 JSON schema。
- 不新增 summary 文件。
- 不新增 validator、exporter、smoke 或脚本。
- 不新增 C++ 类型或 IR。
- 不定义 canonical identity。
- 不实现 observation import pass。
- 不接入 LLVM/MLIR。
- 不改变现有 artifact report、bringup evidence、runtime ledger、witness bundle、world compare 的字段或判决模型。
