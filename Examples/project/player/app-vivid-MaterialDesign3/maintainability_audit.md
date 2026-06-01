# Player MD3 可维护性审计与 Vivid 上收评估

本文件是 `app-vivid-MaterialDesign3` 的工程接手审计，不替代
`design_notes.md`。`design_notes.md` 负责 PixelPlayer 对齐与设计要求；
本文件负责代码职责、维护风险、Vivid 能力沉淀边界和下一轮低风险整理切片。

当前阶段结论：先整理 Player 层的 page-local spec 与 builder/helper 边界，再选择少量跨产品稳定模式上收 Vivid。不要为了“看起来更框架化”把 PixelPlayer 特定参数、播放业务语义或 H747 降级策略提前塞进 Vivid。

## 1. 审计边界

本轮只覆盖 shared Player MD3 与 Vivid 可沉淀能力评估：

- 覆盖：`PlayerController`、Now Playing builder、Library builder、cover/theme、Windows UI CI、已有 Vivid scene helper。
- 不覆盖：`Backends/`、`Modules/platform/README.md`、`Modules/ui/vivid/gfx/draw_cmd*` 当前未归属工作区改动。
- 不做：视觉改版、性能优化、大重构、新 widget admission、H747 专用 UI 分叉。

可移植性护栏继续保持：

- H747 默认 `PRODUCT + StaticCut + BuiltinOnly + ResourceProviderOnly`。
- H747 禁止 file-font/debug/FreeType/layered/snapshot/dynamic cover-theme 回流。
- SDRAM 只允许运行时显式 carve 的资源缓存，不承载核心静态状态。

## 2. 当前维护压力点

| 区域 | 当前判断 | 维护风险 | 建议处理 |
| --- | --- | --- | --- |
| `player.controller.library.inc` | 单文件体量最大，Library 排序、上下文、行 recipe、选择同步混在一起 | 后续修列表视觉时容易碰业务状态 | 先拆职责注释和小 helper，不改语义 |
| `player.controller.now_playing.inc` | 状态同步、seek、transition、主题应用集中 | Now Playing 视觉迭代容易误伤播放状态或 StaticCut gate | 把 transition-only 与 progress contract 继续隔离 |
| `player.ui_builder.shared.inc` | `UiLayout/HomeLayout/NowPlayingLayout/LibraryLayout` 全部集中 | page-local spec 已成型，但同文件过大，查找成本高 | 下一轮优先拆成 page spec include |
| `player.ui_builder.now_playing.inc` | 已有 `NowPlayingBuildMetrics`，但构建、spec、style patch 仍在同一层 | 再调视觉时容易回到魔法数阶段 | 抽出 Now Playing page-local spec 与纯几何函数 |
| `player.ui_builder.library.inc` | 手工 Rect、渐变表面、tab/action/list card 组合较多 | Library 后续视觉恢复会变成局部试错 | 先收敛成 Library surface/tool/list-card helper |
| `player.cover.cppm` | host decode detail 已关进 seam，但文件仍同时承载 fallback、resource、host decode | 可读性一般，但边界是正确的 | 暂不重构，避免破坏 cover seam |
| `player.cover_theme.cppm` | fallback/theme extraction 策略集中 | 主题策略和取样实现容易混淆 | 只保留 Player 层，不上收 Vivid |
| Windows UI CI | Now Playing case 已丰富，但 case 分组继续膨胀 | 新增视觉状态时查找成本升高 | 后续按页面/行为整理 include，不改断言语义 |

## 3. 文件职责边界

当前应保持的边界：

- `player.ui_builder.*` 负责静态页面结构、控件创建、page-local 几何和 style patch 组装。
- `player.controller.*` 负责业务状态、输入处理、runtime 同步、文本/封面/播放状态刷新。
- `player.cover*` 负责封面解析、fallback、host-only decode 和 Player cover theme，不暴露动态图像对象给 controller。
- `player.ui.cppm` 负责 icon 注册、theme/style 初始化、产品视觉 token，不承担平台后端职责。
- Windows `main.ui_ci.*` 负责 host 验证和截图证据，不应成为产品逻辑来源。

当前最容易混淆的边界：

- 页面几何与视觉 token 混在 builder include 中，建议先抽 `*_layout_spec.inc` 风格的 page-local spec。
- Now Playing transition 几何仍有少量常量复制，虽然已被 `CHARM_PLAYER_LAYERED_TRANSITIONS` 隔离，但后续应尽量复用同一几何 spec。
- UI CI 里会直接访问 controller handles，这是可接受的 host 验证 seam，但不能反向驱动产品实现。

## 4. Vivid 上收判定规则

只有同时满足以下条件，才建议从 Player 上收到 Vivid：

- 跨产品可复用，不包含 Player 播放、媒体库、cover resolve、PixelPlayer 专用语义。
- MCU-clean：不需要 `std::vector/std::string`、异常、host filesystem、截图、动态解码。
- 不扩大 H747 PRODUCT 默认模块集，或能通过显式 PRODUCT profile admission 解释。
- 能减少页面手工 Rect/spec 重复，而不是制造第二套 layout/style 系统。
- API 是轻量 helper 或组合 pattern，优先 `Spec + Handles + build/show/hide/update`，不优先新增重 widget。

不应上收的内容：

- PixelPlayer 特定尺寸、颜色、阴影、渐变、按钮形状。
- Player 播放控制语义、seek commit 策略、媒体库排序/分组、cover fallback 选择。
- H747 默认降级策略、runtime policy、memory evidence gate。

## 5. 首轮 Vivid 候选评估

| 候选 | 结论 | 原因 | 建议下一步 |
| --- | --- | --- | --- |
| 稳定纵向页面区块布局 | 后续观察 | Home/Now/Library 都有纵向节奏，但每页差异仍大 | 先在 Player 内抽 page-local spec，等第二个 app 复用再上收 |
| Cover/Image Hero 容器 | 后续观察 | cover stage 是通用模式，但 Player fallback/cover theme 强业务化 | 只评估通用 `ImageHeroSpec`，不要带 cover resolve |
| Title/Subtitle 文本块 | 上收 | 纯排版组合，跨页面复用明显，已有 `TextStyleSpec` 基础 | 可规划 Vivid `TitleBlockSpec` helper，保持无业务状态 |
| Seekbar 组合 pattern | 后续观察 | Now Playing 已形成产品级 pattern，但仍依赖播放 duration/seek 状态 | 先保留 Player，若再出现第二个 seek 场景再做 Vivid `SeekBar` admission |
| Mini-player / persistent bottom bar | 暂留 Player | 播放状态、封面、当前曲目同步强业务化 | 只沉淀底部 persistent surface 的 layout 经验，不抽业务组件 |
| Anchored action menu | 上收已完成 | Vivid 已有 `anchored_menu` helper，适合跨产品复用 | Player 继续消费，不复制菜单算法 |
| Scene stats evidence helper | 上收 | `Scene::last_cmd_stats/last_exec_stats/layer_stats` 已是框架事实 | 可补轻量 evidence formatter，避免每个 app 重写统计输出 |
| Home content card family | 后续观察 | Home 卡片暴露真实能力缺口，但目前仍是 Player 探针 | 先整理 Player card spec，避免过早定 Vivid card API |
| Library expressive list row | 后续观察 | 列表项结构有跨产品潜力，但当前仍被媒体库语义牵引 | 先明确封面/主信息/辅助/尾部四区 recipe，再判断是否上收 |
| Page transition/motion | 暂留 Vivid host/FULL | 已在 Vivid，但 H747 StaticCut 不应纳入默认 PRODUCT | 不为 Player H747 恢复 motion 模块 |

## 6. 下一轮低风险整理切片

### 切片 A：拆 page-local layout spec

目标：把 `UiLayout/HomeLayout/NowPlayingLayout/LibraryLayout` 和纯几何计算从 shared builder 大文件中分离为页面局部 spec include。

约束：

- 不改变任何矩形、样式、控件树和 handle 名称。
- 不新增 Vivid API，不新增 PRODUCT payload。
- Windows UI CI 断言结果必须完全保持。

Gate：

- Windows 构建 `charm-player-win-vivid-md3` 并运行 `--ui-ci`。
- H747 构建 `h747_lab_player_md3 -j 1`，刷新 memory evidence。

### 切片 B：整理 Now Playing ProgressSection helper

目标：把 progress section 的 `Spec -> visual/hit/time row -> style patch` 固化成 Player 内部 helper，减少 seekbar 后续视觉迭代的魔法数扩散。

约束：

- 不新增专用 `SeekBar` widget。
- 不改变暂停 seek、drag preview、commit/cancel/no-duration contract。
- 不引入 layered transition 或 host-only 分支。

Gate：

- Windows `now_playing_seek_*`、`now_playing_closure_*` 全绿。
- H747 forbidden module hits 继续为 0。

### 切片 C：UI CI 分组清理

目标：按页面和行为把 Windows UI CI include 分组说明补清楚，降低后续新增视觉 case 的成本。

约束：

- 不改变 case 名称和断言语义。
- 不把 UI CI helper 引入 shared Player runtime。
- 不生成或提交截图产物，除非后续明确要求。

Gate：

- Windows `--ui-ci` 输出 case 仍覆盖 Home / Now Playing / Library / portability / layer host preview。
- 文档同步说明新增 case 应放入哪个分组。

## 7. 推荐顺序

推荐先做切片 A，再做切片 B，最后做切片 C。

原因：

- 切片 A 先降低大文件查找成本，为后续视觉恢复建立稳定落点。
- 切片 B 直接服务 Now Playing 后续维护，且不需要新增 Vivid widget。
- 切片 C 是验证体系清理，适合在结构稳定后补做。

暂不建议立刻做：

- 把 Seekbar 直接上收成 Vivid widget。
- 把 Home card family 直接上收 Vivid。
- 拆 `player.cover.cppm` host decode 内部实现。
- 触碰 `draw_cmd*` 做性能统计或渲染优化。

## 8. 接手检查清单

后续任何 Player MD3 UI 改动前，先检查：

- 是否符合 `design_notes.md`，尤其 PixelPlayer 参考不能被主观重阴影、重渐变、glass、重描边覆盖。
- 是否只依赖 `PlayerUiPolicy/PlayerUiResources/runtime config`，没有直接绑定 SDL、H747 BSP 或具体 framebuffer。
- 是否需要新增 style slot、payload cap、widget module；如果需要，必须同步 PRODUCT profile 和 evidence。
- 是否保持 Windows UI CI 与 H747 memory evidence 双线验证。
- 是否避免触碰用户未归属工作区。
