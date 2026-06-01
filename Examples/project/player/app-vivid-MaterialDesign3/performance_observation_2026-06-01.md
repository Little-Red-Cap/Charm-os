# Player MD3 性能观测第一轮记录 - 2026-06-01

本记录用于建立第一轮可重复性能观测基线，只做记录和初步归因，不做 DrawCmd 优化、不改视觉、不设置性能硬阈值。

## 观测输入

- Windows 目标：`charm-player-win-vivid-md3.exe --ui-ci`
- Windows perf-only 目标：`charm-player-win-vivid-md3.exe --ui-ci-perf-only`
- Windows 日志：`Examples/project/player/win/cmake-build-debug/generated/perf/player_md3_ui_ci_perf_observation.log`
- Windows perf-only 日志：`Examples/project/player/win/cmake-build-debug/generated/perf/player_md3_ui_perf_profile.log`
- H747 目标：`h747_lab_player_md3`
- H747 smoke 日志：`Examples/project/h747-lab/cmake-build-h747-lab-debug/generated/perf/player_md3_h747_perf_observation.log`
- H747 memory evidence：`Examples/project/h747-lab/cmake-build-h747-lab-debug/generated/memory/player_md3_memory_evidence.txt`

## Windows 样本

`--ui-ci-perf-only` 已作为日常观测入口，固定采 Home 首帧、Home / Now Playing / Library / Library stress 稳定帧，以及 Home <-> Now Playing 动画过程，并输出五类 record-only 证据：

- `[ui-ci.perf]`：原始 workload counters，用于判断画了多少、画了什么。
- `[ui-ci.perf_time]`：阶段耗时，用于平台代价校准。
- `[ui-ci.perf_profile]`：派生压力画像，用于横向比较。
- `[ui-ci.perf_cmd_detail]`：按 DrawCmd 类型聚合 `count/area/alpha/max_area`。
- `[ui-ci.perf_scope_detail]`：按 Player UI 粗粒度 scope 聚合 `cmd/area/alpha`。

| 场景 | cmd | alpha | groups | flush | frame_us | tick_us | render_us | record_us | execute_us | present_us |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Home 稳定帧 | 110 | 210058 | 65 | 65 | 27563 | 127 | 27436 | 99 | 25145 | 826 |
| Now Playing 稳定帧 | 66 | 74588 | 38 | 38 | 14193 | 40 | 14153 | 53 | 11983 | 800 |
| Library 稳定帧 | 136 | 2213412 | 73 | 73 | 82109 | 77 | 82032 | 149 | 79760 | 794 |

字段说明：

- `groups` 取自 `[ui-ci.perf]` 的 `rect + text + image + other`。
- `flush` 取自 `[ui-ci.perf]` 的 `batch_flushes`。
- `frame_us/tick_us/render_us/record_us/execute_us/present_us` 取自 `[ui-ci.perf_time]`。
- `alpha_screen_x1000` 取自 `[ui-ci.perf_profile]`，表示 alpha blending 覆盖屏幕倍数乘以 1000。
- `flush_density_x1000` 表示 `batch_flushes / cmd_count * 1000`。
- `exec_density_x1000` 表示 `exec_cmds / cmd_count * 1000`。
- 本次 Windows `--ui-ci-perf-only` 与完整 `--ui-ci` 均已通过；完整 UI CI 输出 `ok=1 failed=0`。

优化前 perf-only 样本摘要：

| 场景 | alpha_screen_x1000 | cmd_mix rect/text/image/other | group_mix rect/text/image/other | flush_density_x1000 | exec_density_x1000 | text_bytes | blob_bytes |
| --- | ---: | --- | --- | ---: | ---: | ---: | ---: |
| Home | 306 | 48/29/23/10 | 21/20/14/10 | 591 | 1000 | 232/4096 | 48/2048 |
| Now Playing | 109 | 37/20/9/0 | 14/15/9/0 | 576 | 1000 | 144/4096 | 336/2048 |
| Library | 3221 | 75/38/15/8 | 27/23/15/8 | 537 | 1000 | 285/4096 | 0/2048 |
| Library stress | 3221 | 75/38/15/8 | 27/23/15/8 | 537 | 1000 | 285/4096 | 0/2048 |

Library stress 当前采用列表 wheel 后稳定帧。该样本与 Library 稳定帧 workload 相同，说明这次滚动输入后可见绘制压力没有变化；后续如需更强 stress，应加入明确可见区域变化或更深列表滚动状态。

## 绘制细节归因

Windows MD3 现在默认启用 `CHARM_VIVID_DRAW_DETAIL_EVIDENCE=ON`；H747 PRODUCT 默认保持关闭。detail evidence 不改变 DrawCmd executor 策略，只在执行后记录命令类型与 Player scope 聚合结果。

Library 稳定帧 top 命令类型：

| 命令类型 | count | area | alpha | max_area |
| --- | ---: | ---: | ---: | ---: |
| FillLinearGradientRect | 9 | 1717600 | 1712380 | 458376 |
| FillRoundRect | 27 | 682008 | 497900 | 388800 |
| StrokeRoundRect | 36 | 2399608 | 3132 | 458376 |
| DrawTextBox | 38 | 287526 | 0 | 42872 |

Library 稳定帧 top scope：

| Scope | cmd | area | alpha |
| --- | ---: | ---: | ---: |
| default | 38 | 2715062 | 1253540 |
| library.list | 57 | 3644440 | 829344 |
| library.chrome | 23 | 279476 | 92648 |
| bottom_bar | 16 | 217336 | 37880 |

`exit_now` 峰值附近帧示例：

| 帧 | 命令类型 / scope | count/cmd | area | alpha |
| --- | --- | ---: | ---: | ---: |
| frame2 | FillRoundRect | 15 | 1989007 | 461113 |
| frame2 | now.transition | 12 | 2703335 | 289179 |
| frame2 | home.cards | 32 | 781924 | 162094 |
| frame3 | now.transition | 12 | 2342142 | 125066 |

当前细节证据显示：Library 的主要 alpha 来源不是文本或图片，而是大面积 `FillLinearGradientRect` 和 `FillRoundRect`；`exit_now` 的中间帧则同时混入 Home 卡片区域和 `now.transition` overlay。下一轮如果开始优化，应优先讨论“减少重复大面积 alpha 覆盖”或“把 transition overlay 的可见区域/不透明路径切清楚”，而不是先动文本布局或图片缩放。

## Library Alpha Overdraw 第一轮

本轮只处理 Library 稳定帧中常驻大面积底板的 alpha overdraw：`library_bg_top / library_bg_bottom`、`library_controls`、`LibraryListCard` 父级卡片 surface、`list_header_plate`、`list_body_plate`、`list_context_tray`。做法是在 Player Library builder 内把已知半透明颜色/渐变按近似背景预混合为 `alpha=255`，不改布局、控件树、文案、按钮形状、DrawCmd executor 或 PixelPlayer 参考视觉目标。

优化前基线来自本记录前面的 detail evidence：

| 场景 | execute_us | alpha_screen_x1000 | FillLinearGradientRect.alpha | FillRoundRect.alpha |
| --- | ---: | ---: | ---: | ---: |
| Library before | 77050 | 3165 | 1712380 | 460020 |

优化后 Windows `--ui-ci-perf-only` 样本：

| 场景 | execute_us | alpha_screen_x1000 | FillLinearGradientRect.alpha | FillRoundRect.alpha |
| --- | ---: | ---: | ---: | ---: |
| Library after | 42639 | 218 | 74724 | 72016 |
| Library stress after | 43117 | 218 | 74724 | 72016 |

完整 Windows `--ui-ci` 中 Library 样本为 `alpha=149872`、`alpha_screen_x1000=218`、`execute_us=44591`，并且完整 UI CI 输出 `done ok=1 failed=0`。这说明本轮主要降低了稳定帧大面积 alpha blending 工作量；`cmd_count=136` 基本保持不变，优化目标不是减少控件数量。

## Library Alpha Overdraw 第二轮

第二轮继续只处理 Library 稳定帧中常驻局部底板：`tabs_plate`、`action_row_plate`、`list_tab_indicator`、`list_context_tray_divider`、`list_path_bg`、`list_scroll_rail`。这些 surface 仍使用原有颜色/渐变方向，只按父级近似底色预混合为不透明；overlay popup、action menu、info sheet、scrim 未纳入本轮。

| 场景 | execute_us | alpha_screen_x1000 | FillLinearGradientRect.alpha | FillRoundRect.alpha | library.chrome.alpha | library.list.alpha |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Library after round1 | 42639 | 218 | 74724 | 72016 | 92648 | 53964 |
| Library after round2 | 43108 | 124 | 15648 | 66628 | 33572 | 48576 |
| Library stress after round2 | 42595 | 124 | 15648 | 66628 | 33572 | 48576 |

本轮主要收益来自 `library.chrome` 的 tabs/action plate 预混合，`FillLinearGradientRect.alpha` 从 `74724` 降到 `15648`。`FillRoundRect.alpha` 只小幅下降，说明剩余 alpha 主要来自更小的圆角控件或交互状态 surface；如果继续压，需要先做更细的 widget 实例级归因，不能再盲目扩大预混合范围。

## Home 首帧与 Now Playing 动画样本

Home 首帧与稳定帧 workload 相同，但首帧 record 成本明显更高：

| 场景 | frame_us | record_us | execute_us | present_us | alpha_screen_x1000 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Home first | 37699 | 8642 | 24699 | 2447 | 306 |
| Home stable | 27563 | 99 | 25145 | 826 | 306 |

新增 `[ui-ci.record_profile]` 后的当前样本显示，Home first 与 Home stable 的 record workload 完全一致：

| 场景 | record_us | cmd | text_bytes | blob_bytes | cmd_text | group_text | DrawTextBox | GlyphRun | overflow |
| --- | ---: | ---: | --- | --- | ---: | ---: | --- | --- | --- |
| Home first | 9327 | 110 | 232/4096 | 48/2048 | 29 | 20 | 28/174006 | 2/80224 | 0/0 |
| Home stable | 106 | 110 | 232/4096 | 48/2048 | 29 | 20 | 28/174006 | 2/80224 | 0/0 |

结论：首帧 `record_us` 峰值不是 UI tree 变大、文本 bytes 变多或 glyph run 数量变化导致；更像首次 record 热路径中的字体/glyph cache、host 字体后端或首次路径初始化成本。下一轮如要优化 Home 首帧，应先做 font/glyph warmup 或更细的字体后端 timing，而不是改 Home 布局或减少控件。

Now Playing 进入/退出动画使用真实 `bottom_hit` / `now_back` 点击触发，按 16ms 间隔采逐帧样本并输出 summary：

| 动画 | frames | completed | total_frame_us | total_execute_us | max_frame_us | max_execute_us | total_alpha | max_alpha_screen_x1000 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| enter_now | 6 | 1 | 277640 | 86837 | 73156 | 23074 | 1702193 | 914 |
| exit_now | 7 | 1 | 296400 | 182148 | 61990 | 34365 | 1582417 | 671 |

进入 Now 的峰值帧出现在动画中段，`max_frame_us=74320`，但 `total_execute_us=86935`。退出 Now 的 `total_execute_us=176978` 明显更高，且多个中间帧执行 78 条命令，说明退出动画比进入动画更值得优先看 executor / overlay 组合成本。

## Exit Now 双快照收敛验证

`NowPlaying -> Home` collapse 已改为与 expand 一样捕获 destination snapshot。动画中间帧不再提前暴露 live Home；`home.cards` 只在完成后的最终 Home 帧出现，不再与 `now.transition` overlay 同帧叠加。

| 场景 | frames | completed | total_execute_us | max_execute_us | total_alpha | max_alpha_screen_x1000 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| exit_now before | 7 | 1 | 182148 | 34365 | 1582417 | 671 |
| exit_now after | 7 | 1 | 67067 | 23607 | 782715 | 413 |

after 样本中 `exit_now:frame1` 到 `exit_now:frame5` 只有 `now.transition` scope；`home.cards` 只出现在 `active=0` 的最终 Home 帧。这说明本轮主要降低的是退出动画中段的重复 Home live UI 绘制，而不是改变视觉参数或 DrawCmd executor 算法。

后续人工观察仍发现进入/退出 Now Playing 时 Home 有轻微抖动。根因定位为整页 snapshot compose 带有水平 slide：进入时 Home source snapshot 从 `x=0` 立即偏到 `x=-18`，退出时 Home destination snapshot 末段偏到 `x=-18` 后最终 live Home 回到 `x=0`。已将整页 snapshot compose 固定在原位，运动只保留在 Now Playing / mini-player transition overlay 上。该修复不改变视觉参数，也不改变 DrawCmd executor。

继续观察退出动画时发现原画面残留感。原因是双快照路径仍每帧叠加 source snapshot，且 source opacity 末段仍接近 `180`。当前已改为 destination snapshot 作为稳定背景，source snapshot 只负责冻结/隐藏 live source，不再参与每帧合成；退出动画中段帧耗时降到约 `6.5-13.5ms` 区间，首帧仍包含 capture/准备成本。

## Transition 首帧 Capture / Prewarm 收敛

点击触发 Now Playing 进入/退出后，destination snapshot capture 已从同一 render frame 中拆出：frame0 只完成 source freeze 与 transition overlay compose，frame1 再 prepare/capture destination snapshot。动画中间帧仍使用 destination snapshot，不暴露 live destination，因此不会回到 Home 抖动或 `home.cards + now.transition` 同帧叠绘路径。

新增 record-only 日志：

```text
[ui-ci.perf_transition_capture] name=... capture_source_us=... prepare_destination_us=... capture_destination_us=... first_compose_us=... defer_frames=...
```

当前 Windows perf-only 样本：

| 动画 | frame0 frame_us | frame0 execute_us | frame0 alpha_screen_x1000 | frame1 frame_us | capture_source_us | prepare_destination_us | capture_destination_us | first_compose_us | defer_frames |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| enter_now | 8559 | 6360 | 176 | 19290 | 155 | 16 | 5547 | 180 | 1 |
| exit_now | 13302 | 10978 | 0 | 37772 | 173 | 22 | 23633 | 154 | 1 |

对比上一轮“首帧仍包含 capture/准备成本”的观察，本轮把首帧集中负载拆开：`enter_now frame0` 已低于 9ms，`exit_now frame0` 约 13.3ms；destination capture 仍然是实际成本中心，尤其 `exit_now capture_destination_us=23633`，但它被移到下一帧并可被独立观测。中段帧仍保持上一轮的顺滑路径，`home.cards` 只在最终 live Home 帧出现。

本轮不改变 PixelPlayer 视觉参数、不改 DrawCmd executor。完整 Windows `--ui-ci` 通过；H747 `h747_lab_player_md3 -j 1` 通过，`CHARM_PLAYER_LAYERED_TRANSITIONS=0`，`layered_forbidden_hits=0`。当前 H747 evidence 摘要：

| 指标 | 值 |
| --- | ---: |
| RAM_D1.used_bytes | 50376 |
| FLASH.bin_bytes | 1300512 |
| PlayerController.size_bytes | 9560 |
| PLAYER_ICON.ram_d1_buffer_count | 0 |

## H747 样本

当前已烧录固件的 H747 smoke 采集通过：

```text
real_md3=1 mock=0 smoke=1/11111 cmd=111/1024 text=241/4096 exec_fail=0 exec=48/31/22 cmd_batch=0/0/0 exec_batch=64/64/0/0 exec_groups=21/20/13/10 exec_cmds=48/31/22/0/0/10
```

该固件尚未包含新的 `perf_time` status 字段，因此本次不能作为 MCU timing 证据。尝试烧录当前 timing-enabled 固件时，pyOCD 返回 `DAP_TRANSFER_BLOCK response error`；后续重试时探针/串口路径被另一个 `h747_lab_dev_loader.bin` pyOCD 流程占用。因此 MCU timing 样本仍待补采。

当前 H747 memory evidence 摘要：

| 指标 | 值 |
| --- | ---: |
| RAM_D1.used_bytes | 50248 |
| RAM_D2.used_bytes | 4096 |
| FLASH.bin_bytes | 1299264 |
| PlayerController.size_bytes | 9560 |
| PLAYER_ICON.ram_d1_buffer_count | 0 |

备注：当前本地 H747 build cache 中 `CHARM_PLAYER_FILE_FONTS=ON`、`CHARM_ENABLE_FREETYPE=ON`，因此本地构建出现 FreeType/font-package 命中属于环境配置事实，不由本轮观测记录引入。

## 初步归因

- Library 是当前 Windows 样本中最明显的热点：`frame_us=82109`，其中 `execute_us=79760`，DrawCmd execute 主导整帧耗时。
- Library 的 `alpha=2213412` 明显高于 Home 的 `210058` 和 Now Playing 的 `74588`，大面积 alpha blending 是首要嫌疑。
- perf-only 派生画像进一步确认 Library `alpha_screen_x1000=3221`，即约 3.221 屏 alpha 覆盖；Home 约 0.306 屏，Now Playing 约 0.109 屏。
- Library 的 `record_us=149` 远小于 `execute_us`，第一优先级不是 controller tick 或 UI tree record。
- Library 的 `present_us=794` 低于 Home 首帧、接近其它稳定帧，本次样本不支持把 Windows 主要瓶颈归因为 framebuffer present/flush。
- `flush_density_x1000` 三页都在 537-591 区间，当前证据不支持把主要差异归因为 batch flush 密度。
- `exec_density_x1000=1000` 表示 executor 执行命令数与 recorded cmd 数一致，当前没有明显命令扩张。
- Home 首帧的额外成本主要在 `record_us=8463`，不是 workload 变多；这更像首次字体/文本/scene record 热路径成本。
- Now Playing 退出动画的 execute 总量高于进入动画，后续若处理动画卡顿，应优先看 `exit_now` 中间帧的 overlay alpha 和 command 组合。
- 在 MCU timing 补采之前，优化优先级只能先标记为候选；真正排序应以 H747 `perf_time` 和 status workload 一起判断。

## 待补观测

1. 保留 `--ui-ci-perf-only` 作为日常观测入口；如果完整 Windows UI CI 挂起再次复现，再单独排查。
2. 探针空闲后烧录当前 `h747_lab_player_md3`，采集包含 `perf_time=<available>/<frame>/<tick>/<render>/<record>/<execute>/<present>` 的 status 行。
3. 增强 Library 高压力样本，让 stress 帧确实改变可见绘制状态，而不是与普通 Library 稳定帧相同。
4. 若继续追动画卡顿，优先对 `exit_now` 增加更细的 overlay/layer composition 画像，再决定是否优化 alpha blending 或动画状态组织。
