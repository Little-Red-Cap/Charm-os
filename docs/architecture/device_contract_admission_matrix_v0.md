# 设备契约准入台账 v0

## 定位

本文是 Charm 设备公共契约窄腰的准入台账。

它不定义新的 C++ API，不声明任何接口已经成为稳定 ABI，也不替代
[`device_contract_narrow_waist_v0.md`](device_contract_narrow_waist_v0.md) 的路线总览。

它只回答一个更执行化的问题：

> **哪些设备契约已经具备什么证据，当前处于什么准入等级，下一步缺什么。**

准入等级以 [`interface_admission_policy.md`](interface_admission_policy.md) 为准。

## 当前总表

| 契约 | 当前等级 | 已有证据 | 主要缺口 | 下一步 |
| --- | --- | --- | --- | --- |
| I2C bus/device | `experimental` | mock backend、HAL adapter、准 driver、facts sidecar、no-hardware smoke | 真实硬件 evidence、真实芯片 driver、正式 probe / bringup evidence | 保持样板卡，补真实 driver 或 probe evidence |
| SPI bus/device | `proposed` | driver model、窄腰文档、[`spi_device_contract_v0.md`](spi_device_contract_v0.md) 已记录责任边界 | 未冻结 driver-facing API、无 mock、无 driver evidence | 先保持 proposed card，不写代码 |
| GPIO input/output/edge | `proposed` | HAL 层已有 GPIO 入口，[`gpio_device_contract_v0.md`](gpio_device_contract_v0.md) 已拆分三种语义面 | 未冻结 driver-facing API、无 mock、无 driver evidence | 先保持 proposed card，不写代码 |
| Block device | `proposed` | block registry、stable slot、runtime slot export、[`block_device_contract_v0.md`](block_device_contract_v0.md) 已记录 sector/live/flush/error 边界 | 未冻结 driver-facing API、无 contract mock、无 facts sidecar | 保持 proposed card，不写代码 |
| Stream IO | `proposed` | `io::Channel` 非阻塞纪律、registry/reactor/slot 经验、[`stream_io_device_contract_v0.md`](stream_io_device_contract_v0.md) 已记录等待与错误边界 | 未冻结为 device public contract、无 facts sidecar、无 contract mock | 保持 proposed card，不写代码 |
| Timebase | `proposed` | `charm.system.clock`、host/manual time source、[`timebase_device_contract_v0.md`](timebase_device_contract_v0.md) 已记录 monotonic/resolution/context 边界 | 未冻结 driver-facing API、无 facts sidecar、无 timeout evidence | 保持 proposed card，不写代码 |

## I2C 样板卡

### 保护对象

- I2C 外设 driver 作者
- 需要 mock/no-hardware 验证的 component 作者
- 需要把 HAL/backend 投影成 driver-facing contract 的 runtime glue 作者

### 语义面

I2C 当前只承诺最小同步 transaction 语义：

- `write(address, tx)`
- `read(address, rx)`
- `write_read(address, tx, rx)`
- `I2cDeviceRef` 绑定 bus backend 与 endpoint address

driver 作者优先依赖 `I2cDeviceRef`，不直接依赖 HAL、BoardCaps、mock 内部对象或 runtime discovery 字段。

### 责任边界

- `I2cBusRef` 负责保存 backend 对象与 ops，并做最小完整性检查。
- `I2cDeviceRef` 负责保存 endpoint address，避免 driver 重复传 address。
- backend 负责完成 transaction、映射平台错误、保证调用返回时 transaction 已完成或失败。
- driver 不负责 bus recovery、timeout loop、busy-spin、HAL handle 访问或 board 私有事实读取。

### 执行语义

当前 I2C contract 是同步完成模型。

当前不承诺：

- ISR-safe
- reentrant
- non-blocking
- reactor-managed
- managed time / replay controllable
- timeout 由公共 contract 托管

如果未来引入 timeout 或 async，必须通过明确 timebase / reactor contract 进入。

### 错误语言

当前 domain error kind 是 `I2cErrorKind`，并映射到 `util::Errc`。

已记录类别包括：

- `bus`
- `arbitration_lost`
- `nack_address`
- `nack_data`
- `overrun`
- `timeout`
- `target_detached`
- `policy_violation`
- `unsupported`
- `unknown`

`busy` 当前仍保留为 HAL adapter 映射语义，不提升为公共 I2C taxonomy。

### Facts

当前已有 contract-local fact vocabulary：

- `i2c.bus`
- `i2c.controller`
- `i2c.device`
- `clock.domain`
- `pinmux`
- `power.domain`
- `i2c.backend`
- `i2c.evidence`

这些 facts 已能形成最小 resolution 摘要，但仍是报告证据，不是构建期执法。

### Mock Evidence

已有：

- `io.device_i2c_mock`
- 固定容量 transaction script
- expected write/read/write_read
- expected backend failure
- unexpected transaction 记录

### Driver Evidence

已有：

- `driver.i2c_register_device`
- `RegisterDevice8<MaxPayload>`
- 单寄存器与 burst 读写语义

它是准真实 driver evidence，不等价于真实芯片 driver。

### System Compiler Projection

已有：

- `system_compiler.fact_evidence/v0` sidecar
- I2C facts artifact report sample
- `i2c-device-contract-facts-smoke`

仍缺：

- probe evidence
- board bringup evidence
- 真实硬件 evidence
- 真实芯片 driver evidence
- 更正式的 binding result / unresolved facts 投影

### 当前等级

`experimental`

理由：

- 已满足 mock backend、HAL adapter、准 driver、facts sidecar、no-hardware smoke。
- 未满足 candidate 要求中的真实硬件 evidence、真实 driver、正式 evidence pipeline/probe evidence。

## 其他候选卡

### SPI bus/device

当前等级：`proposed`

当前 proposed card：

- [`spi_device_contract_v0.md`](spi_device_contract_v0.md)

下一步只维护责任边界，不写实现。

必须先拆清：

- `SpiBus` 是否代表整条 bus 的受管访问能力
- `SpiDevice` 是否负责 CS、lock、transaction、flush、idle
- 带 CS 的 driver 是否默认禁止手动管理 CS
- 错误 taxonomy 是否区分 mode fault、overrun、chip select fault、timeout

当前明确不把 `hal_spi`、`hal::SpiBinding` 或 `block::SpiFlashBinding`
当作 driver-facing SPI contract evidence。

### GPIO input/output/edge

当前等级：`proposed`

当前 proposed card：

- [`gpio_device_contract_v0.md`](gpio_device_contract_v0.md)

下一步只维护三种语义面：

- `GpioInput`
- `GpioOutput`
- `GpioEdgeSource`

必须明确 debounce 属于上层 service，不属于基础 pin contract。

当前明确不把 `hal_gpio`、`hal_input` 或 `input.raw_sampler`
当作 driver-facing GPIO contract evidence。

### Block device

当前等级：`proposed`

当前 proposed card：

- [`block_device_contract_v0.md`](block_device_contract_v0.md)

下一步对齐已有 block slot/live state 经验，但不宣布为公共 ABI。

必须记录：

- sector size
- read/write/flush
- media present
- attached/detached/missing
- write protect
- erase granularity
- failure evidence

当前明确不把 `fs::BlockDevice`、`block.registry`、`DeviceSlotExport`
或 USB MSC / VFS demo 路径单独当作 admitted block contract evidence。

### Stream IO

当前等级：`proposed`

当前 proposed card：

- [`stream_io_device_contract_v0.md`](stream_io_device_contract_v0.md)

下一步把现有 `io::Channel` 纪律转成 admission record，但不宣布为公共 ABI。

必须保持：

- read/write 非阻塞
- 不返回 `Ok(0)`
- 暂不可用返回 `Errc::would_block`
- timeout 不由协议层 busy-spin
- 等待由 reactor / scheduler / EDA 负责

当前明确不把 `io::Channel`、`io.registry`、`io.reactor`、`ChannelSlotExport`
或 USB Host CDC runtime smoke 路径单独当作 admitted Stream IO contract evidence。

### Timebase

当前等级：`proposed`

当前 proposed card：

- [`timebase_device_contract_v0.md`](timebase_device_contract_v0.md)

下一步只记录 driver-facing time dependency，不新增 API。

必须明确：

- monotonic 语义
- resolution
- wrap behavior
- ISR 是否可读
- managed time / replay 是否可控
- timeout 由谁推进

当前明确不把 `charm.system.clock`、`hal_timer`、kernel timer queue、
Vivid replay 或 RK3506 generic timer IRQ smoke 单独当作 admitted Timebase contract evidence。

## 当前非目标

- 不把 I2C 升级为 `candidate`
- 不新增 SPI / GPIO / Block / Stream / Timebase 代码
- 不修改 `Modules/`、`Examples/`、`schemas/`、`scripts/`、`CMakeLists.txt`
- 不把准入台账当成 system compiler 执法入口
- 不把 proposed 候选写成 admitted 公共 ABI

## 维护规则

每次推进设备契约时，先更新本台账，再决定是否需要改具体 contract 文档。

如果一条契约从 `proposed` 升到 `experimental`，至少要同步记录：

- 一个 backend 或 mock
- 一个 driver / component / middleware 使用者
- 一条 no-hardware 或 bringup evidence
- 当前错误语言
- 当前执行语义
- 当前 system compiler facts 投影状态
