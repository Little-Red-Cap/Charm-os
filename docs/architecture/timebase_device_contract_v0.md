# Timebase Device Contract v0

## 定位

本文记录 Charm 设备契约窄腰中的 Timebase proposed contract card。

它不是 admitted 公共 ABI，也不是当前 `charm.system.clock` 的重命名。

它只回答一个更窄的问题：

> **一个 driver、mock backend、protocol adapter 或 timeout-aware component 如果需要时间，未来最小应该依赖什么 timebase 语义。**

当前代码中已经存在：

- `Modules/system/clock/system_clock.cppm`
- `Modules/system/clock/system_time.cppm`
- `Modules/platform/win/time_source.cppm`
- `Modules/platform/win/manual_time_source.cppm`
- `Modules/io/hal/hal_timer.cppm`
- `Modules/system/kernel/timer.cppm`
- `Modules/system/kernel/timer_wheel.cppm`

这些说明 Charm 已经有系统 clock、平台 time source、HAL timer 与 kernel timer queue 的不同胚胎。

但它们仍然不等价于一个已经 admitted 的公共 driver-facing Timebase device contract。

## 1. 当前等级

当前等级是 `proposed`。

它已经具备：

- `charm.system.clock` 的 `now_ms / now_us` 基础形状
- `ClockBinding` 把 `system.clock` 接入 init.graph
- `ClockCaps::TimeSource` 的全局绑定入口
- Win steady clock source
- Win manual time source
- `hal_timer` 的 controller-facing timer driver concept
- kernel timer queue / timer wheel 策略经验
- RK3506 generic timer IRQ smoke 文档经验

它还不是 `experimental`，因为仍然缺：

- 面向 driver / component 作者的正式 timebase contract 责任卡
- contract-local timebase facts vocabulary
- 更窄的 Timebase domain error taxonomy
- resolution / monotonic / wrap behavior 的统一表达
- ISR-readable / task-only 边界说明
- managed time / replay 是否受控的准入语言
- artifact / evidence pipeline 中正式的 facts 投影
- 一个只依赖该 contract 的准真实 timeout-aware driver / middleware evidence

## 2. Contract Shape

当前 v0 不新增 C++ API。

Timebase proposed contract 的最小语义面应围绕下面对象收敛：

- `Timebase`
- `TimeInstant`
- `TimeDuration`
- `TimeSourceState`

`Timebase` 表示一个可读时间源。

它至少需要表达：

- monotonic 还是 wall-clock
- resolution
- unit
- wrap behavior
- ISR 是否可读
- task context 是否可读
- 是否可由 managed time / replay 控制

`TimeInstant` 表示来自同一 timebase 的时间点。

`TimeDuration` 表示时间差或 timeout budget。

`TimeSourceState` 表示当前 time source 是否：

- missing
- bound
- running
- paused
- replaying
- invalid

当前仓库里的 `charm::system::Clock` 已经提供 `now_ms / now_us` 胚胎。
但 proposed contract 仍需要把它收束成面向公共设备契约的准入记录。

## 3. Ownership And Responsibility

### 3.1 Timebase Provider

Timebase provider 负责：

- 提供当前 tick / instant
- 声明 unit 与 resolution
- 声明是否 monotonic
- 声明 wrap behavior
- 声明可读上下文
- 把平台错误或未绑定状态映射成公共错误语言

Timebase provider 不应该泄漏：

- vendor SDK timer handle
- CPU 私有寄存器访问细节
- board 私有 clock handle
- kernel timer queue 内部结构
- Vivid replay 或测试脚本内部状态

### 3.2 Driver / Component User

driver / component 负责：

- 显式声明自己是否需要 timebase
- 不自建 `now_ms` / `now_us`
- 不 busy-spin 等待 timeout
- 不把 sleep loop 藏进协议层
- 不假设 wall-clock 语义
- 不混用不同 timebase 的 instant

如果需要 timeout，driver 应表达：

```text
requires:
  monotonic timebase
  minimum resolution
  scheduler / reactor path if waiting is needed
```

而不是直接在 driver 内部睡眠或轮询。

### 3.3 Scheduler / Reactor / Runtime Layer

Scheduler / reactor / runtime layer 负责：

- 推进等待
- 管理 timeout
- 决定 task context 里的 sleep / wake
- 将 managed time / replay 语义投影给上层

基础 Timebase contract 不应该直接承诺：

- sleep API
- timer queue API
- callback dispatch
- replay timeline
- deterministic execution

这些属于更高层 runtime contract。

## 4. Time Semantics

Timebase contract 必须把时间语义写清楚，而不是只提供 `now()`。

至少需要记录：

- monotonic 或 wall-clock
- unit：ticks / us / ms / ns
- resolution
- tick width
- wrap behavior
- 读操作是否无阻塞
- 读操作是否 ISR-safe
- 是否可能暂停
- 是否可能回退
- 是否可手动推进
- 是否由 replay / managed time 控制

当前 `charm.system.clock` 提供：

- `now_ms()`
- `now_us()`
- `ClockBinding`
- `ClockRef`

当前 `platform.win.manual_time_source` 说明 host/mock 路径可以手动推进时间。

但这些只是现有能力胚胎，还不是完整公共 Timebase contract。

## 5. Execution Semantics

当前 Timebase proposed contract 暂定只描述读取语义。

一次 timebase read 调用返回时，provider 应完成下列之一：

- 返回当前 instant / tick
- 表示 timebase 未绑定
- 表示当前上下文不允许读取
- 表示 provider 当前不可用

当前不承诺：

- sleep
- delay
- callback
- periodic tick
- timeout dispatch
- scheduler integration
- reactor integration
- managed time / replay 可控制

如果未来需要 timeout、delay、periodic timer、replay 或 fast-forward，必须通过明确 runtime / scheduler / managed-time contract 进入。

特别注意：

- `charm.system.time::try_sleep_ms` 当前是 busy wait helper。
- 该 helper 不应被提升为基础 Timebase device contract 的等待模型。

## 6. Error Semantics

当前 clock read API 主要返回 tick 值。

这对最小系统很简单，但 proposed contract 仍缺一组能解释失败状态的 Timebase domain error taxonomy。

candidate taxonomy 至少应考虑：

- `missing`
- `not_bound`
- `not_running`
- `not_monotonic`
- `resolution_too_low`
- `context_not_allowed`
- `wrap_unknown`
- `paused`
- `replay_mismatch`
- `unsupported`
- `policy_violation`
- `unknown`

在 `experimental` 前，不应为了某个单一 backend 草率冻结 taxonomy。

## 7. Facts

Timebase proposed contract 未来至少需要能投影下面 facts：

- `timebase.source`
- `timebase.monotonic`
- `timebase.resolution`
- `timebase.unit`
- `timebase.wrap`
- `timebase.context`
- `timebase.managed`
- `timebase.replay`
- `clock.domain`
- `timer.controller`
- `irq.line`
- `power.domain`
- `timebase.evidence`

这些 facts 在 v0 不做构建期执法。

它们应先服务：

- admission record
- artifact report
- evidence sample
- explain / unresolved binding 入口
- timeout dependency audit
- managed time / replay 边界说明

## 8. Evidence Inventory

当前已有的 Timebase 证据主要分成四类。

### 8.1 System Clock Evidence

已有：

- `charm.system.clock`
- `Clock`
- `ClockRef`
- `ClockCaps::TimeSource`
- `ClockBinding`
- `system.clock` capability

这证明系统时钟可以进入 capability / init.graph 装配语言。

### 8.2 Host / Mock Time Evidence

已有：

- `platform.win.time_source`
- `platform.win.manual_time_source`

这证明 hosted backend 与可手动推进 time source 已有胚胎。

### 8.3 HAL / Kernel Timer Evidence

已有：

- `hal_timer`
- `kernel.timer`
- `kernel.timer_wheel`

这证明 controller-facing timer 与 scheduler timer queue 有独立经验。

它们不等价于 driver-facing Timebase contract。

### 8.4 Board Bringup Evidence

已有：

- `docs/board/rk3506/generic_timer_irq_smoke.md`

这证明真板 generic timer IRQ 路径有 bringup evidence。

它不等价于公共 Timebase contract admitted evidence。

## 9. Evidence Gaps

当前缺口明确保留：

- 没有专门的 timebase contract mock / fault script
- 没有 contract-local facts vocabulary
- 没有 Timebase domain error kind
- 没有 resolution / wrap behavior 统一记录
- 没有 ISR-readable / task-only 准入字段
- 没有 timeout dependency 的 artifact report 投影
- 没有 managed time / replay 与基础 timebase 的正式边界卡
- 没有真实 driver / middleware 只依赖 Timebase contract 的 evidence

这些缺口补齐前，Timebase 仍保持 `proposed`。

## 10. Non-goals

当前阶段明确不做：

- 不新增 `Modules/io/device/io.device_timebase.cppm`
- 不修改 `charm.system.clock`
- 不修改 `charm.system.time`
- 不修改 `hal_timer`
- 不修改 kernel timer queue
- 不修改 Vivid replay / motion runtime
- 不修改 minimal-kernel runtime
- 不宣布 Timebase contract 为 `experimental`
- 不把 busy-wait sleep 提升为公共等待模型
- 不承诺全局确定性时间宇宙
- 不承诺 managed time / replay

## 11. Next Steps

最值当的下一步是：

1. 保持本文件为 `proposed` card。
2. 先设计 timebase facts 草案，例如 monotonic、resolution、wrap、context、managed。
3. 明确 `Clock` read-only 语义与 runtime sleep / scheduler timeout 的分界。
4. 选择一个准真实 timeout-aware middleware evidence，例如 polling helper、debounce service、retry policy。
5. 再决定是否需要 `TimebaseRef` 或 `MonotonicClockRef` 这类 driver-facing 类型。
6. 与 [`device_contract_admission_matrix_v0.md`](device_contract_admission_matrix_v0.md) 同步准入状态。

在这些完成前，Timebase 仍保持 `proposed`。
