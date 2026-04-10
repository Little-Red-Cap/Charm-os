# Player Vivid 模式沉淀

本页总结 `Examples/project/player/app-vivid-MaterialDesign3` 在推进过程中形成的稳定 UI 模式。

目标不是描述某一页长什么样，而是识别哪些模式已经跨页面复用、值得继续上收回 `Vivid`。

## 现状

当前 Player 的三大主屏：
- `Home`
- `Now Playing`
- `Library`

已经完成一轮“页面级 Token 化”：
- 字体链路稳定化
- 页面布局参数集中到共享层
- 顶栏、封面、列表卡片等模式开始收敛

相关代码入口：
- `Examples/project/player/app-vivid-MaterialDesign3/player.ui_builder.shared.inc`
- `Examples/project/player/app-vivid-MaterialDesign3/player.ui_builder.home.inc`
- `Examples/project/player/app-vivid-MaterialDesign3/player.ui_builder.now_playing.inc`
- `Examples/project/player/app-vivid-MaterialDesign3/player.ui_builder.library.inc`

## 已稳定的模式

### 1. 页面级布局 Token

当前已经存在页面级布局结构：
- `HomeLayout`
- `NowPlayingLayout`
- `LibraryLayout`

它们的价值：
- 避免页面内部散落魔法数
- 让视觉调参收敛到单点
- 为后续做尺寸缩放、主题变体、设备适配留接口

建议后续方向：
- 在 Player 内继续保留“页面专属 Token”
- 对真正跨页面的参数，再继续提升到更通用层

### 2. 顶栏模式

三页都存在顶栏变体：
- 左返回 / 标题 / 右操作
- 无标题仅右侧按钮组
- 单标题 + 单操作按钮

当前已抽出轻量共享助手：
- `page_header_right_rect(...)`
- `page_header_title_rect(...)`
- `create_top_icon_button(...)`

当前已开始上收为 `Vivid` 公共 helper：
- `Modules/ui/vivid/core/page_header_layout.cppm`
- `charm.ui.scene.page_header`
- `Modules/ui/vivid/core/top_bar_layout.cppm`
- `charm.ui.scene.top_bar`
- `Modules/ui/vivid/core/pill_layout.cppm`
- `charm.ui.scene.pill`
- `Modules/ui/vivid/core/list_card_header_layout.cppm`
- `charm.ui.scene.list_card_header`
- `Modules/ui/vivid/core/pill_surface.cppm`
- `charm.ui.scene.pill_surface`

其中 `pill_surface` 已经开始承接：
- 顶栏按钮表面
- Tab 基础表面
- InfoTag 表面
- 路径条背景
- 激活 Tab 的阴影/内描边变体
- Shuffle 按钮的阴影变体

说明：
- 这说明顶栏已经不只是某页私有布局，而是“同一种模式的不同配置”
- `TopBarLayout` 已经覆盖 Player 的三种主变体：
  - `Home`：无标题，仅右侧双按钮
  - `Library`：标题 + 右侧单按钮
  - `Now Playing`：左返回 + 标题 + 右侧双控件

这一轮的演进可以理解为：
- 第一阶段：`page_header_*_rect(...)` 只提供单个矩形计算
- 第二阶段：`TopBarLayout` 把一整个顶栏变体需要的关键矩形一起产出

这一步的价值：
- 页面侧不再重复推导左右按钮和标题的相对关系
- 顶栏变体开始具备“组合 spec”意义，而不再只是若干零散 helper
- 后续如果继续演进到 `TopBarBuilder`，也已经有了稳定的中间层

建议上收方向：
- 当前先保持 `TopBarLayout` 这一层轻量、稳定、可配置
- 等第二个真实页面族也复用后，再考虑是否继续形成 `TopBarBuilder` 或 `PageHeaderBuilder`

### 3. 封面卡片模式

Player 里已经出现两类封面样式：
- Home 页的圆形拼贴封面
- Now Playing 页的大封面 + 可选拼贴小封面

当前已有共享助手：
- `apply_home_cover_style(...)`
- `apply_now_cover_style(...)`

说明：
- 二者还没有完全统一，因为 Home 偏圆形拼贴，Now Playing 偏圆角卡片
- 但它们已经说明“封面视觉样式”值得被当作一类 UI Pattern 对待

建议上收方向：
- 形成 `CoverStyleSpec`
- 支持 `corner_radius / shadow / collage offsets / emphasis level`

### 4. 胶囊按钮与标签条模式

Player 内部已经多次出现：
- Tab 胶囊
- Shuffle 胶囊按钮
- 路径条背景
- 信息标签 `info_tag`

这些模式的共同点：
- 圆角矩形
- 轻边框
- 深色背景上的低对比层次
- 文本或图标作为主体内容

建议上收方向：
- 可以沉淀出一类 `PillSurface` / `ChipSurface` 样式模式
- 先以 style helper 的形式存在，再决定是否做独立 widget

### 5. 列表卡片头模式

`Library` 页已经形成了一种稳定结构：
- 卡片容器
- 标题
- 排序按钮
- 路径条
- 列表主体
- 底部提示文本

这类模式的价值在于：
- 很适合作为数据列表页面的标准母版
- 后面如果做 Search / Album / Artist 页面，可以直接复用结构

建议上收方向：
- 先保留在 Player 内作为 `ListCardHeader` 模式
- 等第二个页面也复用后，再考虑变为 `Vivid` 通用布局助手

### 6. Path Bar 组合层

`Library` 页的路径条最初是由多个已存在 helper 组合出来的：
- 背景走 `pill_surface`
- 图标和文本位置走 `pill_layout`
- 文本样式走 `MetaText`

这说明它一开始确实可以被看成“胶囊模式的一种特例”。

但在实际推进中，它已经表现出独立组合层特征：
- 需要明确的 `bar / icon / label` 三段矩形
- 语义上更接近“路径条”而不是一般按钮或 chip
- 后续很可能会被搜索页、文件页、浏览页复用

当前已上收的公共 helper：
- `Modules/ui/vivid/core/path_bar_layout.cppm`
- `charm.ui.scene.path_bar`

当前公共 helper 提供：
- `PathBarSpec`
- `PathBarLayout`
- `make_path_bar_layout(...)`

这一轮的演进可以理解为：
- 第一阶段：路径条复用 `pill_layout` 的内容布局能力
- 第二阶段：路径条作为独立 pattern，拥有自己的组合层 helper

这一步的价值：
- 页面侧不再把“路径条”当成泛化的 pill 手工拼接
- `PathBar` 开始具备明确 pattern 身份，便于后续扩展更多浏览类页面
- 同时仍保持轻量，不急着过早做独立 widget

建议上收方向：
- 当前先保持 `PathBarLayout` 这一层轻量 helper
- 如果未来出现第二个真实页面复用，再考虑是否继续增加 `PathBarBuilder` 或状态变体 helper

### 7. 文本语义与 Text Recipe

这一轮推进后，Player 的文本样式已经不再直接散落 `FontId / FontWeight / 显式字号`。

当前结构分成两层：
- `Vivid` 提供通用文本样式 helper
- `Player` 提供页面级文本语义映射

当前已上收的公共 helper：
- `Modules/ui/vivid/core/text_style.cppm`
- `charm.ui.scene.text_style`

当前公共 helper 提供：
- `TextStyleSpec`
- `make_text_style_patch(...)`

它解决的问题：
- 统一“文本颜色 + font role + font weight + 可选显式字体”的 patch 生成
- 明确了 `explicit font > font_role > default` 的优先级落点
- 避免每个页面都手写一遍透明背景、零边框、零 padding 的文本 patch

Player 当前保留的页面级语义包括：
- `HeroTitle`
- `HeroSubtitle`
- `PageTitle`
- `PageSubtitle`
- `CardTitle`
- `CardBody`
- `MetaText`

其中更适合继续留在 Player 的：
- `HeroTitle`
- `HeroSubtitle`

原因：
- 这类语义仍明显带有播放器页面特征
- 其中 `HeroTitle` 还绑定了显式大字号逃逸层

更适合继续上收到 `Vivid` 的：
- 文本 patch 的通用生成逻辑
- 页面内常见的 `title / subtitle / body / meta` 配方能力

建议后续方向：
- 暂时不要把 `PlayerTextRole` 直接搬进 `Vivid`
- 先让更多页面通过 `TextStyleSpec` 组合出自己的语义层
- 当第二个真实项目也出现稳定的 `title / subtitle / meta` 分层后，再考虑上收更高层 typography recipe

### 8. 最小线性渐变背景能力

这一轮推进后，`Vivid` 已经补上了一类非常克制但实用的背景能力：
- 两色线性渐变
- 方向目前只支持 `vertical / horizontal`
- 支持圆角裁剪
- 当前优先服务 `Container` 类背景

这项能力的落点不是“为了做炫技渐变”，而是为了让 `Player` 的 `Library` 页面这类真实场景不再只能用“多层半透明色块近似”。

当前这版能力适合处理的场景：
- 页面背景的上下层次渐变
- 大容器 / 面板 / Hero 背景的轻渐变过渡
- 不需要复杂 stop 的主题化底色

当前明确不做的范围：
- 多 stop 渐变
- 任意角度线性渐变
- radial / conical gradient
- 渐变描边
- 通用 shader / 缓存系统

为什么先只做这一版：
- `arm-2d` 一类 MCU 场景实践说明，固定方向的最小渐变已经很有价值
- `lvgl` 那类完整 gradient descriptor 虽然更强，但对当前阶段来说偏重
- `Player` 的 `Library` 背景已经证明：两色、固定方向、圆角裁剪，这个组合就足够解决第一批真实问题

建议上收方向：
- 目前先把它视为 `surface background capability`，不是独立 widget
- 先继续观察 `Library`、未来 `Home Hero`、信息条背景是否稳定复用
- 等第二个以上真实 pattern 复用后，再决定是否增加更高层的 `GradientSurfaceSpec`

### 9. SeekBar 样式 Helper

在 `Now Playing` 的第二轮推进中，一个新的事实变得清楚：
- 仅靠页面里零散的 `padding / bg / border / accent / radius` patch
- 很难稳定表达“播放器 seekbar”这种更强语义的视觉身份

这说明它已经不再只是一个页面内微调问题，而值得抽出一层轻量 recipe。

当前已上收的公共 helper：
- `Modules/ui/vivid/core/seek_bar_style.cppm`
- `charm.ui.scene.seek_bar_style`

当前公共 helper 提供：
- `SeekBarStyleSpec`
- `make_seek_bar_style_patch(...)`

它当前解决的问题：
- 统一 seekbar 的 `bg / track / fill / padding / radius` 这组基础样式输入
- 避免每个页面都重复手写同一套 patch 字段
- 让“播放器进度条样式”第一次具备了轻量但明确的公共入口

当前这层 helper 的边界：
- 它还是 style helper，不是独立 widget
- 它不定义交互模型，只定义视觉基础规则
- 它适合先服务 `Now Playing`，未来再观察是否会被第二个真实页面复用

这一步的意义：
- `ProgressSection` 不再只是页面内一组散装 patch
- `Vivid` 开始拥有一种更接近媒体类控件的中层样式语言
- 后续如果要继续演进专属 `SeekBar` 组件，这层 helper 可以作为稳定过渡层

## 暂不建议直接上收的部分

以下内容目前仍更偏 Player 私有：
- 具体页面文案
- 音乐播放器专属控件语义
- `HeroTitle / HeroSubtitle` 这类页面级文本命名
- EQ / Volume / Clip 这类业务面板布局
- 专辑封面提取色和主题应用逻辑

原因：
- 这些内容还没有跨场景复用证据
- 现在直接抽象，容易变成“为一个示例而设计的通用层”

## 建议的上收顺序

优先级建议：

1. 顶栏布局助手
2. 胶囊/Chip 类 surface helper
3. 通用文本样式 helper
4. 路径条组合层 helper
5. 封面视觉样式 spec
6. 列表卡片头布局 helper
7. 页面级 builder 组合器

理由：
- 先上收低语义、高复用的模式
- 避免一开始就把业务概念塞进 `Vivid`

## 下一步建议

比较合适的下一步是二选一：

- 方案 A：继续在 Player 内做第二轮“共性收敛”，把顶栏、chip、path bar 再统一一层
- 方案 B：开始在 `Modules/ui/vivid` 里试着落一个非常轻的公共 helper，只接纳最稳定的一类模式

如果目标是“让 Player 推动 Vivid 演进”，建议先走方案 A，再走方案 B。

## 参考：来自 USB 推进的共通方法

`Draft/USB推进沟通` 虽然讨论的是 USB，不是 UI，但里面有几条方法论与 `Player -> Vivid` 的推进高度同构，值得记录下来。

### 1. 先做声明式规格 + 运行时装配，不先做重型生成器

USB 那边最有价值的一条判断是：
- 先做 `spec + runtime`
- 不要一开始就做“大而全 generator”

这和 `Vivid` 当前的推进方式是相通的：
- 先在真实页面里验证 `TopBarLayout / PathBarLayout / TextStyleSpec`
- 再把稳定模式上收到公共层
- 而不是一上来就设计一套很重的页面生成框架

这条原则的意义：
- 先吃真实运行链路
- 先暴露真实约束和缺口
- 避免在抽象还不稳定时就固化过早的高层接口

### 2. 规格层不泄漏后端语义

USB 那边强调：
- `spec` 里不应该出现 `DCD / HAL / endpoint callback` 这类板级语义

对应到 `Vivid`：
- 页面级 pattern / spec 不应该直接暴露 `SoaGui / draw_cmd / SDL` 语义
- 页面作者应该描述“顶栏 / 路径条 / 文本层级 / 封面样式”
- 而不是直接关心绘制命令和后端细节

这条原则可以继续指导 `Vivid` 的中层演进：
- `Pattern / Layout / Text Recipe` 应该站在页面语义一侧
- 渲染链和平台链仍留在运行时与底层实现中

### 3. 专家能力应是局部覆写，不是整体逃逸

USB 那边对 expert hook 的判断很准确：
- 需要保留专家能力
- 但应该是局部覆写，而不是“一旦特殊就整套系统失效，回到裸写”

对应到当前 UI：
- 默认走 `TextStyleSpec / TopBarLayout / PathBarLayout / PillSurface`
- 特殊场景再用 explicit override
- 不应该一有特殊需求就整页退回到裸 `Rect + StylePatch` 拼装

这条原则已经被字体链路问题验证过：
- `font_role` 提供主路径
- `explicit font` 提供逃逸层
- 但逃逸层仍留在系统内部，而不是让页面全面失去模式语义

### 4. 组合关系应由框架统一处理

USB 那边强调 composite 设备的 `interface / endpoint` 分配必须由框架统一管理。

在 UI 里，这对应的是：
- 顶栏内部左右按钮和标题关系
- 路径条内部 `bar / icon / label` 关系
- 列表卡片头内部标题、排序、路径条、列表主体关系

如果这些组合关系仍然要求页面作者手工推导所有关键矩形，那么 pattern 层就没有真正成立。

这也是为什么我们当前优先上收的是：
- `TopBarLayout`
- `PathBarLayout`
- `ListCardHeaderLayout`

它们的本质不是“少写几个数”，而是让组合关系成为框架能力。

### 5. 真正的价值在中间表示，而不只是最终 API

USB 沟通里最值得保留的长期视角是：
- `spec -> IR -> runtime`

对 `Vivid` 而言，这不是当前立刻要做的事，但它提供了一个很重要的未来方向：
- 页面 spec
- pattern/layout IR
- 运行时渲染与交互装配

一旦未来 `Vivid` 进入更复杂的页面组合、约束检查、多后端渲染阶段，这种分层会非常有价值。

当前不急着做 `IR` 的原因：
- 真实页面模式还在收敛阶段
- 先把 pattern/helper 站稳更重要

但这个视角值得保留，因为它解释了为什么：
- 运行时验证不能跳过
- 高层 DSL 不能先于中层语义稳定

### 6. 对当前 UI 推进的直接启发

从这份 USB 沟通里，可以提炼出几条继续适用于 `Player -> Vivid` 的工作纪律：

- 先真实项目，后通用抽象
- 先运行时装配，后生成器/DSL
- 先中层 pattern，后大而全框架
- 先局部 expert override，后整体逃逸
- 先把组合关系交给框架，后谈页面自由度

换句话说，`USB` 和 `UI` 虽然领域不同，但这份沟通再次验证了一个共同原则：

- 真正可演进的系统，不是靠一开始设计得很大很完整
- 而是靠真实场景反复逼出稳定的中层语义，再逐步上收
