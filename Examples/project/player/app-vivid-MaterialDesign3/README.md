# Player UI - Material Design 3

本目录是 Player 的 **Material Design 3 / PixelPlayer 方向 UI 变体**。

目标不是单纯做一个“更好看”的播放器皮肤，而是：
- 用真实项目页面推动 `Vivid` 子系统演进
- 在 `Player` 中验证组合层、文本语义、样式 helper 是否足够稳定
- 把真正可复用的模式逐步上收回 `Modules/ui/vivid`

---

## 1. 目录定位

这一套 UI 的职责是：
- 替换界面与视觉语言
- 验证页面结构、交互节奏和设计 token
- 不改动 `app-common` 的后端 / 文件系统 / 播放主逻辑

当前模块仍复用：
- `player.ui`
- `player.ui_builder`
- `player.controller`

通过独立 target 接入 Windows 入口，不与旧 UI 变体混用。

---

## 2. 当前协作原则

后续所有 UI 推进，优先遵守以下原则：

### 2.1 先有要求，再改样式
- 不再通过“图标大一点、颜色深一点、圆角再多一点”式反复试错推进
- 先更新设计要求，再改代码

### 2.2 先找已有组合层，再做页面拼接
- 顶栏优先走 `TopBarLayout`
- 路径条优先走 `PathBarLayout`
- 文本 patch 优先走 `TextStyleSpec`
- 不再新增第二套同类算法

### 2.3 先在 Player 验证，再上收回 Vivid
- 页面内先证明模式稳定
- 确认跨页面复用成立后，再上收回 `Vivid`

---

## 3. 两份核心文档如何分工

### `README.md`

本文件负责：
- 说明目录定位
- 说明协作原则
- 说明当前已沉淀的关键模式
- 告诉后续开发者应该先看什么

本文件 **不负责**：
- 写完整设计规范正文
- 记录所有视觉要求细节
- 承担验收清单主文档职责

### `design_notes.md`

`design_notes.md` 负责：
- 记录可执行设计要求清单
- 定义页面目标、视觉 token、排版要求、组合层要求
- 记录从 `Draft/PixelPlayer` 提取出的可执行参考
- 作为 UI 调整时的主要验收依据

一句话概括：
- `README` = 协作入口
- `design_notes` = 设计规范与验收标准

---

## 4. 当前已验证的关键演进成果

当前这套 UI 已经不只是页面代码，而是 `Player -> Vivid` 演进样板。

### 4.1 已在 Player 中验证并上收到 Vivid 的 helper
- `page_header_layout`
- `top_bar_layout`
- `pill_layout`
- `pill_surface`
- `path_bar_layout`
- `list_card_header_layout`
- `text_style`

### 4.2 已在 Player 内稳定的页面语义层
- `PlayerTextRole`
- `HomeLayout`
- `NowPlayingLayout`
- `LibraryLayout`

### 4.3 已经统一的页面模式
- 三大主屏顶栏统一到 `TopBarLayout`
- `Library` 路径条统一到 `PathBarLayout`
- 文本 patch 统一通过 `TextStyleSpec` 生成

---

## 5. 当前最应该关注的文件

### 页面与样式
- `player.ui.cppm`
- `player.ui_builder.cppm`
- `player.ui_builder.shared.inc`
- `player.ui_builder.home.inc`
- `player.ui_builder.now_playing.inc`
- `player.ui_builder.library.inc`

### 设计与规范
- `design_notes.md`
- `docs/ui/player_vivid_patterns.md`

### 入口与联动
- `Examples/project/player/win/CMakeLists.txt`
- `Examples/project/player/app-common/player.app.cppm`
- `player.controller.cppm`

---

## 6. 推荐工作流

如果后续继续推进 UI，建议按这个顺序：

1. 先看本目录 `README.md`
2. 再看 `design_notes.md` 确认当前要求
3. 如果涉及抽象或上收，再看 `docs/ui/player_vivid_patterns.md`
4. 明确该任务属于：
   - 页面表现调整
   - 组合层收敛
   - `Vivid` 通用 helper 上收
5. 改完后至少做一次对应页面截图验证

---

## 7. 下一阶段建议

从当前状态看，后续可以优先走两条路线之一：

- 回到 `Player` 本体，继续推进页面表现与交互完成度
- 继续从 `Player` 中提炼稳定组合层，推进 `Vivid` 能力建设

两条路线都可以，但不建议再回到“凭感觉微调”的工作方式。
