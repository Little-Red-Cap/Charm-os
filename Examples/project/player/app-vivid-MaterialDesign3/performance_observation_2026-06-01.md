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

`--ui-ci-perf-only` 已作为日常观测入口，固定采 Home 首帧、Home / Now Playing / Library / Library stress 稳定帧，以及 Home <-> Now Playing 动画过程，并输出三类 record-only 证据：

- `[ui-ci.perf]`：原始 workload counters，用于判断画了多少、画了什么。
- `[ui-ci.perf_time]`：阶段耗时，用于平台代价校准。
- `[ui-ci.perf_profile]`：派生压力画像，用于横向比较。

| 场景 | cmd | alpha | groups | flush | frame_us | tick_us | render_us | record_us | execute_us | present_us |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Home 稳定帧 | 110 | 210058 | 65 | 65 | 27366 | 14 | 27352 | 61 | 23394 | 2577 |
| Now Playing 稳定帧 | 66 | 74588 | 38 | 38 | 14738 | 8 | 14730 | 47 | 12055 | 1304 |
| Library 稳定帧 | 136 | 2213412 | 73 | 73 | 83855 | 27 | 83828 | 2969 | 78914 | 583 |

字段说明：

- `groups` 取自 `[ui-ci.perf]` 的 `rect + text + image + other`。
- `flush` 取自 `[ui-ci.perf]` 的 `batch_flushes`。
- `frame_us/tick_us/render_us/record_us/execute_us/present_us` 取自 `[ui-ci.perf_time]`。
- `alpha_screen_x1000` 取自 `[ui-ci.perf_profile]`，表示 alpha blending 覆盖屏幕倍数乘以 1000。
- `flush_density_x1000` 表示 `batch_flushes / cmd_count * 1000`。
- `exec_density_x1000` 表示 `exec_cmds / cmd_count * 1000`。
- 本次 Windows UI CI 已输出上述性能样本，但后续 case 出现挂起，进程已手动终止。因此这些样本可作为观测输入，但不能作为完整 UI CI 通过证据。
- perf-only 接入后完整 `--ui-ci` 曾通过 `ok=1 failed=0`；后续一次运行再次超时，说明完整 UI CI 仍有偶发挂起风险，但 perf-only 不受阻塞。

当前 perf-only 样本摘要：

| 场景 | alpha_screen_x1000 | cmd_mix rect/text/image/other | group_mix rect/text/image/other | flush_density_x1000 | exec_density_x1000 | text_bytes | blob_bytes |
| --- | ---: | --- | --- | ---: | ---: | ---: | ---: |
| Home | 306 | 48/29/23/10 | 21/20/14/10 | 591 | 1000 | 232/4096 | 48/2048 |
| Now Playing | 109 | 37/20/9/0 | 14/15/9/0 | 576 | 1000 | 144/4096 | 336/2048 |
| Library | 3221 | 75/38/15/8 | 27/23/15/8 | 537 | 1000 | 285/4096 | 0/2048 |
| Library stress | 3221 | 75/38/15/8 | 27/23/15/8 | 537 | 1000 | 285/4096 | 0/2048 |

Library stress 当前采用列表 wheel 后稳定帧。该样本与 Library 稳定帧 workload 相同，说明这次滚动输入后可见绘制压力没有变化；后续如需更强 stress，应加入明确可见区域变化或更深列表滚动状态。

## Home 首帧与 Now Playing 动画样本

Home 首帧与稳定帧 workload 相同，但首帧 record 成本明显更高：

| 场景 | frame_us | record_us | execute_us | present_us | alpha_screen_x1000 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Home first | 37506 | 8463 | 24788 | 2196 | 306 |
| Home stable | 26394 | 55 | 24062 | 799 | 306 |

Now Playing 进入/退出动画使用真实 `bottom_hit` / `now_back` 点击触发，按 16ms 间隔采逐帧样本并输出 summary：

| 动画 | frames | completed | total_frame_us | total_execute_us | max_frame_us | max_execute_us | total_alpha | max_alpha_screen_x1000 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| enter_now | 6 | 1 | 278595 | 86935 | 74320 | 22893 | 1697920 | 912 |
| exit_now | 7 | 1 | 289080 | 176978 | 59669 | 32630 | 1587352 | 671 |

进入 Now 的峰值帧出现在动画中段，`max_frame_us=74320`，但 `total_execute_us=86935`。退出 Now 的 `total_execute_us=176978` 明显更高，且多个中间帧执行 78 条命令，说明退出动画比进入动画更值得优先看 executor / overlay 组合成本。

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

- Library 是当前 Windows 样本中最明显的热点：`frame_us=83855`，其中 `execute_us=78914`，DrawCmd execute 主导整帧耗时。
- Library 的 `alpha=2213412` 明显高于 Home 的 `210058` 和 Now Playing 的 `74588`，大面积 alpha blending 是首要嫌疑。
- perf-only 派生画像进一步确认 Library `alpha_screen_x1000=3221`，即约 3.221 屏 alpha 覆盖；Home 约 0.306 屏，Now Playing 约 0.109 屏。
- Library 的 `record_us=2969` 可见，但远小于 `execute_us`，第一优先级不是 controller tick 或 UI tree record。
- Library 的 `present_us=583` 低于 Home 和 Now Playing，本次样本不支持把 Windows 主要瓶颈归因为 framebuffer present/flush。
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
