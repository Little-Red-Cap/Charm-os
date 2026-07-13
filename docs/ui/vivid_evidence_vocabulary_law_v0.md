# Vivid Evidence Vocabulary Law v0

> status: `contract`

本文定义 Vivid Evidence Plane 的候选字段语义和 promotion 边界。字段名称是 evidence language，不自动
成为 public runtime API。

```text
demo collector implementation -> stays demo-side
stable evidence vocabulary     -> law
runtime-native decision/result -> core-facing ledger candidate
```

`Examples/ui/vivid/evidence_vocabulary_demo` 只验证字段/helper verdict 与本法律一致，不证明实际渲染行为。

## 通用规则

- 字段描述 observed/declared causality，必须 deterministic、grep-friendly；
- 禁止 pointer、address、wall time、random id 和 localized text 进入稳定字段；
- field value 必须由对应 runtime fact 或 comparison 推导，不能只为 stdout 填值；
- helper implementation 可替换，但 stable field meaning 不得静默变化；
- stdout line shape 由 [`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md) 管理。

## Candidate Vocabulary

### StateDeltaEvidence

回答哪个 truth 由谁从什么值变成什么值。

| field | meaning |
|---|---|
| `state_delta` / `changed` | `old != new` 的派生 verdict；两者必须一致 |
| `id` | stable product/runtime owner id |
| `key` | stable truth key |
| `old` / `new` | 变化前后的 integer/stable enum |
| `source` | stable cause domain |
| `reason` | no-op/rejection 的稳定原因 |

rejected path 可以用 `state_delta=0` 证明 no mutation。state delta 不自动蕴含 layout 或 repaint，影响必须由
invalidation evidence 单独声明。

### InvalidationEvidence

回答变化声明了什么 impact、dirty ownership 在哪里、是否需要 layout。

| field | meaning |
|---|---|
| `invalidation` | 是否存在 invalidation claim |
| `kind` | `none/paint_only/layout/text_metrics/style/render_cache` |
| `dirty_scope` | `none/widget/component/page/layer/full_frame` |
| component bounds | containment evidence 的范围 |
| `layout_changed` | 是否要求 layout |

`paint_only` 不得同时声称 layout changed。component-local claim 应尽可能由 artifact dirty containment 证明；
declarative impact 不能冒充实际 render consequence。

### RenderEvidence

render evidence 摘要一次 pass 的 dirty、DrawCmd、execution 与 pixel artifact：

| field family | meaning |
|---|---|
| `*_dirty_count/hash` | dirty rect 数量与稳定结构摘要 |
| `*_cmd_count/bytes/hash` | draw intent 统计与摘要 |
| `*_exec_cmds/failed` | execution 结果 |
| `*_pixel_hash` | 被测 backend 的 pixel artifact 摘要 |

passing visual case 的 failed command 必须为 0，除非该 case 明确验证失败。`cmd_hash` 不是 command stream
golden，`pixel_hash` 也不是产品视觉审批。DrawCmd 观察边界见
[`vivid_draw_cmd_evidence_boundary_v0.md`](vivid_draw_cmd_evidence_boundary_v0.md)。

### RenderArtifactDeltaEvidence

比较 baseline 与 after artifact：

| field | meaning |
|---|---|
| `artifact_delta` | delta verdict 存在 |
| `changed` | render evidence 是否变化 |
| `dirty_within_component` | dirty 是否在 claimed bounds 内 |
| `single_dirty_rect` | 更强的单区域局部性证据 |

positive mutation 通常要求 changed；reject/no-op 通常要求 unchanged。`single_dirty_rect` 不是所有 case 的
默认法律。

### CausalChainEvidence

final `causal_chain` 将 request、state、invalidation、artifact 和 rejection guard 连接起来：

| field | meaning |
|---|---|
| `name` | stable chain identity |
| `ok` | required segments 的派生总 verdict |
| `request_ok` | request/admission/ledger segment |
| `state_delta_ok` | state segment |
| `invalidation_ok` | impact segment |
| `artifact_ok` | render consequence segment |
| `rejected_no_mutation` | rejection 保持 state/artifact 的 guard |

final verdict 不能是唯一 evidence line。`AxisCausal` 资格与 count-based 迁移规则见
[`vivid_causal_verdict_law_v0.md`](vivid_causal_verdict_law_v0.md)。

## Promotion Boundary

candidate 只有同时满足以下条件才可成为 core-facing contract：

1. 描述 runtime semantic fact，而不是 demo collection detail；
2. 至少被两个独立 evidence chains 消费；
3. 不依赖 stdout、CTest、`DefaultCanvas`、fixture 或 input simulation。

未通过 promotion tests 的类型可以继续作为 law vocabulary，但 implementation 保持 demo-side。

### Demo-side

以下类别不得因复用方便整体提升到 core：

- run log、expect、stdout formatter、case counter；
- click/pointer simulation 与 fixture setup；
- `DefaultCanvas` render/hash helper；
- one-off trace collector、scenario assertion；
- recorder/private wire probing helper。

`Examples/ui/vivid/support/vivid_evidence_support.hpp` 可以承载这些 helper，但不是 Vivid API。

### Runtime-native ledgers

由 runtime decision 或 completed execution 产生的 ledger 可以是 core-facing，例如 semantic focus/action request
ledger、page transition ledger、layer admission/profile decision、resolved style evidence。它们必须：

- 从完成的 runtime result 推导；
- 不依赖 demo printing；
- 保留 rejection/fallback 的真实 boundary/reason；
- 将“ledger fact 可在 core”与“如何打印仍在 demo”分开。

## 非目标

- 不定义 screenshot golden、完整 public C++ evidence API 或所有 widget 的 state object；
- 不替代 runtime-native ledger 专题契约；
- 不把当前 helper type layout 冻结为 ABI；
- 不用 vocabulary 稳定性证明 runtime behavior 已验证。
