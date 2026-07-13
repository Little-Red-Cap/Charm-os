# Vivid Import Boundary Contract

本文定义 Vivid v0 阶段的 import 边界。

目标不是整理所有模块名，而是冻结一个可审查规则：

> 产品 UI 代码、Vivid 内部实现、Evidence Lab / regression demo 不共享同一 import 权限。

## 分层入口

### Product-facing

产品 UI 默认只应依赖这些入口：

- `charm.ui.vivid`
- `charm.ui.scene`
- `charm.gfx.canvas`
- `charm.gfx.color`
- `charm.gfx.image`
- `charm.gfx.render_style`
- 必要的 `charm.core.geometry` / `charm.core.style` / `charm.core.theme_preset`

规则：

- `charm.ui.vivid` 是产品根入口，不是 host tool、font provider、snapshot、motion / page-transition 的方便型大聚合。
- `Scene` 是产品运行时主边界。
- 产品证据观察 `Scene::last_cmd_stats()`、`Scene::last_exec_stats()`、`Scene::layer_stats()` 等 scene-level facts。
- 页面、pattern、controller 不应直接依赖 SoA kernel、DrawCmd partition 或 scene private detail。

产品代码需要扩展能力时应显式 import 对应 surface：

- runtime motion / page transition：`charm.ui.scene.motion_runtime`
- VFS font package / typography runtime：`charm.ui.vivid.font_runtime`
- host snapshot / DrawCmd evidence tools：`charm.gfx.host_tools`，仅限 host CI、diagnostic 或 internal regression；不属于 MCU 产品入口。

### PRODUCT profile closure

PRODUCT 的 source closure 由 Vivid 自己的 Profile Compiler 生成：

- `.cppm` 中的单行 `module`、`import`、`export import` 是模块依赖唯一真源。
- module policy 只声明 `PRODUCT_ROOT`、`INTERNAL`、`HOST_ONLY`；不得在 CMake 中再维护一份依赖图。
- profile 只能显式选择 `PRODUCT_ROOT`。SoA kernel、DrawCmd partitions 和 widget implementation 只能由 public root 或 catalog module 的真实 closure 引入。
- closure 到达 `HOST_ONLY` 会在配置期失败；非 Vivid module 记录为 external requirement，由上层 target 提供。
- `WidgetKind` 保持完整、稳定的 `uint8_t` ABI。profile 只裁剪 active capability、module implementation 与 payload pool，不裁枚举项。

Profile Compiler 是 Vivid `Implementation / Tool`，不进入 Charm Core，也不扩大产品 import 权限。PRODUCT 的旧手写白名单与 `CHARM_VIVID_PAYLOAD_CAP_*` 配置已删除，发现旧变量时配置直接失败。

### Scene support

以下模块是 `Scene` 的支撑层，不是独立产品架构层：

- `charm.ui.scene.builder_support`
- `charm.ui.scene.layer_support`
- `charm.ui.scene:render_detail`

规则：

- `builder_support` / `layer_support` 可以被 `charm.ui.scene` re-export，作为 `Scene` 附属 surface 使用。
- 产品代码不应把它们当成可独立演进的入口。
- `scene:render_detail` 是 private partition，不得被产品、demo 或 evidence 直接依赖。

细节边界见 [`vivid_scene_support_boundary_v0.md`](vivid_scene_support_boundary_v0.md)。

### Internal runtime

以下入口只用于 Vivid 内部或明确的 internal regression：

- `charm.ui.vivid_internal`
- `charm.core.soa_kernel`
- `charm.core.soa_factory`
- `charm.core.soa_gui`
- `charm.core.soa_payload`
- `charm.gfx.draw_cmd`
- `charm.gfx.draw_cmd:schema`
- `charm.gfx.draw_cmd:buffer`
- `charm.gfx.draw_cmd:executor`

规则：

- `charm.ui.vivid_internal` 是 internal regression 汇总入口，不是产品入口。
- SoA kernel / factory / gui 是 runtime implementation boundary，不是页面业务 API。
- DrawCmd partition 的私有编码、arena offset、payload layout、executor grouping 不进入产品证据词汇。

DrawCmd evidence 边界见 [`vivid_draw_cmd_evidence_boundary_v0.md`](vivid_draw_cmd_evidence_boundary_v0.md)。

### Object-level widget

`charm.widgets.*` 表达 object-level widget / helper 表面。

规则：

- `Button::observe_click()`、`Checkbox::observe_checked()` 等遵守 object-level signal/state 契约。
- 这些接口适合局部 widget 组合、对象级 smoke、非 SoA 小系统。
- 不能因为 object-level widget 有 `observe_*`，就要求 `SceneAccess` 镜像暴露同名 observe 面。

object-level widget 与 SoA `SceneAccess` 的区别见 [`../architecture/signal_state_contract_v0.md`](../architecture/signal_state_contract_v0.md)。

### Evidence and regression demo

Evidence Lab / regression demo 可以比产品代码更靠近内部，但必须声明测试目标。

允许：

- `Examples/ui/vivid/soa_demo` 使用 `charm.ui.vivid_internal` 验证 SoA / DrawCmd 内部回归。
- object-level widget smoke 直接 import `charm.widgets.*`，验证 widget-local truth / edge contract。
- 内部 DrawCmd regression 在目标明确时检查 command bytes、arena layout 或 executor grouping。

禁止：

- 把 demo-only import 反推成产品入口。
- 把 DrawCmd 私有 payload layout 写成产品 UI evidence。
- 把 helper 级 `observe_*` 误写成 scene/runtime 级自由订阅图。

## 当前使用面快照

本节记录 2026-06 首轮审计看到的现状，用于区分“合法例外”和“后续收敛候选”。

### Examples/ui/vivid

`Examples/ui/vivid` 不是单一产品入口，而是多类验证入口的集合。

分类：

- `component_*`、`focus_*`、`semantic_*`、`style_token_law_demo` 等多数 demo 走 `charm.ui.scene`，属于产品/runtime evidence 路径。
- `motion_time_demo`、`page_transition_demo`、`semantic_transition_demo`、`semantic_action_state_transition_demo` 显式 import `charm.ui.scene.motion_runtime`，属于 runtime spine 级验证路径；不要把 motion / page-transition 重新塞回 `charm.ui.vivid`。
- `soa_demo` 走 `charm.ui.vivid_internal`，属于 internal regression，允许验证 SoA / DrawCmd 内部行为，但不得反推为产品入口。
- `widget_signal_demo`、`widget_state_demo` 直接 import `charm.widgets.*`，属于 object-level widget smoke，合法冻结 widget-local truth / edge contract。
- `dropdown_popup_demo`、`menu_tree_demo` 直接使用 `SoaFactory` 和 helper widget，属于 SoA-backed helper contract smoke，合法但只说明 helper 局部语义成立。
- `fullframe_demo`、`tile_demo`、`text_demo`、`theme_demo` 是 host/demo 形态，可直接接触 canvas、framebuffer、font、SDL 等演示面；它们不是 MCU 产品入口模板。

审查规则：

- 不要把 `Examples/ui/vivid` 的所有 import 统一改成 `charm.ui.scene`。
- demo 直连内部模块时，README 或文件职责必须能说明测试目标。
- 新增 demo 如果只是产品 evidence，默认走 `Scene`；只有 internal regression 才走 `vivid_internal` / SoA / DrawCmd。

### Player / product pressure line

Player 是真实产品压力线，不等同于普通 demo。当前使用面分三类。

分类：

- `player_md3_runtime.cpp` 走 `charm.ui.scene`，`player_md3_diag_scene.cpp` 走 `charm.ui.scene.scene_evidence`，属于 runtime / diagnostic 合法路径。
- `player.ui_builder.cppm` 和 `player.scene_runtime.cppm` 通过 `charm.ui.scene` 使用 builder / layer surface，属于 `Scene` 附属 surface 的产品压力用法；不要重新引入对 `builder_support` 的直接 import。
- `win/main.ui_ci.object_tree.cpp` 直连 SoA、structured view 和 `charm.widgets.*`，属于 Windows host UI CI / object-tree regression，不是产品 UI 入口。
- `win/main.host_module.cppm` 直连 `charm.gfx.draw_cmd` / `snapshot`，属于 host preview / UI CI 边界，不应作为 MCU 产品路径复制。
- `player.ui.cppm` 直接 import 大量 `charm.widgets.*`，属于当前 Player MD3 组合压力面；后续若要收敛，应通过 Pattern / Scene builder API 渐进替换，而不是一次性删除 widget imports。

审查规则：

- Player 侧新增长期产品代码时，优先使用 `charm.ui.scene` / `charm.ui.vivid`。
- Player PRODUCT / MCU 路径不得直接 import `charm.gfx.snapshot`、`charm.gfx.host_tools` 或 `charm.font.provider_freetype`；这类能力必须先通过 product profile / host gate 说明。
- 新增 `builder_support` 直接 import 需要说明为什么不能通过 `charm.ui.scene` 获得所需 surface；默认应改走 `charm.ui.scene`。
- 新增 SoA / DrawCmd 直连只允许出现在 host CI、diagnostic 或 internal regression 中。
- `player.ui.cppm` 的 widget imports 是迁移债，不是立即阻断项；新增类似扩散应优先进入审查清单。

## 命名债说明

Vivid 当前存在 `charm.core.*` 模块名，例如：

- `charm.core.geometry`
- `charm.core.style`
- `charm.core.soa_kernel`
- `charm.core.soa_payload`

其中一部分是 Vivid UI-domain core，不等同于 Charm 全局 Foundation 层。

审查时按实际依赖和入口语义判断，不要仅凭 `charm.core` 前缀认定其属于 Foundation。后续若做命名收敛，应先建立兼容层和迁移计划，不应直接批量改名。

## Review Rules

- 产品代码新增 import 时，优先选择 `charm.ui.vivid` 或 `charm.ui.scene`；motion、font runtime、host tools 必须显式 import 对应扩展入口。
- 新增对 `charm.ui.vivid_internal`、SoA kernel、DrawCmd partition 的依赖，必须说明它是 internal regression 还是 Vivid 内部实现。
- 新增对 `charm.gfx.host_tools`、`charm.gfx.snapshot`、`charm.font.provider_freetype` 的依赖，必须说明它是 host-only、diagnostic 还是经过 PRODUCT profile admission。
- 新增 evidence 只能依赖 scene-level facts，除非测试目标明确是内部 DrawCmd / SoA regression。
- 新增 object-level widget 观察接口，不得自动扩散到 `SceneAccess`。
- 涉及 `FULL` / `MCU_MIN` / `PRODUCT` 裁剪时，必须确认被 import 的模块在对应 featureset 中存在。

## Verification

影响 import 边界、公共入口或裁剪规则时，至少验证：

- `Examples/ui/vivid/soa_demo` 的 `--soa-ci --regress-ui` 路径。
- `scripts/vivid_evidence_lab_manifest_smoke.ps1`。
- 涉及 semantic / focus / transition 的改动，补跑对应 demo 的 CTest final verdict。
- 涉及 CMake / Modules 裁剪的改动，分别检查 `FULL`、`MCU_MIN`、`PRODUCT` featureset。
- `scripts/vivid_product_profile_compiler_smoke.ps1` 的 profile/envelope 正例与配置期负例。
- `scripts/vivid_static_memory_admission_smoke.ps1` 的 base/debug/base profile 切换与当前 closure/stack source 证据。
