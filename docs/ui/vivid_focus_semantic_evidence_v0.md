# Vivid Focus Semantic Evidence v0

本文定义 Vivid v0 对 semantic focus target 与 visual focus artifact 对齐的最小证据。

## 定位

`Focus Evidence Boundary v0` 证明 `focused` 不进入普通 style mask。

`Focus Transfer / Scope / Navigation Evidence v0` 证明真实 input dispatch 可以提交 `input_focused`，并让 focus ring artifact 迁移。

`Focus Semantic Evidence v0` 继续向前一步：证明 focus 不只是视觉 ring，也必须能映射到稳定的产品语义 target。

v0 暂不引入完整 accessibility tree，也不把语义存储写入 SoA node。第一版使用 demo 侧 semantic target table 建立证据语言，后续再上收为 Vivid runtime capability。

## v0 法律

### Law 1：semantic target 必须稳定

每个可被语义暴露的 focus target 必须有稳定 id、role 与 label：

```text
semantic_id
role
label
focusable
```

这些字段不应依赖 widget handle 地址或运行时随机值。

### Law 2：input focus truth 必须能解析为 semantic focus

当 `input_focused` 提交到某个 target 后，semantic table 必须能解析：

```text
input_focused -> semantic_id
```

如果 focused handle 不在 semantic table 内，必须显式输出 `semantic_found=0`，而不是静默假装对齐。

### Law 3：semantic focus 与 visual focus artifact 必须对齐

当 semantic target A 成为 current focus：

```text
input_truth=A.handle
semantic_current=A.semantic_id
visual_focus_ring=A.handle
```

v0 使用 `FocusOut / FocusIn`、`input_focused`、`cmd_hash / pixel_hash` 共同证明对齐。

### Law 4：semantic focus 不得绕过 active scope

scope 外 target 即使存在 semantic entry，也不得被 active scope 内的 keyboard / spatial navigation 选中。

```text
outside_semantic_present=1
outside_selected=0
semantic_current remains inside scope
```

### Law 5：非语义 widget 可以存在，但不能污染 semantic focus

装饰性 label、container、divider 这类 widget 可以参与布局和绘制，但不应成为 semantic focus target。

```text
decorative_present=1
decorative_semantic=0
```

## 首个落点

`Examples/ui/vivid/focus_semantic_demo` 是 Focus Semantic Evidence v0 的第一条运行证据。

它验证：

- semantic target table 中存在 `primary / secondary / outside` 三个稳定条目。
- decorative label 不进入 semantic target table。
- pointer focus 可以从 primary 迁移到 secondary，并解析为 `semantic_id=secondary`。
- keyboard navigation 在 active scope 内迁移 semantic focus。
- scope 外 semantic target 不参与 active scope navigation。
- focus ring artifact 与 semantic current target 对齐。

stdout 最终约束：

```text
[fsem] run=focus_semantic_demo phase=end result=ok cases=8
```

核心字段：

```text
semantic_id=primary/secondary/outside
role=button/list_item
semantic_found=1
semantic_current=secondary
input_truth=secondary
focus_ring=1
outside_semantic_present=1
outside_selected=0
decorative_semantic=0
```

## 后续方向

- 把 semantic target table 上收为 Vivid core capability。
- 区分 input focus、semantic focus、accessibility focus 与 visual focus ring。
- 输出 semantic tree / accessibility tree 的 artifact hash。
- 让 component pattern 声明默认 semantic role 与 label source。
