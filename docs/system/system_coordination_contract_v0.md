# System Coordination Contract v0

本文定义 Charm 在真实板级工程里建立共享系统协调层时的第一版契约。

它来自 `stm32h747-player` 这个真实 H747 `EvidenceRig` 暴露出的工程压力：多个主线可以同时推进硬件证据，但如果没有统一的系统壳、服务状态准入和受控 mutation 规则，`system_probe` 很容易变成新的 bring-up 杂物箱。

v0 的目标很窄：

- 固定 `SystemShell`、`ServiceSnapshotContract`、`PowerProfile`、`GuardedMutation`、`ReadyFacts` 五个术语。
- 说明系统协调层如何合法消费 board service。
- 把 `power` 作为第一条已经适合进入协调层的样板。
- 明确 audio / network / display / storage 这类重 runtime 不应被第一轮直接拖入共享系统壳。

## Roles

### Board App

`board app` 负责板级 bring-up、证据采集和局部实验命令。

典型例子：

- `board_smoke` 证明烧录、串口、最小板级启动链成立。
- `audio_board_probe` 负责音频板控、PMIC 受控实验和 codec 控制面。
- `usb_audio` 负责 USB Audio OUT、ring、I2S DMA 和数字播放证据。
- `storage_probe` 负责 QSPI / SDRAM / eMMC 等存储主线证据。

`board app` 可以包含某条主线的临时命令、重 probe 逻辑和实验性输出格式，但它的结论不能自动升级为共享系统契约。

### SystemShell

`SystemShell` 是统一系统级命令入口，不是新的 bring-up app 杂物箱。

它负责：

- 统一 shell 命名空间。
- 输出 `ReadyFacts`。
- 列出 service registry。
- 查看已经准入的 service snapshot。
- 执行受控、系统级 mutation，例如 `reboot` 或命名 `PowerProfile`。

它不负责：

- 直接拉起 audio / network / display / storage 的重 runtime。
- 暴露原始寄存器写透传。
- 把每条主线的本地实验命令搬进一个大 app。
- 替代某条硬件主线自己的 evidence app。

### Service Snapshot

`service snapshot` 是 board service 给系统层消费的只读状态面。

它必须满足 `ServiceSnapshotContract` 后，才允许进入 `SystemShell` 或共享 coordination 层。

## Public Terms

### SystemShell

统一系统级命令入口。

`SystemShell` 的命名空间应优先收束为：

- `sys status`
- `sys services`
- `service status <name>`
- `<service> status`
- `<service> probe`
- `reboot`

其中 `<service> probe` 只允许用于无破坏或受控的刷新动作。某条主线的重 probe、压力测试、raw write、初始化表实验，应继续留在对应 board app。

### ServiceSnapshotContract

任何 board service 要进入共享协调层，必须先提供满足以下条件的状态导出面：

- 只读：读取 snapshot 不改变硬件状态。
- 无副作用：不会隐式启用 rail、启动 DMA、枚举总线或改变外设模式。
- 可重复读取：多次读取应得到当前事实，不依赖一次性消耗。
- 可串口打印：状态字段可以稳定转成简短诊断文本。
- 可判定 ready：至少能导出当前是否可用、是否已配置、是否存在错误。

没有满足该契约的 service，可以在 `sys services` 中作为占位项出现，但必须标记为未准入，不得伪装成已经被系统层协调。

### PowerProfile

`PowerProfile` 是命名的板级电源语义，不等于任意 PMIC 寄存器写。

系统层只允许消费已经命名、已经归档边界的 profile，例如：

- `alive_minimal`
- `system_console`
- `audio_stage_a`
- `display_stage_a`
- `network_stage_a`
- `storage_stage_a`

具体 profile 的寄存器细节属于 board service 和对应硬件主线，不属于 Charm 通约层。

### GuardedMutation

`GuardedMutation` 是允许进入系统层的受控 mutation。

它必须满足：

- 有高层语义，例如“应用 `system_console` profile”，而不是“写 PMIC reg 0x12”。
- 有明确 owner 和适用场景。
- 可以通过 snapshot 验证结果。
- 失败时不会让系统层误判为其它子系统故障。

原始 PMIC `raw write`、level1 / level2 寄存器写透传、codec 初始化表实验，不属于共享 `SystemShell` 的默认命令。它们可以留在对应 board app 的受控实验入口。

### ReadyFacts

`ReadyFacts` 是系统成立过程中的可打印、可判定事实。

H747 当前采用的最小集合是：

- `board.ready`
- `platform.ready`
- `console.ready`
- `power.ready`
- `system.ready`

这些事实应来自真实 init / graph / service 状态，不应只是静态 banner 文本。

## First Admitted Service: Power

`power` 是第一条适合进入系统协调层的 service，原因是：

- 它已经有 `power_snapshot()`。
- 它已经有 `power_current_profile()` 和 `power_profile_name()`。
- 它已经区分命名 profile 与 raw PMIC write。
- 它是 Audio / Net / Display / Storage 的共享前置事实，但又不要求系统层拉入这些重 runtime。

H747 `system_probe` v0 的合法命令样板固定为：

- `sys status`
- `sys services`
- `power status`
- `power probe`
- `service status power`

其中 `service status power` 必须基于真实 power snapshot，而不是静态描述文本。

## Non-Admitted Services

第一轮不把以下内容纳入共享协调层：

- Audio 的 USB / ring / I2S DMA 数字链状态。
- Network 的 SDIO / HCI / AP6212 细节。
- Display 的 LTDC / DSI / framebuffer 重状态。
- Storage 的 probe / read / write 细节。

这些 service 可以在 registry 中出现，但必须清楚标注为 `not-admitted` 或 `coordinated=0`。它们进入系统层前，需要先提供独立、无副作用、可重复打印的 snapshot/status 接口。

## Migration Rule

新增 service 要进入 `SystemShell` 时，按这个顺序推进：

1. 在自己的 board app 中证明硬件证据。
2. 提供无副作用 `snapshot/status`。
3. 在文档中声明 owner、profile、ready 判据和 mutation 边界。
4. 只把 snapshot 接到 `sys services` / `service status <name>`。
5. 等 snapshot 稳定后，再讨论是否把受控 mutation 暴露给系统层。

这条规则的重点是先让系统层“看见事实”，再让系统层“执行动作”。

## Boundary With Charm Architecture

本文不是要求所有 board service 立刻抽象成一套通用 C++ 接口。

v0 只固定契约、命令语义、状态准入和第一条样板。后续如果某个模式在多个 board 上复现，再把它提升进 `BoardCaps / BringupMinimal / service contract / app template`。

