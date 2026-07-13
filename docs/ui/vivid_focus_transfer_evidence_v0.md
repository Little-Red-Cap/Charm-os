# Vivid Focus Transfer Evidence v0

> status: `contract`

本文定义 Vivid v0 在两个 focusable target 之间提交 focus transfer 的最小证据。ordinary style 隔离由
[`vivid_focus_evidence_boundary_v0.md`](vivid_focus_evidence_boundary_v0.md) 定义；本文只固定 event、truth、
invalidation 与 artifact 迁移。

## Event 法律

已有 focus target 时，新的 focusable target 经 input dispatch 获得 focus，必须产生：

```text
FocusOut(old)
FocusIn(new)
```

同一 dispatch 可以保留原始 pointer/key event，但 `FocusOut / FocusIn` 必须从正常 input event surface
观察，不能由 demo 另造旁路通知。

already-focused no-op 或 rejected transfer 不得产生虚假的 out/in pair。

## Truth 法律

成功 transfer 后，kernel truth 必须提交到 destination：

```text
input_focused == new target
```

事件存在但 truth 未提交不算成功。scope/policy 拒绝时，current truth 必须保持；scope admission 与 fallback
见 [`vivid_focus_scope_evidence_v0.md`](vivid_focus_scope_evidence_v0.md)。

## Style 与 Artifact 法律

transfer 不扩展 ordinary style mask：

```text
focused_in_style_mask=0
resolved_style same
```

artifact 必须从 source 迁移到 destination。两个同构控件可能产生相同 draw command shape，因此不能只看
`cmd_hash`；证据应结合 target、pixel/dirty artifact 与 focus ring presence。被拒绝路径必须证明 artifact
不变且没有 ring 泄漏。

## Causal Closure

final `causal_chain` verdict 必须连接：

```text
input request
FocusOut / FocusIn
input_focused truth
bounded invalidation
destination artifact
rejected-no-mutation (when applicable)
```

单独的 event count、最终 `ok=1` 或 style hash 都不足以证明 transfer。资格规则见
[`vivid_causal_verdict_law_v0.md`](vivid_causal_verdict_law_v0.md)。semantic identity 对齐见
[`vivid_focus_semantic_evidence_v0.md`](vivid_focus_semantic_evidence_v0.md)。

## 证据入口

`Examples/ui/vivid/focus_transfer_demo` 使用无额外业务 state 变化的 focusable targets，验证真实 dispatch、
truth commit、style stability 与 artifact relocation。具体 fixture、case 数和 stdout token 由 demo、manifest
与测试定义，不在本文复制。
