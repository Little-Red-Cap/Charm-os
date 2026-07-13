# Vivid Semantic Transition Law v0

> status: `contract`

本文定义 semantic action 跨入 page transaction 的唯一边界。semantic request 可以产生启动 navigation 的
runtime edge，但不能直接修改 page truth，也不能绕过 transaction/layer admission。

该法律覆盖两个 evidence profile：

| profile | conformance sample |
|---|---|
| semantic -> transaction | `Examples/ui/vivid/semantic_transition_demo` |
| semantic -> state/render -> transaction | `Examples/ui/vivid/semantic_action_state_transition_demo` |

## Legal Chain

基础链：

```text
Semantic Action Request
  -> Semantic Admission
  -> Edge Evidence
  -> Transaction Admission
  -> PageTransitionRunner
  -> Layer Admission
  -> Render/Page Truth Evidence
  -> Causal Verdict
```

composite profile 在 edge 与 transaction admission 之间增加：

```text
State Delta -> Invalidation -> Render Artifact Delta
```

这是同一条 transition law 的增强 evidence profile，不是第二套 page transition 或 semantic ABI。

## Boundary Laws

### Semantic 不拥有 page truth

禁止：

```text
semantic request -> set current page
```

合法路径必须由 admitted semantic edge 触发 application bridge，再由 `PageTransitionRunner` commit page
truth。semantic code 不得自行 patch destination/source visibility。

### Admissions 不能合并

semantic admission 只证明 target/action 可执行；它不证明 transition resource、layer capture、backend
profile 或 budget 可用。证据必须区分：

```text
SemanticActionAdmission
PageTransitionAdmission
LayerAdmission
```

composite profile 的 state/render bridge 也不能冒充 transaction admission。

### Runtime edge 是唯一 bridge

application bridge 只能在 request ledger 证明 execution 且正常 input/action edge 已发出后启动 transition。
demo-side callback、直接函数调用或 page mutation 不能代替 edge evidence。

### Composite consequence 必须先闭合

composite profile 要求 state delta、invalidation 和 bounded render artifact 在 transaction begin 前可见。
这些证据证明 semantic action 的局部 consequence，但仍不授权 page commit。

### Transaction 拥有生命周期

begin 后的 sample、commit、cancel、abort、interrupt、fallback、static-cut cleanup、snapshot release/thaw 与
page truth restore 全部属于 Transaction/Layer axis。semantic 或 component code 不得独立清理 transaction
artifact。

## Positive Evidence

基础 profile 必须证明：

- semantic request executed，edge emitted；
- transaction 与 layer admission 可区分；
- transition sample/render consequence 可观察；
- commit 后 destination/source page truth 正确；
- runner 回到 idle，snapshot lifecycle 闭合。

composite profile 还必须在 transaction begin 前证明 state delta、invalidation impact 和 bounded artifact
delta。final causal verdict 应直接引用实际 required segments，不能只汇总 case count。

## Rejection Evidence

被拒绝路径必须证明：

```text
request not executed or edge not emitted
transaction bridge not started
page truth unchanged
snapshot ownership unchanged / no leak
state and render unchanged when composite bridge was not admitted
```

只有同时证明 no unintended mutation / no leaked resource，rejection 才能计入 causal evidence。规则见
[`vivid_causal_verdict_law_v0.md`](vivid_causal_verdict_law_v0.md)。state/render 字段语义见
[`vivid_evidence_vocabulary_law_v0.md`](vivid_evidence_vocabulary_law_v0.md)。

## 证据与路由

具体 demo registry、tag、case gate 和 axes 由
[`vivid_evidence_lab_manifest_v0.md`](vivid_evidence_lab_manifest_v0.md) 与
`Examples/ui/vivid/evidence_lab_manifest_demo` 定义。request ledger 见
[`vivid_semantic_request_ledger_law_v0.md`](vivid_semantic_request_ledger_law_v0.md)；vertical intent-to-artifact
边界见 [`vivid_intent_to_artifact_evidence_v0.md`](vivid_intent_to_artifact_evidence_v0.md)。本文不复制 stdout。

## 非目标

- 不新增 navigation、callback、transaction bridge 或 screenshot API；
- 不让 `SemanticActionRequest` 拥有 page transition；
- 不把 demo support helper 提升到 Vivid core；
- 不把 composite profile 拆成第二套 transition model。
