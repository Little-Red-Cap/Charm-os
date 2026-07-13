# Player Ink UI 早期规格保留笔记

> status: `archived`
>
> scope: ST7305 168x384 1bpp Player 页面与低刷新交互草图

本文从旧 `docs/ui/player_ui.md` 提取设计约束。当前 canonical Player 是 MD3；旧 Ink target 仅兼容保留。
本文不定义现行按键、引脚、页面、display backend 或验收状态。

## 历史目标

- panel：ST7305；
- raster：`168 x 384`、1bpp、portrait；
- style：monochrome、high contrast、单字重、简单 1-bit icon；
- input 草案：一个全局 primary action、一个 page-cycle key，可选 rotary encoder。

具体 KEY0/WKUP2 名称来自当时板级草图，不属于 Player 产品契约。

## Layout 约束

历史尺寸：

- margin `6 px`；
- header `20 px`；
- footer `30 px`；
- list row `14 px`；
- row 间距至少 `4 px`。

选择和当前播放状态使用反色块；内容左对齐，紧凑状态值可右对齐。以上数值只适用于该 168x384
草图，不得传播到 MD3/Vivid token。

## 页面草图

| 页面 | 历史内容 |
|---|---|
| Now Playing | title/artist、elapsed/total、format/rate、播放图标、source 状态与进度 |
| File List | current path、选中行、当前播放标记、scroll indicator、scan/empty/error |
| Metrics | uptime、frame、audio buffer、storage/display 状态 |
| Controls | volume、play/pause、repeat/shuffle 与可选 EQ/gain |

page-cycle key 按上述顺序循环；primary action 在当时草案中全局切换播放。encoder 若存在，只在 File
List 中移动选择、进入目录或播放，不应由通用 Player Port 假定。

## 低刷新策略

- 避免常规全屏动画与无意义全屏 clear；
- page transition 可分 header/footer 与 body 两阶段刷新；
- selection 只刷新 highlight 区；
- progress 只刷新 bar 区，更新周期按面板能力限制；
- play/pause icon 直接替换，不要求连续动画；
- dirty region、ghosting、partial/full refresh 和 refresh budget 由 EInk backend/policy 决定。

现行 EInk 边界见 [`../../ui/eink_refresh_policy.md`](../../ui/eink_refresh_policy.md)。若重新启用该产品，
必须重新核对 panel、input、page consumer、memory、refresh evidence 和 current source；不能从本草图恢复
板级事实。
