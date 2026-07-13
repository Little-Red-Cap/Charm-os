# Vivid PageLayer / StylePatch 边界

> status: `supporting`
>
> scope: local style override 与 PageLayer basic visibility/hook 使用

本文不是 API 清单。当前字段和方法以
[`style.cppm`](../../Modules/ui/vivid/core/style.cppm) 与
[`scene.cppm`](../../Modules/ui/vivid/core/scene.cppm) 为准。

## StylePatch

`StylePatch` 在 resolved stylesheet 结果上对单个 widget 做局部 override。构建期和运行期分别通过
`SceneBuilder` / `SceneAccess` 设置或清除 patch。

边界：

- patch 只属于目标 widget，不修改全局 theme/stylesheet；
- color、state color 与 metrics override 由当前 `StylePatch` 字段定义，文档不复制字段表；
- metrics-affecting patch 必须遵守 layout/text-metrics invalidation，不能统一当作 repaint；
- 产品专属视觉值留在产品层，不因使用 patch 就提升为 Vivid token；
- semantic token、state mask 与 impact 法律见
  [`vivid_style_token_law_v0.md`](vivid_style_token_law_v0.md)。

## PageLayer Basic Mode

`PageLayer` 持有 page root、visibility、layer state、hooks 与可选 snapshot ownership。basic mode 只使用：

```text
root + show/hide + on_show/on_hide
```

`show/hide` 通过 `SceneAccess` 提交 root visibility；hook 只在 visibility truth 实际变化时执行。重复 show
或 hide 不得重复触发 hook。refresh/page-state 同步仍由产品 controller 决定，PageLayer 不拥有 navigation。

`PageLayer` 还提供 freeze/replay/transition 相关能力；一旦使用 snapshot 或非 Live/Hidden state，就必须遵守
[`vivid_layer_runtime_v0.md`](vivid_layer_runtime_v0.md) 和
[`vivid_motion_runtime_v0.md`](vivid_motion_runtime_v0.md)，不能用本页 basic mode 规避 capture、admission、
rollback 或 release 规则。

## 非目标

- 不定义页面业务状态、navigation 顺序或产品刷新策略；
- 不建立第二套 theme/style/layout 系统；
- 不承诺 `PageLayer` 只有 show/hide 能力；
- 不复制 C++ 示例、字段枚举或推荐实施顺序。
