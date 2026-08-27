# UI Kernel Contract v0（硬规则）

目标：把 UI 语义红线固化成可执行契约，统一 Ink / Vivid 的输入、布局、渲染与边界约束。

## 适用范围
- Ink / Vivid 共享的输入链路、布局失效、焦点/导航与渲染记录/执行边界。
- 所有 UI 域必须遵守本契约，不得绕过。

## 硬规则（10 条）
1) 输入阶段只允许产出 Action。
2) 状态写入只允许在统一提交点发生。
3) hover/pressed/focused/capture/drag 的最终写权限属于 kernel。
4) repaint / relayout 必须可判定：默认 hover/pressed/focus 只触发 repaint。
5) layout 影响位必须显式声明，未声明不得触发布局。
6) UI 只负责 record，gfx 只负责 execute；任何 UI 域共享的绘制基础设施必须归属 gfx。
7) style 解释只发生在 record 侧，禁止回灌执行层。
8) public 聚合禁止 re-export internal/private 模块。
9) bridge/compat/alias 仅允许 internal，且必须声明生命周期。
10) Ink/Vivid 共享契约不得依赖 Vivid 专有模块。

## 最小守卫（先上 2 条）
- 守卫 1：输入阶段禁止直接写状态（hover/pressed/focused/capture/drag）。
- 守卫 2：public 聚合入口不得 re-export internal/private 模块。

## 违规处理（硬）
- 命中守卫即构建失败。
- 新增 bridge/compat/alias 未声明生命周期即构建失败。
- 除正式 public 清单与显式允许白名单外，新增对 deprecated/internal 模块的依赖即构建失败。

## 相关文档
- 聚合入口状态：`docs/architecture/entry_surface_contract.md`
- UI 专题入口：`docs/ui/README.md`
