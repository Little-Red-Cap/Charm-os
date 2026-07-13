# Vivid Causal Verdict Law v0

> status: `contract`

本文定义 Vivid `causal_chain` verdict 的资格与含义，防止它退化为“所有 case 都通过”的结尾。stdout
形状由 [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md) 管理，共享字段由
[`vivid_evidence_vocabulary_law_v0.md`](vivid_evidence_vocabulary_law_v0.md) 管理；本文不把 demo support
helper 提升为 Vivid core。

## Evidence 层级

```text
Evidence Point -> 单个可观察事实
Evidence Chain -> 按 runtime 顺序连接的多个事实
Causal Verdict -> 对 chain 的 commit/reject/fallback/cancel/rollback 判断
```

final verdict 是摘要，不是唯一证据行。它必须由前面的 required segments 推导，不能手写一个独立
`ok=1`。

## AxisCausal 资格

demo 只有同时证明以下内容才可声明 `AxisCausal`：

1. 存在 intent 或 request；
2. 执行了 admission、policy 或 precondition 检查；
3. execution、rejection、fallback、cancel 或 rollback 边界明确；
4. 至少有一个可观察 consequence；
5. final verdict 把 required evidence segments 连接起来。

consequence 可以是 state、focus、time、transaction、layer、budget、invalidation、DrawCmd、render artifact、
snapshot lifetime 或 semantic artifact。rejection/no-op/fallback/cancel/rollback 只有在证明 no unintended
mutation 或 no leaked resource 时才算 causal。静态 metadata 与 stdout formatting 测试不得声明
`AxisCausal`。

## Verdict 形状

canonical final line 仍是：

```text
[tag] case=causal_chain causal_chain=1 name=<stable_name> ok=<derived_verdict> ...
```

chain name 必须稳定；semantic chain 优先使用产品语义名，runtime chain 使用法律名。共享 segment 字段包括：

```text
request_ok
state_delta_ok
invalidation_ok
artifact_ok
rejected_no_mutation
```

### Count-based

`prior_cases_complete=1` 只允许用于已有 transitional verdict，且 paired law 必须明确它闭合的 evidence
segments。case count 本身不能证明 cause-and-effect。

### Evidence-referenced

新 causal demo 应在 final line 直接引用 segment verdict。既有 count-based verdict 不要求为格式统一而立即
重写；后续行为修改时应优先迁移到 evidence-referenced 形式。

shared helper 只能减少 stdout 拼装，不能定义字段法律或 verdict 含义。

## Verdict Families

### Semantic

必须连接 resolution/request ledger、admission/rejection、planning 或 execution boundary，以及 state/focus/
event/semantic/render consequence。rejection case 必须证明 no mutation。

semantic-to-transaction 还必须保持两层 admission：semantic intent 可以发出 edge 启动 transaction，但不能
直接修改 page truth。边界见
[`vivid_semantic_transition_law_v0.md`](vivid_semantic_transition_law_v0.md)。

### Time

必须连接 managed time source、motion recipe/profile decision、compose/page trace、相关 budget/admission，
以及 bounded final state。page-local frame loop 不能冒充 runtime-owned time evidence。

### Transaction

必须连接 begin/admission、owned artifact acquisition、commit/cancel/interrupt/fallback 分支、release/thaw/
restore，以及 no leaked snapshot / no stale transaction state。

### Render / State

必须连接 state delta 或 no-delta、invalidation、dirty containment、DrawCmd/render artifact delta，以及被拒绝
或 no-op 时的 artifact stability。

### Composite

semantic-action-state-transaction chain 必须分别保留 semantic request、event/edge、state delta、
invalidation、artifact delta、transaction admission、commit/abort、snapshot lifecycle 与 page truth；不能用
一个宽泛 `ok` 抹平各边界。

## Manifest 关系

[`vivid_evidence_lab_manifest_v0.md`](vivid_evidence_lab_manifest_v0.md) 负责 demo-to-axis map。声明
`AxisCausal` 的 row 必须满足以下之一：

1. stdout final verdict 使用 evidence-referenced fields；
2. primary law 明确 count-based verdict 闭合的 segments。

manifest 当前的 vertical/composite anchor 由 manifest 自己维护；本文不复制 demo 名单、case 数或阶段状态。

## 非目标

- 不新增 demo、case、screenshot golden 或 C++ API；
- 不要求立即重写全部 transitional verdict；
- 不替代 stdout law、vocabulary law 或 manifest；
- 不将 `Examples/ui/vivid/support/` 变成 core contract。
