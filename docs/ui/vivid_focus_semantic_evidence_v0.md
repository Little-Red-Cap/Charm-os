# Vivid Focus Semantic Evidence v0

> status: `contract`

本文定义 semantic target、input focus truth 与 visual focus artifact 的最小对齐法律。它不引入完整
accessibility tree，也不允许 semantic API 绕过正常 input/runtime 边界。request ledger 见
[`vivid_semantic_request_ledger_law_v0.md`](vivid_semantic_request_ledger_law_v0.md)。

## Identity 与对齐

### Stable identity

semantic target 必须有稳定的 `semantic_id`、role、label 与 focusable truth。identity 不得来自 handle、
pointer、随机值或可变 display text；widget handle 只是 runtime binding。

装饰性 label、container、divider 可以参与布局和绘制，但在显式 opt-in 前不得成为 semantic target。

### 三种 truth

input focus 提交后必须能显式解析 semantic focus：

```text
input_focused=A.handle
semantic_current=A.semantic_id
visual_focus_ring=A.handle
```

focused handle 没有 semantic entry 时必须报告 not found，不能伪造对齐。semantic target 即使存在，也
不能绕过 active focus scope。scope 外拒绝必须保持当前 semantic/input truth，并证明无 visual artifact
泄漏。

### Causal evidence

完整 focus alignment verdict 至少连接：stable identity、pointer/keyboard transfer、`FocusOut / FocusIn`、
scope rejection、style boundary 与 focus ring artifact。`focused` 仍不得进入普通 style mask；causal
资格遵循 [`vivid_causal_verdict_law_v0.md`](vivid_causal_verdict_law_v0.md)。

## Semantic Artifact

### Tree snapshot

semantic tree 是 root-bound、fixed-capacity 的 evidence snapshot：

```text
root -> deterministic preorder semantic nodes -> semantic_hash
```

- 只采集已有 runtime semantic entry 的 node；
- root 是 artifact policy，page root 与 focus-scope root 可得到不同集合；
- capacity overflow 必须显式报告，不能静默截断；
- focused node 超出存储容量时，focus identity 仍应保留；
- `semantic_hash` 只摘要 semantic artifact，不宣称完整 accessibility tree。

### Defaults

pattern 可以派生 role 和 label source，但 stable id 必须由产品提供。decorative widget 不自动 opt-in；
显式 semantic 设置可以覆盖 default。handle 和 display text 都不得自动成为 stable id。

### Action mask

action mask 表示节点声明的能力，不表示事件已执行。v0 中 Button/ListItem 可默认声明 `activate`，
Text/Container 默认无 action；产品可显式覆盖。action 必须进入 tree artifact/hash，但不得因此合成 input、
调用 callback 或绑定 OS accessibility。

## Query、Admission 与 Request

三层边界不得合并：

```text
query      -> 当前是否可寻址
admission  -> 是否允许未来执行及执行计划
request    -> 受控跨入真实 input execution
```

### Intent resolution

`root + semantic_id + action` lookup 必须 root-bound 且 deterministic。duplicate id 是 ambiguous；missing
id、invalid root、unsupported action、disabled target 都是显式状态。resolved 只表示当前可寻址，不得产生
input、focus、state 或 callback side effect。

### Action admission

action admission 复用 intent resolution，不执行第二套 lookup。它声明未来是否需要 focus、是否会 emit
click；admitted 仍不等于已运行。所有 rejection 必须保留原 semantic reason，且不得改变 focus/pressed/
toggle truth。

### Focus query

`root + semantic_id + active_scope` 只回答当前是否 focus-addressable。non-focusable、disabled、missing、
ambiguous、invalid root 与 outside-active-scope 必须区分。query 不得发出 `FocusIn / FocusOut`、修改 input
focus 或绘制 focus ring。

### Focus admission

focus admission 复用 focus query，并产生 transfer plan：

- `admitted + transfer_needed`：未来允许发出 `FocusOut / FocusIn`；
- `already_focused`：admitted no-op，不计划事件；
- query failure：映射为同语义 rejection。

admission 本身不提交 focus。

### Focus request

focus request 是第一条允许提交 transfer 的 semantic 边界。它必须先经过 admission，并复用正常 input
focus transfer。`already_focused` 是无事件 no-op；rejected request 保持 current truth 且不发出事件；
committed request 暴露 before/after truth 与 event evidence。

### Action request

action request 可以在 action admission 后调用 focus request，再进入 click execution。拒绝必须命名最后
到达的边界：

| reason | 边界 |
|---|---|
| `action_admission_rejected` | semantic/action policy 未通过 |
| `focus_request_rejected` | active scope 或 focus admission 未通过 |
| `input_action_overflow` | execution queue 无容量 |
| `no_action_emitted` | execution 后未产生声明 action |

成功必须报告 `none`。除明确的 execution-time failure 外，被拒绝路径不得产生 click、focus event 或状态
变化。runtime ledger 是证据来源，demo printf 不能另造一套 stage 语义。

## 证据入口

| 入口 | 证明内容 |
|---|---|
| `Examples/ui/vivid/focus_semantic_demo` | stable identity、focus truth 与 ring alignment |
| `Examples/ui/vivid/semantic_tree_demo` | root policy、preorder、overflow、hash |
| `Examples/ui/vivid/semantic_default_demo` | opt-in defaults 与 explicit override |
| `Examples/ui/vivid/semantic_action_demo` | role-derived/overridden action artifacts |
| `Examples/ui/vivid/semantic_intent_demo` | root-bound lookup 与 no-execute failures |
| `Examples/ui/vivid/semantic_action_admission_demo` | action planning without side effects |
| `Examples/ui/vivid/semantic_focus_query_demo` | focus addressability 与 scope rejection |
| `Examples/ui/vivid/semantic_focus_admission_demo` | transfer planning 与 already-focused no-op |
| `Examples/ui/vivid/semantic_focus_request_demo` | controlled transfer 与 rejection stability |
| `Examples/ui/vivid/semantic_action_request_demo` | focus preparation、click execution 与 reject ledger |

具体 case 数、stdout token、API 名称与当前字段枚举由源码、manifest 和测试定义；本文只固定 identity、
artifact、query/admission/request 和拒绝无副作用边界。每个 AxisCausal profile 的 `causal_chain` 都必须
由对应 required evidence 推导。
