# Vivid Focus Scope Evidence v0

本文定义 Vivid v0 对焦点作用域与焦点陷阱的最小证据。

## 定位

`Focus Evidence Boundary v0` 证明 `focused` 不进入普通 style mask，而是通过 focus ring 改变 render artifact。

`Focus Transfer Evidence v0` 证明真实输入链可以把 focus truth 从 old target 提交到 new target，并留下 `FocusOut / FocusIn` 事件证据。

`Focus Scope Evidence v0` 继续向前一步：证明一个组件或 overlay 可以声明焦点作用域，允许 scope 内迁移，拒绝 scope 外焦点提交，并保证被拒绝请求不会把 focus ring artifact 泄漏到外部 target。

## v0 法律

### Law 1：scope 内请求必须允许

当 requested target 属于 `FocusScopeSpec::scope` 时：

```text
decision=allow
allowed=1
```

v0 保留 `decide_focus_scope_request()` 作为 policy helper，并把同一条准入语义接入 `SoaKernel::input_set_focus()`，让真实 input dispatch 能提交 scope 内焦点迁移。

### Law 2：scope 外请求必须拒绝

当 requested target 不属于 scope，且 `trap=true` 时：

```text
decision=reject_outside_scope
allowed=0
fallback=<current-or-fallback>
```

`RejectOutsideScope` 对 Focus Scope 不是失败，而是焦点陷阱的合法裁决。

### Law 3：scope truth 必须由 runtime focus admission 提交

v0 的 scope helper 返回：

```text
requested
fallback
decision
```

runtime 集成后，`SceneBuilder::set_focus_scope()` / `SceneAccess::set_focus_scope()` 会安装 active focus scope：

```text
scope
fallback
trap
```

真实 input dispatch 进入 `input_set_focus()` 后先经过 focus admission：

```text
inside request -> FocusOut(old) + FocusIn(new) + input_focused=new
outside request -> no focus transfer + input_focused remains fallback/current
```

`SceneAccess::set_focused()` 仍然只是 visual focus flag helper，不等价于 `input_focused()` 提交。

### Law 4：scope 外 artifact 不得泄漏 focus ring

被拒绝的外部请求不应让外部 target 获得 focus ring artifact。v0 使用外部 target 的 unfocused baseline 对照：

```text
outside_cmd_hash == outside_baseline_cmd_hash
outside_pixel_hash == outside_baseline_pixel_hash
outside_focus_ring=0
leaked=0
```

这条证据比简单比较内外 `cmd_count` 更稳定，因为不同 target 尺寸或 widget 形态可能产生不同的基础命令数。

### Law 5：focus scope demo 必须闭合 final causal_chain

`focus_scope_demo` 的最终 verdict 需要同时证明：

```text
request_ok=1
state_delta_ok=1
invalidation_ok=1
artifact_ok=1
rejected_no_mutation=1
causal_chain ok=1
```

这条法律把 inside allow、outside reject、trap truth 与 no-leak artifact 收束成同一张证据账本。

### Law 6：nested scope 必须 push/pop 闭合

modal / popup 这类临时 UI 不应该覆盖 base scope 后遗忘旧 truth。v0 使用小型 focus scope stack 表达：

```text
push modal scope -> 保存 base scope frame
pop modal scope  -> 恢复 base scope frame
```

嵌套 scope 的拒绝顺序是：

```text
inside request  -> allow requested
outside request -> keep current focused target if still inside active scope
otherwise       -> fallback if fallback still inside active scope
otherwise       -> reject to empty
```

也就是 current-first / fallback-second。这样用户点击 modal 外部时，不会把 modal 内已有焦点重置到 modal fallback。

`focus_scope_nested_demo` 的 final causal verdict 还必须证明：

```text
push modal scope -> active=modal stack=1
modal outside request -> no focus transfer + no base artifact mutation
pop modal scope -> active=base stack=0
restored base rejects modal target -> fallback=base_b + no modal artifact mutation
causal_chain ok=1
```

### Law 7：keyboard / d-pad navigation 必须限制在 active scope

键盘与方向键焦点移动不应绕过 active focus scope。v0 先用 deterministic preorder focusable 顺序建立键盘导航基础：

```text
Tab / Right / Down -> next focusable in active scope
Left / Up          -> previous focusable in active scope
end -> beginning   -> wrap
beginning -> end   -> reverse wrap
```

scope 外 target 不参与候选集。每次移动都必须走与 pointer focus transfer 相同的提交链：

```text
FocusOut(old)
FocusIn(new)
input_focused=new
```

`focus_scope_navigation_demo` 的 final causal verdict 还必须证明：

```text
Tab/Right/Down/Left -> expected FocusOut/FocusIn chain
wrap/reverse wrap -> stays inside active scope
outside target -> outside_candidate=0 + no focus artifact mutation
causal_chain ok=1
```

### Law 8：directional key 应优先使用 spatial focus candidate

遥控器 / 手柄 UI 里的方向键不应该只等价于 preorder。v0 对 `Left / Right / Up / Down` 增加空间候选裁决：

```text
directional key -> choose nearest focusable candidate in that geometric direction
no spatial candidate -> fallback to preorder wrap
Tab -> always preorder
```

空间候选仍然必须受 active scope 约束。scope 外 target 即使在几何方向上更近，也不得进入候选集。

## 首个落点

`Examples/ui/vivid/focus_scope_demo` 是 Focus Scope Evidence v0 的第一条运行证据。

它构造：

```text
root
  scope
    inside_a
    inside_b
  outside
```

并验证：

- `inside_a / inside_b` 属于 focus scope。
- `outside` 不属于 focus scope。
- runtime 安装 `scope=container fallback=inside_b trap=1`。
- `inside_b` 请求被允许，真实 dispatch 产生 `FocusOut(inside_a)` / `FocusIn(inside_b)`，并把 `input_focused` 提交到 `inside_b`。
- `outside` 请求被拒绝，pointer event 仍送达 outside，但不产生 `FocusOut / FocusIn`，`input_focused` 保持在 fallback/current。
- `outside` artifact 与 unfocused baseline 一致，没有 focus ring 泄漏。
- final `causal_chain` 汇总 inside allow、outside reject、trap truth 与 no-leak artifact。

`Examples/ui/vivid/focus_scope_nested_demo` 是 Focus Scope Nested Evidence v0 的第一条运行证据。

它验证：

- base scope 安装后可以提交 `base_a` focus。
- `push_focus_scope(modal_scope)` 后 active scope 切换到 modal，stack size 变为 1。
- modal 内请求产生 `FocusOut(base_a)` / `FocusIn(modal_b)`，并把 `input_focused` 提交到 `modal_b`。
- modal 外请求只保留 pointer event，不产生 focus transfer，`input_focused` 保持 `modal_b`。
- `pop_focus_scope()` 后 active scope 恢复 base，stack size 回到 0。
- 恢复 base scope 后，modal target 请求被 base scope 拒绝并重定向到 base fallback。
- final `causal_chain` 汇总 push/pop transaction、modal trap、restored fallback 与 no-leak artifact。

`Examples/ui/vivid/focus_scope_navigation_demo` 是 Focus Scope Navigation Evidence v0 的第一条运行证据。

它验证：

- `Tab` 将焦点从 first 移到 second。
- `Right` 将焦点从 second 移到 third。
- `Down` 从 third wrap 到 first。
- `Left` 从 first reverse wrap 到 third。
- scope 外 target 不进入 keyboard / d-pad navigation 候选集。
- 每次移动都产生 `FocusOut / FocusIn` 并提交 `input_focused`。
- final `causal_chain` 汇总顺序导航、wrap、scope 外排除与 no-leak artifact。

`Examples/ui/vivid/focus_spatial_navigation_demo` 是 Focus Spatial Navigation Evidence v0 的第一条运行证据。

它验证：

- `Right / Down / Left / Up` 按世界坐标矩形选择空间方向候选。
- `Tab` 保持 preorder 导航与 wrap。
- 没有空间候选时，方向键回退到 preorder wrap。
- scope 外 target 不进入 spatial candidate。
- 每次移动都产生 `FocusOut / FocusIn` 并提交 `input_focused`。

stdout 最终约束：

```text
[fs] run=focus_scope_demo phase=end result=ok cases=10
[fsn] run=focus_scope_nested_demo phase=end result=ok cases=9
[fsnav] run=focus_scope_navigation_demo phase=end result=ok cases=8
[fss] run=focus_spatial_navigation_demo phase=end result=ok cases=9
```

核心字段：

```text
policy=focus_admission
decision=allow
decision=reject_outside_scope
focus_out=1
focus_in=1
input_truth=inside_b
fallback=inside_b
outside_focus_ring=0
leaked=0
causal_chain=1
rejected_no_mutation=1
stack=0/1
pushed=1
popped=1
causal_chain=1
rejected_no_mutation=1
key=tab/right/down/left
wrap=1
mode=spatial/preorder
fallback=1
outside_candidate=0
```

## 后续方向

- 为 accessibility focus 增加 semantic focus target 与 visual focus artifact 对齐证据。
