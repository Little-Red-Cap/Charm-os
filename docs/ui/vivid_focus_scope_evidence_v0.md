# Vivid Focus Scope Evidence v0

> status: `contract`

本文定义 Vivid v0 的 focus scope、focus trap 与 scope-bound navigation 证据。它建立在
[`vivid_focus_evidence_boundary_v0.md`](vivid_focus_evidence_boundary_v0.md) 和
[`vivid_focus_transfer_evidence_v0.md`](vivid_focus_transfer_evidence_v0.md) 之上：前者隔离 visual focus，
后者证明正常 `FocusOut / FocusIn` 提交链；本文只负责 scope admission。

## 核心边界

active scope 由以下 truth 描述：

```text
scope + fallback + trap
```

`SceneBuilder::set_focus_scope()` / `SceneAccess::set_focus_scope()` 安装 scope；真实 input dispatch 在
`SoaKernel::input_set_focus()` 提交前执行 admission。`SceneAccess::set_focused()` 只是 visual flag helper，
不提交 `input_focused()`。

## Admission 法律

### Scope 内允许

requested target 属于 active scope 时必须允许，并复用正常 focus transfer：

```text
decision=allow
FocusOut(old) + FocusIn(new)
input_focused=new
```

### Trap scope 外拒绝

`trap=true` 且 target 在 scope 外时必须拒绝：

```text
decision=reject_outside_scope
input_focused remains current/fallback
```

`reject_outside_scope` 是合法 policy verdict，不是 runtime 故障。被拒绝路径不得发出 focus transfer，
也不得把 focus ring 泄漏到外部 target。

### Fallback 顺序

scope 外请求按以下顺序选择保留目标：

```text
current focus still inside active scope -> current
fallback still inside active scope      -> fallback
otherwise                               -> empty
```

这保证 modal 外点击不会把 modal 内已有焦点无条件重置到 fallback。

## Artifact 法律

拒绝不能只由一个 policy flag 证明，还必须证明 no mutation / no leak。外部 target 应与其 unfocused
baseline 一致：

```text
outside_cmd_hash == outside_baseline_cmd_hash
outside_pixel_hash == outside_baseline_pixel_hash
outside_focus_ring=0
```

不得只比较 `cmd_count`，因为不同尺寸和 widget 形态可以有不同的基础命令数。最终 causal verdict 必须连接
request、state delta、invalidation、artifact 与 rejected-no-mutation；资格规则见
[`vivid_causal_verdict_law_v0.md`](vivid_causal_verdict_law_v0.md)。

## Nested Scope 法律

modal / popup 使用 push/pop transaction，不得覆盖 base scope 后遗失旧 truth：

```text
push modal -> save base frame -> active=modal
pop modal  -> restore base frame -> active=base
```

证据必须覆盖：

- push 后 modal 内迁移成功；
- modal 外请求被拒绝且 base artifact 不变；
- pop 后恢复 base scope 与 stack depth；
- 恢复后的 base scope 继续拒绝 modal target；
- 所有分支都没有 stale scope 或 focus artifact 泄漏。

## Navigation 法律

keyboard、d-pad 与 spatial navigation 都必须先裁剪到 active scope，再提交正常 focus transfer。

### Preorder

```text
Tab / Right / Down -> next focusable
Left / Up          -> previous focusable
end <-> beginning  -> wrap
```

`Tab` 始终使用 deterministic preorder。scope 外 target 不进入候选集。

### Spatial

方向键优先选择该几何方向上最近的 focusable target；没有 spatial candidate 时回退到 preorder wrap。
距离更近的 scope 外 target 仍不得成为候选。

每次成功移动都必须产生 `FocusOut / FocusIn` 并提交 `input_focused`；无候选或被拒绝路径必须证明 truth
和 artifact 不变。

## 证据入口

| 入口 | 证明内容 |
|---|---|
| `Examples/ui/vivid/focus_scope_demo` | inside allow、outside reject、fallback 与 no-leak |
| `Examples/ui/vivid/focus_scope_nested_demo` | push/pop、modal trap、base restore |
| `Examples/ui/vivid/focus_scope_navigation_demo` | preorder、wrap、scope candidate exclusion |
| `Examples/ui/vivid/focus_spatial_navigation_demo` | spatial choice、preorder fallback、scope exclusion |

具体 case 数、stdout token 与当前 API 拼装由 demo、manifest 和测试定义，不在本文复制。证据变化时必须保持
本页的 admission、fallback、no-mutation 与 no-leak 法律。
