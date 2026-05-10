# 设备契约晋级队列 v0

## 定位

本文是设备契约准入工作的晋级队列。

它不定义新的 C++ API，不修改任何契约等级，也不替代
[`device_contract_evidence_ladder_v0.md`](device_contract_evidence_ladder_v0.md)。

它只回答一个更操作化的问题：

> **如果下一步开始补证据，哪些契约最值得先做，第一张票应该是什么。**

相关入口：

- [`device_contract_admission_matrix_v0.md`](device_contract_admission_matrix_v0.md)
- [`device_contract_evidence_ladder_v0.md`](device_contract_evidence_ladder_v0.md)
- [`interface_admission_policy.md`](interface_admission_policy.md)

## 1. 排序原则

晋级队列按下面顺序排序：

1. 最能验证设备窄腰路线
2. 最少碰撞当前活跃路线
3. 最容易形成 no-hardware evidence
4. 最能复用已有代码胚胎
5. 最能产生 system compiler / artifact / evidence 投影

当前不按“哪个外设最常见”排序。

原因是 Charm 现在最缺的不是更多外设名，而是可复用的证据生产模式。

## 2. 当前推荐顺序

| 优先级 | 契约 | 当前等级 | 目标 | 第一张票 |
| --- | --- | --- | --- | --- |
| P0 | I2C | `experimental` | candidate evidence | 写一个真实芯片 driver 或 probe evidence |
| P1 | SPI | `proposed` | experimental narrow chain | 设计 `SpiDevice` transaction mock，先按 [`../system/spi_device_transaction_mock_readiness_checklist_v0.md`](../system/spi_device_transaction_mock_readiness_checklist_v0.md) 收拢 producer / facts / evidence |
| P1 | GPIO | `proposed` | experimental narrow chain | 设计 `GpioInput / GpioOutput / GpioEdgeSource` mock，先按 [`../system/gpio_device_input_output_edge_readiness_checklist_v0.md`](../system/gpio_device_input_output_edge_readiness_checklist_v0.md) 收拢 producer / facts / evidence |
| P2 | Block | `proposed` | experimental narrow chain | 设计 block fault script 与 media state language |
| P2 | Stream IO | `proposed` | experimental narrow chain | 设计 non-blocking stream fault script |
| P3 | Timebase | `proposed` | facts-first narrow chain | 设计 read-only timebase facts 草案 |

这个顺序不是长期承诺，只是当前最小风险推进顺序。

## 3. P0: I2C candidate evidence

### 当前状态

I2C 已经是第一条 `experimental` 样板链。

已有：

- driver-facing contract
- transaction mock
- HAL adapter backend
- register device 准 driver
- contract-local facts
- no-hardware smoke
- artifact report / fact evidence 投影样例

### 第一张票

优先补一个真实芯片 driver 或 probe evidence。
真实或准真实 I2C board/probe evidence 接入前，先按
[`../system/i2c_board_probe_evidence_readiness_checklist_v0.md`](../system/i2c_board_probe_evidence_readiness_checklist_v0.md)
检查 producer、facts、sidecar、baseline 与验收入口。

推荐候选：

- EEPROM / register-like memory device
- sensor ID probe
- PMIC register probe
- codec register probe

选择标准：

- 寄存器协议简单
- 只依赖 `io.device_i2c`
- 可用 transaction mock 先验证
- 将来可以接真实板级 evidence

### 验收标准

- driver 不依赖 HAL、BoardCaps、mock 内部类型或 platform header
- host mock smoke 能验证成功路径和至少一个失败路径
- 文档记录它是准真实或真实 driver evidence
- 不把 I2C 升级为 `candidate`，除非同时补齐真实硬件或 probe evidence

## 4. P1: SPI experimental narrow chain

### 当前状态

SPI 仍是 `proposed`。

已有：

- controller-facing `hal_spi`
- `hal::SpiBinding`
- `block::SpiFlashBinding` 经验
- SPI proposed card

但这些不能作为 driver-facing SPI contract evidence。

### 第一张票

先设计 `SpiDevice` transaction mock。

建议第一张票只覆盖：

- transaction begin / end
- write bytes
- read bytes
- transfer bytes
- chip select asserted / deasserted expectation
- flush expectation
- backend failure

暂不覆盖：

- quad / dual / octal
- DMA
- memory mapped mode
- async
- timeout

### 验收标准

- mock 语义能证明 driver 不手动管理 CS
- transaction 返回前 device/bus 处于定义状态
- 错误能映射到公共 SPI 错误草案
- 可支持一个 SPI NOR ID probe 或 display command driver

## 5. P1: GPIO experimental narrow chain

### 当前状态

GPIO 仍是 `proposed`。

已有：

- controller-facing `hal_gpio`
- input layering decision
- GPIO proposed card

但这些不能作为 driver-facing GPIO contract evidence。

### 第一张票

先设计三面 mock：

- `GpioInput`
- `GpioOutput`
- `GpioEdgeSource`

建议第一张票只覆盖：

- read level
- write level
- edge occurrence script
- invalid pin / detached / direction mismatch

暂不覆盖：

- debounce
- click / long press
- UI intent
- repeat
- focus navigation

### 验收标准

- LED output evidence 只依赖 output contract
- button input evidence 只依赖 input / edge contract
- edge source 不在 ISR 中执行完整上层逻辑
- input service 边界保持清楚

## 6. P2: Block experimental narrow chain

### 当前状态

Block 仍是 `proposed`。

已有：

- `fs::BlockDevice`
- `block.registry`
- `DeviceSlotExport`
- VFS / USB MSC / runtime slot 经验
- Block proposed card

这些是很强的装配经验，但不能单独升级等级。

### 第一张票

先设计 block fault script 与 media state language。

建议第一张票只覆盖：

- read success
- read fault
- write protect
- detach
- flush fault
- invalid geometry
- media missing

暂不覆盖：

- partition parser
- filesystem policy
- cache replacement policy
- DMA-safe buffer
- async storage

### 验收标准

- mock / script 能验证 detached 后不悬挂
- media state 能解释 missing / detached / attached / write_protected
- 错误不只是 `fs::Errc` 字面转发，而有 block domain taxonomy 草案
- facts 能记录 geometry / endpoint / backend / media

## 7. P2: Stream IO experimental narrow chain

### 当前状态

Stream IO 仍是 `proposed`。

已有：

- `io::Channel`
- `io_channel_contract.md`
- `io.registry`
- `io.reactor`
- `ChannelSlotExport`
- runtime channel smoke
- Stream IO proposed card

这些证明 IO 纪律很强，但还不是公共 Stream IO admitted contract。

### 第一张票

先设计 non-blocking stream fault script。

建议第一张票只覆盖：

- read would_block
- write would_block
- short read
- short write
- closed
- detached
- flush busy
- flush unsupported

同时记录：

- `ChannelAdapter::flush` 缺失 callback 返回 `ok(0)` 的一致性缺口

暂不覆盖：

- protocol framing
- Net reactor smoke
- managed time timeout
- async stream

### 验收标准

- read/write 永不返回 `Ok(0)`
- wait 由 reactor / scheduler / EDA 推进
- protocol helper 不 busy-spin
- fault script 可支持 line reader 或 frame codec evidence

## 8. P3: Timebase facts-first chain

### 当前状态

Timebase 仍是 `proposed`。

已有：

- `charm.system.clock`
- host steady time source
- manual time source
- `hal_timer`
- kernel timer queue
- RK3506 timer IRQ smoke
- Timebase proposed card

这些分别属于不同层，不应合成一个过大的 time contract。

### 第一张票

先设计 read-only timebase facts 草案。

建议第一张票只覆盖：

- monotonic
- resolution
- unit
- wrap behavior
- ISR-readable
- task-readable
- managed / unmanaged
- replay-controlled / not replay-controlled

暂不覆盖：

- sleep
- periodic tick
- timer queue
- Vivid replay
- global deterministic time

### 验收标准

- facts 能表达 driver timeout dependency
- `Clock` read-only 语义与 runtime sleep 分界明确
- busy-wait sleep 不被提升为公共等待模型
- manual time source 可作为 mock evidence 的候选

## 9. 当前不排队的事

下面这些事暂时不进入晋级队列：

- 把所有契约一次性升到 `experimental`
- 给所有契约同时写代码
- 做统一大 HAL
- 做 SVD / PAC 生成链
- 做 managed time 宇宙
- 做完整资源证明
- 重构目录结构
- 修改 CMake 组织方式

这些方向可以很重要，但不是当前设备窄腰准入台账的下一步。

## 10. 当前结论

下一步最值当的路线不是“继续扩列表”，而是让一条契约真正沿证据阶梯上爬。

当前最优先的是：

```text
I2C:
  experimental -> candidate evidence

SPI / GPIO:
  proposed -> experimental narrow chain
```

Block、Stream IO、Timebase 继续保持队列中位，等前两类证据生产模式跑顺后再推进。
