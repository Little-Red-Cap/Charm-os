# Vivid Render Evidence Chain v0

本文定义 Vivid 从 widget 级证据进入 component 级因果证据的最小路线。

它的目标不是替代截图回归，而是在截图之前先证明：

```text
state truth
  -> invalidation intent
  -> dirty evidence
  -> draw command evidence
  -> render artifact evidence
```

## 定位

`vivid_evidence_stdout_law.md` 定义 stdout 行格式。

本文定义 stdout 中应该逐步承载哪些 UI 因果事实。

## v0 证据段

### State Truth

状态变化必须能回答：

```text
哪个 truth 变了？
旧值和新值是什么？
变化来源是什么？
```

v0 先不要求所有 Vivid 控件都产出统一 `StateDelta` 类型，但 component demo 必须在 case 中输出稳定的状态摘要。

### Invalidation Intent

状态变化必须声明预期影响：

```text
none
paint_only
layout
text_metrics
style
render_cache
```

v0 先允许 demo 以 `invalidation=paint_only` 这类字段表达 intent。

### Dirty Evidence

渲染后必须能回答：

```text
是否产生 dirty？
dirty 是否局部？
dirty 是否越出 component 边界？
```

v0 使用 `dirty_count` 与 `dirty_hash` 做最小证据。

### Draw Command Evidence

截图之前，先审计绘制意图：

```text
cmd_count
cmd_bytes
cmd_hash
exec_cmds
failed_cmds
```

v0 可以用 `Scene::last_cmd_stats()` 与 `Scene::last_exec_stats()` 的稳定字段组成 `cmd_hash`。

### Render Artifact Evidence

最终 artifact 先用摘要表达：

```text
width
height
pixel_hash
```

PNG / screenshot diff 是后续投影，不是 v0 的第一真相。

## 首个落点

`Examples/ui/vivid/component_settings_row_demo` 是第一条 component 级证据链样本。

它验证一个 settings row component 中：

- slider truth 变化可被观测。
- progress mirror 受 slider truth 驱动。
- value label 随 truth 更新。
- 本次变化声明为 `paint_only`。
- render 后输出 dirty / command / pixel artifact 摘要。

`Examples/ui/vivid/component_card_state_demo` 是第二条 component 级证据链样本。

它验证一个 card component 中：

- checkbox 与 slider 两个 child truth 同时变化。
- progress mirror 与 summary label 汇入同一个 component artifact。
- 多 child state 变化仍声明为 `paint_only`。
- render 后输出单个 card dirty rect 与 command / pixel artifact 摘要。

stdout 仍遵守 `vivid_evidence_stdout_law.md`。
