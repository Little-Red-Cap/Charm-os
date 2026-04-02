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

### 6. 文本语义与 Text Recipe

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
4. 封面视觉样式 spec
5. 列表卡片头布局 helper
6. 页面级 builder 组合器

理由：
- 先上收低语义、高复用的模式
- 避免一开始就把业务概念塞进 `Vivid`

## 下一步建议

比较合适的下一步是二选一：

- 方案 A：继续在 Player 内做第二轮“共性收敛”，把顶栏、chip、path bar 再统一一层
- 方案 B：开始在 `Modules/ui/vivid` 里试着落一个非常轻的公共 helper，只接纳最稳定的一类模式

如果目标是“让 Player 推动 Vivid 演进”，建议先走方案 A，再走方案 B。
