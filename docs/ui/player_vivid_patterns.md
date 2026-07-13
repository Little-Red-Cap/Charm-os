# Player 到 Vivid 的 Pattern 边界

## 文档状态

- `status`: `exploration`
- `scope`: Player UI 模式的本地归属、Vivid helper 现状与 promotion 条件
- `authority`: Player/Vivid source、CMake source closure 与对应测试

本文不定义 Player 视觉设计，也不把单个页面的复用自动解释为 Vivid 公共能力。当前页面事实从
`Examples/project/player/app-vivid-MaterialDesign3` 核对。

## Promotion 原则

一个 Player 模式进入 Vivid 前必须同时满足：

1. 至少两个独立页面族或产品 consumer 使用相同语义，而不只是相似外观；
2. 输入、输出、容量、失败和 ownership 可以脱离 Player 业务命名表达；
3. 不依赖 SDL、H747、文件路径、播放状态或具体页面 handle；
4. helper 能减少真实重复组合，而不是只把参数换成更长的 spec；
5. 有成功、边界和失败/降级测试，并证明固定容量与 backend 限制；
6. 迁移后删除旧页面拼装，避免公共 helper 与私有实现长期并存。

第二个 consumer 只能证明继续评估的必要性，不能单独授予公共身份。

## 已进入 Vivid Source 的 Helper

| 领域 | Source | 稳定边界 |
|---|---|---|
| Page header rect | [`page_header_layout.cppm`](../../Modules/ui/vivid/core/page_header_layout.cppm) | 单个 header 元素的矩形计算 |
| Top bar composition | [`top_bar_layout.cppm`](../../Modules/ui/vivid/core/top_bar_layout.cppm) | left/title/right 组合布局，不创建产品按钮 |
| Pill content layout | [`pill_layout.cppm`](../../Modules/ui/vivid/core/pill_layout.cppm) | icon/text/content 的几何关系 |
| Pill surface style | [`pill_surface.cppm`](../../Modules/ui/vivid/core/pill_surface.cppm) | surface patch，不定义 Tab、Path 或 Shuffle 业务 |
| List card header | [`list_card_header_layout.cppm`](../../Modules/ui/vivid/core/list_card_header_layout.cppm) | header 几何，不拥有列表数据或排序命令 |
| Path bar layout | [`path_bar_layout.cppm`](../../Modules/ui/vivid/core/path_bar_layout.cppm) | bar/icon/label 组合，不拥有文件路径语义 |
| Text style patch | [`text_style.cppm`](../../Modules/ui/vivid/core/text_style.cppm) | color/font role/weight/explicit font 的 patch 优先级 |
| Seek bar style | [`seek_bar_style.cppm`](../../Modules/ui/vivid/core/seek_bar_style.cppm) | track/fill/padding/radius 样式，不定义 seek 交互 |

这些文件的存在只证明 helper 已进入 source。是否属于推荐 API、有哪些 consumer 和测试，仍需检查当前
imports、CMake 与 evidence。

## 仍属 Player 的模式

| 模式 | 保留在 Player 的原因 |
|---|---|
| `HomeLayout`、`NowPlayingLayout`、`LibraryLayout` | 页面专属结构和产品信息密度，不是通用 device layout |
| Hero title/subtitle 与 Player text role | 含播放器页面语义和产品字号策略 |
| Home/Now Playing cover composition | 圆形拼贴、卡片、主题提色与资源 fallback 尚未形成跨产品语义 |
| EQ、Volume、Clip 等 panel | 业务命令、状态和交互归 Player domain |
| Library card/body/path 组合 | 数据、排序、浏览和 empty state 属产品页面 |
| 专辑提色与主题应用 | 资源解码、颜色策略和产品 fallback 仍由 Player 拥有 |

页面可以消费 Vivid helper，但不得因此把完整页面结构或业务 widget 上收。

## Surface 与 Gradient

两色、固定方向、圆角裁剪的线性渐变属于 surface/render capability，不是独立产品 pattern。增加多 stop、
任意角度、radial/conical 或 shader/cache 系统前，需要独立 consumer、内存/执行成本和 backend 降级证据。

页面使用渐变不能证明需要 `GradientWidget` 或全局 gradient descriptor。

## Text 与 Style

Vivid 可以提供生成 style patch 的机制；Player 继续拥有 title/body/meta 等产品语义映射。公共 text/style
helper 必须保持：

- explicit font、font role 与 default 的优先级明确；
- color 与 metrics 影响可分别观察；
- backend 字体对象和产品文案不进入 helper；
- style helper 不暗含 layout、focus 或交互语义。

相关证据边界见 [`vivid_style_token_law_v0.md`](vivid_style_token_law_v0.md)。

## Layout 与 Builder

layout helper 计算几何，builder 创建对象并绑定生命周期，两者不能因命名相近而合并。只有页面反复
手工创建同一对象图、事件绑定和失败收尾时，才评估 builder；纯矩形重复优先保持轻量 layout spec。

Builder 不得隐藏 pool exhaustion、handle invalid、text/resource capacity 或 partial construction rollback。

## 验证要求

每个上收候选至少记录：

- 当前两个以上 consumer 的 source/import；
- Player 私有字段如何被移除或参数化；
- fixed-capacity、invalid input 和 allocation failure；
- layout/style evidence 与 product screenshot/UI CI 的覆盖差异；
- Host 与 MCU/real-board 降级是否共享语义；
- 旧 helper 的删除或兼容退出条件。

USB、存储或其它子系统的推进方法不能作为 UI pattern 的准入证据。公共化只由实际 UI consumer、行为
边界和测试证明。

## 非目标

- 不在本文维护 Player 页面完成度、视觉参数或下一步排期。
- 不把产品命名改成通用名后直接迁入 Vivid。
- 不为一个示例建立重型 schema、generator 或 builder hierarchy。
- 不让公共 pattern 选择 backend、snapshot、device profile 或产品资源路径。
