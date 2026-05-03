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

v0 同时保留 `decide_focus_scope_request()` 作为 policy helper，并把同一条准入语义接入 `SoaKernel::input_set_focus()`，让真实 input dispatch 能提交 scope 内焦点迁移。

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

这条证据比简单比较内外 `cmd_count` 更稳，因为不同 target 尺寸或 widget 形态可能产生不同的基础命令数。

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

stdout 最终约束：

```text
[fs] run=focus_scope_demo phase=end result=ok cases=9
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
```

## 后续方向

- 支持 modal / popup 的 nested focus scope。
- 支持 keyboard / d-pad 在 scope 内循环。
- 为 accessibility focus 增加 semantic focus target 与 visual focus artifact 对齐证据。
