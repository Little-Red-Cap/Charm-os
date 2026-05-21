# Compiler Lifecycle Projection Coverage Matrix v0

本文是 `Compiler World Lifecycle Projection v0` 的下游覆盖审计页。

它回答的是：九个 lifecycle states 当前分别由哪些真实 artifact、report、witness、ledger、compare surfaces 承接；哪些是直接投影，哪些只是解释性投影；哪些状态还需要未来 sidecar 补上镜头。

本文不创建新 truth，不定义 schema、summary 文件、validator、exporter、smoke、C++ 类型、canonical identity、observation import pass 或 LLVM/MLIR 接入。

## 1. 定位

`Compiler World Lifecycle Projection v0` 定义现有产物如何映射到 lifecycle states。

本文只审计这份映射的覆盖情况：

```text
lifecycle state
  -> current projection surfaces
  -> coverage strength
  -> projection kind
  -> sidecar eligibility
```

Coverage matrix 的职责是让 Charm 明确说出：

- 哪些 world states 已经被真实对象承接。
- 哪些 world states 只是法律上成立，但当前没有直接 artifact marker。
- 哪些 projection 是直接证据，哪些只是解释性阅读。
- 哪些缺口未来可以通过只读 sidecar 补齐。

Coverage matrix 不得：

- 解析 raw logs。
- 重跑 lower brain。
- 修改 verdict。
- 创建 source facts。
- 生成 new semantic truth。
- 替代 lifecycle、projection、runtime ledger、witness bundle 或 world compare 的原有合同。

## 2. Coverage Axes

### 2.1 Coverage strength

v0 使用四档覆盖强度：

| Strength | Meaning |
| --- | --- |
| `strong` | 已有明确 artifact / report / witness / compare surface 承接该 state |
| `medium` | 已有 surface 可稳定解释该 state，但仍缺少专用 marker 或 sidecar |
| `weak` | 只有间接 paired artifact 或归档语义支撑 |
| `missing` | 法律上成立，但当前没有直接或稳定投影 |

### 2.2 Projection kind

v0 区分两种 projection kind：

| Kind | Meaning |
| --- | --- |
| `direct` | 现有 surface 直接表达该 lifecycle state 或其判决 |
| `interpretive` | 需要通过 projection law 解释现有 surface 才能读出该 state |

同一个 state 可以是 `mixed`，表示它同时有直接 surface 与解释性 surface。

### 2.3 Sidecar eligibility

v0 使用三档 sidecar eligibility：

| Eligibility | Meaning |
| --- | --- |
| `not_required` | 当前 surface 已足够支撑 v0 阅读，不急需 sidecar |
| `optional` | sidecar 可提升审计清晰度，但不是当前缺口 |
| `recommended` | 当前缺少直接 marker，未来 sidecar 应优先补齐 |

Sidecar 只允许作为只读投影面。它不得成为新的 truth owner。

## 3. v0 Coverage Matrix

| Lifecycle state | Current projection surfaces | Coverage strength | Projection kind | Sidecar eligibility | v0 judgment |
| --- | --- | --- | --- | --- | --- |
| `declared` | `artifact_report`, `bringup_evidence_pipeline` | `strong` | `mixed` | `not_required` | 输入声明与 bringup 声明已有真实承接 |
| `materialized` | `artifact_report`, `bringup_evidence_pipeline` | `strong` | `mixed` | `not_required` | binding result、bringup order 与 evidence surface 已可稳定阅读 |
| `proven` | `kernel_runtime_session.summary.json` | `strong` | `direct` | `optional` | runtime session verdict 已给出明确 proof 落点 |
| `frozen` | constitution / freeze boundary law only | `missing` | `interpretive` | `recommended` | 法律上成立，但当前没有直接 frozen artifact marker |
| `lowered` | `artifact_report`, witness / metadata surfaces | `medium` | `interpretive` | `optional` | 有 lowered result surface，但 artifact 是结果面，不是完整 world truth |
| `witnessed` | `kernel_runtime_session.summary.json`, `witness_bundle` | `strong` | `mixed` | `not_required` | witness entry 与 bundle 已能承接回指语义 |
| `observed` | `runtime_ledger.json`, `bringup_evidence_pipeline` | `strong` | `direct` | `not_required` | exporter-consumed facts 与 bringup observed state 已有稳定观察面 |
| `archived` | `witness_bundle`, paired artifact direction | `weak` to `medium` | `interpretive` | `optional` | 归档语义已出现，但还不是完整 archive manifest |
| `compared` | `world_compare` | `strong` | `direct` | `optional` | exported witness / surfaces 的 compare 语义已有明确落点 |

这个矩阵不是 schema。它是 coverage audit。

## 4. State Notes

### 4.1 `declared`

`declared` 当前覆盖较强。

`artifact_report` 可以解释 system input、binding input 与 report surface。`bringup_evidence_pipeline` 也已经有 bringup-local `declared` state。

这里的 `mixed` 表示：bringup evidence 的 state 是直接表达；artifact report 对 lifecycle 的 `declared` 阅读是解释性投影。

### 4.2 `materialized`

`materialized` 当前覆盖较强。

binding result、bringup order、report surface 与 bringup evidence materialization 已经能承接“事实被规范化成可观察 surface”的语义。

### 4.3 `proven`

`proven` 当前有明确落点。

`kernel_runtime_session.summary.json` 通过 session verdict、machine witness、semantic witness 与 runtime facts 承接 proof 语义。

Sidecar 可以在未来提升 compiler lifecycle 审计体验，但 v0 不要求新增 proof sidecar。

### 4.4 `frozen`

`frozen` 是当前最重要的缺口。

它已经由 `Charm Compiler Constitution v0` 与 `Compiler Pass Authority & Semantic Freeze Boundary v0` 立法，但现有 evidence world 没有直接导出：

- freeze marker
- frozen world summary
- freeze authority receipt
- freeze-time legality snapshot

因此 v0 明确标记为 `missing`。这不是失败，而是 coverage matrix 要暴露的审计事实。

未来 sidecar 可以补齐 frozen projection，但本刀不定义 sidecar 文件、字段或 schema。

### 4.5 `lowered`

`lowered` 当前有中等覆盖。

`artifact_report`、witness / metadata surfaces 可以说明某些 target surfaces 已经从 world facts 投影出来。

但 lowered artifact 只是结果面，不是完整 world truth。尤其 firmware、report、docs、metadata 都必须遵守 lossy lowering law，不能被误读为 world 本体。

### 4.6 `witnessed`

`witnessed` 当前覆盖较强。

`kernel_runtime_session.summary.json` 与 `witness_bundle` 已能承接“artifact 或 observed behavior 可回指 world facts”的语义。

Witness 仍然只是观察者。它不得修改 semantic truth，也不得替代 builder、deriver 或 verifier。

### 4.7 `observed`

`observed` 当前覆盖较强。

`runtime_ledger.json` 表示 session exporter 已消费 summary facts 的顺序观察。`bringup_evidence_pipeline` 也已有 bringup-local observed state。

Observation 不得直接修改 frozen world。若 observation 未来要影响语义，只能形成 source fact proposal，再进入 new world branch。

### 4.8 `archived`

`archived` 当前覆盖弱到中等。

`witness_bundle` 与 paired artifact direction 已经提供归档入口，但当前还没有完整 archive manifest 或 archive receipt 来证明：

- world summary
- paired artifacts
- witness bundle
- evidence summary
- topology / capability metadata

已经作为一组可复盘交付被保存。

因此 future sidecar 是 `optional`，不是本刀要求。

### 4.9 `compared`

`compared` 当前有明确落点。

`world_compare` 可以比较 exported witness bundles 或合法 surfaces。它不得重建 truth、重跑 lower brain、读取 raw runtime evidence 或修复 drift / collapse。

当前 `world_compare` 文档在归档目录中，但 compare 语义仍可作为已有 projection coverage 对象引用。本刀不恢复旧 front-page 大索引。

## 5. Sidecar Direction

未来可以考虑只读 sidecar 来补齐 coverage 缺口，例如：

```text
compiler_lifecycle.summary.json
compiler_freeze_receipt.json
compiler_archive_manifest.json
```

这些名字只是未来方向，不是本刀接口。

任何 sidecar 都必须遵守：

- read-only projection
- no raw log parsing
- no lower brain rerun
- no verdict mutation
- no source fact creation
- no canonical identity definition by accident
- no observation import implementation by accident

## 6. 非目标

本 v0 不做：

- 不新增 JSON schema。
- 不新增 summary 文件。
- 不新增 validator、exporter、smoke 或脚本。
- 不新增 C++ 类型或 IR。
- 不定义 canonical identity。
- 不实现 observation import pass。
- 不定义 semantic debugging protocol。
- 不把 semantic optics 升格为正式主语。
- 不接入 LLVM/MLIR。
- 不改变现有 artifact report、bringup evidence、runtime ledger、witness bundle、world compare 的字段或判决模型。
