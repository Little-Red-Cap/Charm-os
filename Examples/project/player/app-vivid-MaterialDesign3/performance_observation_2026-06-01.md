# Player MD3 性能观测第一轮记录 - 2026-06-01

本记录用于建立第一轮可重复性能观测基线，只做记录和初步归因，不做 DrawCmd 优化、不改视觉、不设置性能硬阈值。

## 观测输入

- Windows 目标：`charm-player-win-vivid-md3.exe --ui-ci`
- Windows 日志：`Examples/project/player/win/cmake-build-debug/generated/perf/player_md3_ui_ci_perf_observation.log`
- H747 目标：`h747_lab_player_md3`
- H747 smoke 日志：`Examples/project/h747-lab/cmake-build-h747-lab-debug/generated/perf/player_md3_h747_perf_observation.log`
- H747 memory evidence：`Examples/project/h747-lab/cmake-build-h747-lab-debug/generated/memory/player_md3_memory_evidence.txt`

## Windows 样本

| 场景 | cmd | alpha | groups | flush | frame_us | tick_us | render_us | record_us | execute_us | present_us |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Home 稳定帧 | 110 | 210058 | 65 | 65 | 27366 | 14 | 27352 | 61 | 23394 | 2577 |
| Now Playing 稳定帧 | 66 | 74588 | 38 | 38 | 14738 | 8 | 14730 | 47 | 12055 | 1304 |
| Library 稳定帧 | 136 | 2213412 | 73 | 73 | 83855 | 27 | 83828 | 2969 | 78914 | 583 |

字段说明：

- `groups` 取自 `[ui-ci.perf]` 的 `rect + text + image + other`。
- `flush` 取自 `[ui-ci.perf]` 的 `batch_flushes`。
- `frame_us/tick_us/render_us/record_us/execute_us/present_us` 取自 `[ui-ci.perf_time]`。
- 本次 Windows UI CI 已输出上述性能样本，但后续 case 出现挂起，进程已手动终止。因此这些样本可作为观测输入，但不能作为完整 UI CI 通过证据。

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
- Library 的 `record_us=2969` 可见，但远小于 `execute_us`，第一优先级不是 controller tick 或 UI tree record。
- Library 的 `present_us=583` 低于 Home 和 Now Playing，本次样本不支持把 Windows 主要瓶颈归因为 framebuffer present/flush。
- 在 MCU timing 补采之前，优化优先级只能先标记为候选；真正排序应以 H747 `perf_time` 和 status workload 一起判断。

## 待补观测

1. 解决或绕开 Windows UI CI 在性能样本输出后的挂起，让同一批样本附着到完整通过的 UI CI。
2. 探针空闲后烧录当前 `h747_lab_player_md3`，采集包含 `perf_time=<available>/<frame>/<tick>/<render>/<record>/<execute>/<present>` 的 status 行。
3. 增加 Library 高压力样本：滚动或切 tab 后等待稳定帧，再记录 workload/timing。
4. 若 H747 与 Windows 都指向 `execute_us + alpha`，下一轮再规划 Library alpha blending / DrawCmd execute 路径的针对性优化。
