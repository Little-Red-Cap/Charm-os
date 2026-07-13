# H747 Lab Player MD3 板级 Smoke

## 文档状态

- `status`: `supporting`
- `scope`: `h747_lab_player_md3` 串口证据采集、验收边界与故障分层
- `authority`: capture 脚本、smoke schema、当前 app source/CMake 与实板日志

本页不复制全部 status 字段、case 数或当前功能完成度。字段名与版本以
[`player_md3_smoke_schema.hpp`](../apps/player_md3/player_md3_smoke_schema.hpp) 为准，自动验收以
[`capture-player-md3-smoke.ps1`](../tools/capture-player-md3-smoke.ps1) 为准，app 命令与产品配置见
[`player_md3/README.md`](../apps/player_md3/README.md)。

## 证据边界

该 smoke 证明真实 H747 target 经 Player Port/adapter 运行 canonical MD3 应用，并产生 display、input、
resource 或 playback 证据。它不证明主观视觉质量、产品触摸策略、音频质量、长期稳定性或所有硬件
加速路径。

Host、QEMU 与真实板是不同证据域。Host UI/smoke 不能替代 SDRAM、LTDC/DSI、touch、eMMC、I2S、
DMA/cache 和中断的实板结果。

## 推荐入口

在 `Examples/project/h747-lab` 下执行：

```powershell
./tools/flash-player-md3-pyocd.ps1
./tools/capture-player-md3-smoke.ps1
```

capture 默认使用 `COM16`、`115200 8N1`，并把日志写到：

```text
cmake-build-h747-lab-debug/h747_lab_player_md3_smoke.log
```

端口、波特率、probe、target、frequency、timeout 和日志路径均可由脚本参数覆盖。脚本会 reset/resume
目标、采集串口并验证一条完整 status；退出码 `0` 才表示所选 gate 通过。

当前 flash helper 使用 pyOCD。部分 CMSIS-DAP 路径可能打印
`Exception reading AP#2 IDR: Memory transfer fault`；只有 pyOCD 最终退出码和 erase/program summary
可以判断烧录是否成功，不能单凭该警告判定失败或成功。

## Capture 模式

| 模式 | 命令 | 证明范围 |
|---|---|---|
| 基础 | `./tools/capture-player-md3-smoke.ps1` | real MD3、bootstrap、render、present、可见 content 与基础输入事实 |
| 资源 | `./tools/capture-player-md3-smoke.ps1 -ResourceSmoke` | storage/VFS、font、cover 与 media 分层状态；资源为空可以形成有效负证据 |
| 输入 | `./tools/capture-player-md3-smoke.ps1 -InputSmoke` | 固定语义命令序列通过 Player input boundary 并触发 evidence render |
| 触摸 | `./tools/capture-player-md3-smoke.ps1 -TouchTrace` | 人工触摸产生 GT9xx/app-local trace；需要操作者交互 |
| 播放 | `./tools/capture-player-md3-smoke.ps1 -PlaybackSmoke` | 已填充资源通过前置 gate 后，曲目进入共享播放与 I2S/DMA callback 路径 |

`PlaybackSmoke` 隐含资源 gate。详细 required token、strict field 和数值判定直接读 capture 脚本，避免
文档与实现维护两份列表。

## 资源准备

资源 staging helper：

```powershell
./tools/stage-player-md3-resources.ps1
```

默认输出到 `cmake-build-h747-lab-debug/player_md3_resources`。helper 不写板；需要把其内容复制到
eMMC FAT 根目录。空资源和已填充资源是两种不同的合法证据：前者要求错误字段解释缺失层，后者才
要求 media/font 等资源可打开。资源文件存在不等于 MCU 具备对应 decoder 或 runtime binding。

## Schema 与验收

smoke schema v1 的字段名是 append-only board evidence identifier。修改或删除既有 token 时，必须同步
更新 schema consumer 和 capture gate。新增 record-only 诊断不能自动进入严格验收。

基础 gate 至少区分：

- 是否运行 real MD3 而非 mock/probe；
- bootstrap、render、present 和 content 是否成立；
- framebuffer/display/SDRAM/runtime storage 是否就绪；
- command/text overflow 与执行失败是否为零；
- status 是否包含可解析的 input、buffer 和 present 事实。

DMA2D、console TX DMA、batch/perf/touch latency 等字段可作为诊断，但是否属于 gate 由脚本决定。
README 中出现某个字段不授予它验收资格。

## 故障分层

按最早失败层定位，不先修改 Player UI：

1. **无 boot/prompt**：检查 artifact、flash 退出码、reset/startup、串口和 target 配置。
2. **display/SDRAM 未就绪**：检查内存初始化、MPU/cache、raster service 和 LTDC/DSI，再检查应用。
3. **boot/render/present 失败**：检查 Player Port materialization、runtime bootstrap、surface binding 和
   present sink。
4. **content/overflow/exec 失败**：记录页面、command/text 使用量和失败 draw kind，再检查 scene/render。
5. **raw input 有、route 无**：检查 app-local hardware-to-command translation；route 有、UI 无则检查
   runtime dispatch 与 focus/admission。
6. **touch 越界或丢短触摸**：保留原始点字节、配置表、采样频率和中断事实；不要先改 Vivid 坐标逻辑。
7. **storage/resource 失败**：依次检查 card/HAL、bus、partition、FAT mount、VFS、路径与 decoder/binding。
8. **playback 失败**：先按 AudioPlayer stage/error 区分 open、decode、buffer、sink 与 DMA callback。

性能字段只在固定 workload、时钟来源和采样窗口明确时可比较。单次 status 不能证明吞吐、帧率或长期
稳定性。

## 历史实板证据

以下只记录 2026-05 当时的结果，不代表当前提交仍已复测。

### 2026-05-25 基础显示

pyOCD 烧录后，串口出现 `player_md3: bootstrap ok`、`player_md3: first render ok` 和 console prompt；
captured status 满足 real MD3、display/SDRAM、render/present、可见 content、无 command/text overflow
和无 execution failure。pyOCD 报告
`Erased 1441792 bytes (11 sectors), programmed 1441792 bytes (1408 pages)`。原始 accepted line：

```text
player_md3 real_md3=1 mock=0 smoke=1/11111 delta=1/1 display=1 sdram1=1/1 rt_store=1 boot=1 render=1 frames=1 present=1 layer=1 fb=0xC0000000 bytes=0x00384000 render_buf=0xC0708000 platform=0xC0A8C000 runtime=0xC0D97328 pool_bytes=0x00724D20 r_s=0xFF101218/0xFF101218/0xFF12141E cmd=111/1024 co=0 text=225/4096 to=0 exec_fail=0 exec=60/31/10 fail_ti=0/0 input=0/0 t=1/1/0@0,0 e=0/0/0 storage=0/0/0/0@0:0 audio=1/1/0/0 content=0xFF101218:382479@0,3-719,1279 p_src=0xFF101218/0xFF101218/0xFF12141E p_dst=0xFF101218/0xFF101218/0xFF12141E front=0xC0384000:0xFF101218/0xFF101218/0xFF12141E back=0xC0000000:0x00000000/0x00000000/0x00000000 lfb=0xC0384000 lcr=0x00000001 lpf=0x00000000
```

### 2026-05-26 空资源

基础 gate 与 `-ResourceSmoke` 通过，资源状态为：

```text
resource=empty-or-missing fs=not-mounted(err=-2) no-tracks font=font-primary-missing(err=-38) font_cfg=1/1/0/0 media=media-missing(err=-2) cover=cover-missing(err=-2)
```

这是 eMMC 未填充资源时的有效负证据，只证明错误分层正确，不证明 populated-resource 或 playback
路径。任何“当前已通过”声明都必须附本次日志路径、commit/artifact identity 和 capture 命令。
