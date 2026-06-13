# Player 完整系统能力地图

本文是 Player 从 UI demo 走向完整播放系统的接手入口。它记录能力域、边界、验证路线和近期优先级，不定义新的公共 API，也不把 Draft 草稿提升为正式 contract。

## 1. 定位

Player 是 Charm 的真实项目压力线。它应该用真实播放系统需求推动共享能力收敛，但不能把 Player 私有需求反向写成 Charm 公共规则。

本文件回答：

- 完整 Player 需要哪些能力域。
- 每个能力域当前做到哪里。
- 哪些内容属于 Player 产品层、Vivid UI 层、runtime service、backend provider 或 board fact。
- Host、QEMU、Real Board 分别验证什么。
- 近期应该先闭环哪些功能，哪些继续保留为后续能力。

本文件不回答：

- 不定义 `Backends/`、BSP 或目录重构的最终形态。
- 不定义新的 Vivid widget、Player runtime API 或 PRODUCT profile。
- 不把 QSPI、eMMC、FAT path、Store layout、H747 status line 等板端事实提升为 Player API。
- 不替代 `app-vivid-MaterialDesign3/design_notes.md` 的 UI 视觉要求，也不替代 H747 `player_md3` README 的板端 evidence。

## 2. 输入原则

当前能力地图吸收以下 Draft 的结论作为设计输入：

- `Draft/block_storage_candidate_boundary_v0.md`：Player/runtime 不依赖 QSPI、eMMC、HAL handle、FAT path、Store layout 或 USB CDC frontend，只依赖 storage/file/resource capability 或更高层 runtime source。
- `Draft/charm_backend_capability_boundary_v0.md`：Application 声明 capability requirement，profile 绑定 provider instance，provider 把 backend 资源适配成稳定 capability contract。
- `Draft/host_qemu_realchip_validation_ladder_v0.md`：Host 验语义，QEMU 验启动、异常、loader 和 runtime-domain 骨架，Real Board 验真实外设、电气、DMA/cache、显示、触摸、音频和存储介质。
- `Draft/h747_board_execution_model_v0.md`：H747 当前 SDRAM、display、touch、storage、audio、DMA/cache 等事实是 board fact 和 provisional strategy，不直接进入 Player 公共语义。

这些 Draft 仍然是 Draft。Player 后续实现只能引用其中的边界原则，不能把它们当成已晋升的 contract。

## 3. 能力域地图

| 能力域 | 当前状态 | 目标闭环 | 归属层 | Host 验证 | QEMU 可选验证 | Real Board 验证 | 明确不做 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 媒体库扫描 | 已建立 host-first 路径推导索引：扫描 `/music` 与下一级目录，回退根目录一层扫描，并从路径推导 title/artist/album/format。 | 从资源源读取曲目，建立歌曲、专辑、艺术家索引，并支持空库、损坏文件、增量刷新。 | Player runtime service，底层消费 file/resource capability。 | 用 host file tree 验证扫描、排序、分组、错误映射。 | 验证文件 source 控制流或 storage failure path。 | 验证 eMMC/FAT/resource population、真实路径缺失和错误 evidence。 | 不让 UI 直接依赖 FAT path；不把 Store layout 当媒体库模型；当前不读取 FLAC/MP3 tag。 |
| 播放队列 | 已建立第一切片：Library track 选择会捕获当前可见视图顺序作为固定容量队列，Next/Prev、track end、repeat/random 均按队列作用域运行；直接选曲保留全库 fallback 队列。 | 支持当前队列、上一首/下一首、shuffle、repeat、Library 选择同步、mini-player/Now Playing 一致性。 | Player domain/controller，小容量状态留在 shared controller。 | UI CI 覆盖队列语义、随机顺序、切歌同步。 | 可验证状态机或 capability table。 | 验证触摸/encoder/button 到语义命令的真实输入路径。 | 不在 controller 引入动态容器或 host-only 分支；当前不新增可视化 Queue 页面。 |
| 解码与 audio sink | UI playback 状态已存在，H747 audio smoke 仍是后续闭环。 | 音频文件解码、PCM 输出、pause/resume/seek、underrun/error evidence、sink unavailable 降级。 | Decoder/runtime service + AudioSink provider；UI 只消费播放状态。 | 用 host decoder/sink mock 验证状态和错误语义。 | 可验证 sink unavailable/failure path。 | 验证 I2S/DMA callback、speaker/HiFi path、underrun、真实时钟与缓存行为。 | 不把 I2S、DMA、codec HAL 暴露给 Player UI。 |
| 资源、封面、歌词 | Cover 已通过 `ResolvedCover` seam 收敛；歌词代码保留但默认从固件剔除。 | 资源封面、placeholder、host cover decode、sidecar 歌词、后续 embedded metadata 通过统一 seam 进入 UI。 | Player resource/cover/lyrics service；host-only decode 保持内部细节。 | 用 host 文件和 metadata 样本验证解析、fallback、截断和 UI 同步。 | 可验证 resource source 控制流。 | 验证资源文件存在、unsupported decoder、容量和 memory evidence。 | 当前不做网络歌词、歌词编辑、翻译、TTML、word-by-word 动画。 |
| Now Playing UI | 主结构、cover/title/progress/control、seek 和 transition 已有 UI CI 与性能收敛。 | 成为第一张完整产品体验闭环页：封面、标题、歌词开关、进度、控制、fallback 状态稳定。 | Player MD3/Vivid UI 产品层。 | Windows UI CI、截图、transition/perf-only 观测。 | 通常不作为 UI gate。 | H747 PRODUCT/StaticCut 编译与 first-frame smoke，不要求 host 动效。 | 不引入 H747 专用 UI 分叉；不主观偏离 PixelPlayer 参考。 |
| Library UI | Tab、Sort、Shuffle、Path、Action menu、Info popup 已进入基础功能闭环。 | 成为完整本地浏览入口：分类、上下文、当前播放标记、操作菜单、详情与队列同步。 | Player MD3/Vivid UI + Player controller。 | UI CI 覆盖点击和键盘操作；perf-only 监控稳定帧。 | 通常不作为 UI gate。 | 验证触摸命中、列表输入、PRODUCT profile 和 memory evidence。 | 本阶段不把媒体扫描细节写进 UI builder。 |
| Home UI | Home 作为能力探针，Hero、卡片、统计视觉和性能已部分收敛。 | 承载推荐、最近播放、统计和主要播放入口，后续接真实数据。 | Player MD3/Vivid UI + Player statistics service。 | UI CI、截图、Home first/stable perf record。 | 通常不作为 UI gate。 | 验证 first-frame、触摸入口、内存和 forbidden module gates。 | 不先做复杂推荐算法；不为 Home 视觉微调牺牲产品 gate。 |
| 持久化与统计 | Listening stats 已有核心 UI 与部分真实统计语义；持久化策略仍需继续产品化。 | 记录播放次数、播放时长、最近播放、用户偏好，并支持容量受限降级。 | Player persistence service，底层消费 storage/key-value/file capability。 | Host 验证读写、损坏恢复、版本兼容。 | 可验证 storage failure path。 | 验证真实 eMMC/FAT/QSPI provider evidence、断电/写失败策略。 | 不让 UI 直接写文件；不把某个板端路径写成产品规则。 |
| 诊断 evidence | Windows perf/profile、H747 memory/status evidence 已建立。 | 每个产品功能切片都有语义、性能、内存和 provider 状态证据。 | Evidence helper + app presentation。 | UI CI、perf-only、host fake provider evidence。 | 可验证 startup/fault/evidence projection。 | 串口 status、memory evidence、DMA/cache/storage/audio/touch provider evidence。 | 不把 status line 文本格式当公共 schema。 |
| 平台 capability | Player 已暴露 display/input/storage/audio 等真实需求，但后端目录仍由平台线整理。 | Player 只声明能力需求，由 Host/QEMU/Board profile 绑定 provider。 | Profile/runtime assembly + backend provider。 | Host provider 验证 app/domain 语义。 | QEMU 验启动、trap、loader、runtime-domain 骨架。 | Board BSP/provider 验真实硬件事实。 | Player 不定义 backend/BSP 目录结构，不绕过 capability seam。 |

## 4. 近期路线

近期 Player 工作按以下顺序推进，避免同时打开过多系统面：

1. 继续完成 UI 产品闭环：Library 已完成基础操作闭环，下一步优先补 Home 真实入口与 Now Playing 细节缺口。
2. 收敛媒体库最小闭环：当前已有路径推导索引，下一步再评估真实 tag metadata、深层递归、增量刷新和持久媒体库数据库。
3. 继续收敛播放队列：当前已完成 Library 视图队列第一切片，后续再补可视化 Queue、播放历史、持久化和更完整的 Home 入口语义。
4. 接入 audio sink：先 host mock/host playback，再按 H747 provider evidence 验证 I2S/DMA/underrun。
5. 恢复资源扩展：封面、歌词、统计、最近播放按 capability seam 接入，不把 host-only 或 board-only 路径写进 shared controller。

每个切片都应独立提交，并明确：

- 是否改 UI、controller、runtime service、provider glue 或文档。
- Windows 需要跑哪些 UI CI、perf-only 或 host smoke。
- H747 是否需要 build、memory evidence、status smoke 或真实外设采样。
- 是否触发 PRODUCT profile、Vivid module、style/payload cap 或 memory baseline 更新。

## 5. 验证分层

Host 是日常开发主线，用于快速验证：

- 媒体库扫描、排序、分组和错误语义。
- 播放队列、seek、pause/resume、shuffle/repeat 状态机。
- UI CI、截图、perf-only workload/timing。
- resource/cover/lyrics fallback 和容量截断。

QEMU 是后续补齐路径，不作为每个 Player 功能的前置 gate，用于验证：

- startup、loader、fault、trap、runtime-domain 骨架。
- capability table、storage failure path、AppRuntime 控制流。
- 不依赖真实外设的模型一致性。

Real Board 是真实产品事实来源，用于验证：

- SDRAM framebuffer、DCache clean、LTDC/DSI present、DMA2D。
- GT9xx touch、encoder/button、input latency 和 mapping。
- eMMC/QSPI/FAT resource、真实文件缺失和 provider degradation。
- I2S/DMA audio callback、underrun、speaker/HiFi path。
- RAM_D1/FLASH/PRODUCT forbidden module gates。

如果 Host 与 Real Board 结论冲突，优先按 failure class 归因：semantic failure、projection failure、model gap、simulator gap、board failure 或 evidence gap。不要用 host mock 替代真实硬件 evidence，也不要用 QEMU 缺失否定真实板事实。

## 6. 接手规则

- UI 视觉目标继续看 `app-vivid-MaterialDesign3/design_notes.md`，不要在本文件里调颜色、圆角、阴影或 PixelPlayer 参考参数。
- H747 PRODUCT、StaticCut、memory evidence 和 forbidden module gates 继续看 `Examples/project/h747-lab/apps/player_md3/README.md`。
- Backend/BSP/目录整理由平台线推进；Player 只暴露能力需求和验证压力。
- 新功能默认 host-first；只有涉及真实显示、触摸、音频、存储、DMA/cache 时才要求 Real Board evidence。
- shared controller 继续保持 MCU-clean：不新增动态容器、host-only 状态或板端专用分叉。
- 歌词保留为 future capability；默认固件路径继续保持剔除，后续恢复必须显式开启 profile 并刷新 evidence。
