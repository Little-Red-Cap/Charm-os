# SPI Device Contract v0

## 定位

本文记录 Charm 设备契约窄腰中的 SPI proposed contract card。

它不是 admitted 公共 ABI，也不是当前 `hal_spi` 的重命名。
它只回答一个设计前置问题：

> **一个 SPI 外设 driver 如果不想手动管理片选、互斥、flush 与 transaction 边界，未来最小应该依赖什么语义。**

当前代码中已经存在：

- `Modules/io/hal/hal_spi.cppm`
- `Modules/io/hal/hal_spi.node.cppm`
- `Modules/io/block/block.spi_flash.cppm`

但这些都不等价于本文描述的 driver-facing SPI device contract。

## 1. 当前等级

当前等级是 `proposed`。

它已经具备：

- controller-facing HAL：`hal_spi`
- 静态 controller binding：`hal::SpiBinding`
- 一个折叠式 SPI flash block binding：`block::SpiFlashBinding`
- 在窄腰总览中已经明确 `SpiBus / SpiDevice` 是高优先级候选

它还不是 `experimental`，因为仍然缺：

- driver-facing `SpiBus` / `SpiDevice` contract
- transaction mock backend
- HAL adapter backend
- 一个只依赖 SPI contract 的准真实 driver
- no-hardware smoke
- contract-local facts vocabulary
- artifact / evidence 投影样例

## 2. Contract Shape

SPI v0 的核心不是先冻结 `transfer()` 函数签名。

核心是拆清两种对象：

- `SpiBus`
- `SpiDevice`

`SpiBus` 表示对整条 SPI bus 的受管访问能力。

`SpiDevice` 表示一个带 endpoint 语义的事务性设备访问能力。

带片选的外设 driver 默认应该依赖 `SpiDevice`，而不是依赖 `SpiBus` 后自己手动处理 CS。

推荐理解：

```text
driver -> SpiDevice -> managed transaction -> SpiBus/backend
```

而不是：

```text
driver -> SpiBus + GPIO CS + ad-hoc delay/flush
```

## 3. Ownership And Responsibility

### 3.1 `SpiBus`

`SpiBus` 负责表达：

- 这个 backend 能执行 SPI transfer
- transfer 的模式、bit order、bits、clock 已经由 controller binding 或 backend policy 管理
- bus 访问是否独占、共享、或由上层 device wrapper 受管

`SpiBus` 不应该负责：

- 替 driver 猜测具体片选
- 暴露 vendor SDK handle
- 让 driver 自己拼 clock / pinmux / power facts
- 在公共层承诺所有平台都支持 DMA、quad、dual、memory mapped mode

### 3.2 `SpiDevice`

`SpiDevice` 负责表达：

- 选中哪个 endpoint / chip select
- transaction 开始时 assert CS
- transaction 结束时 flush 必要数据
- transaction 结束后 deassert CS
- 调用返回前 bus 已回到 contract 定义的 idle 边界
- 失败时能给出明确错误类别

这意味着带 CS 的外设 driver 不应直接负责：

- GPIO CS assert/deassert
- bus lock/unlock
- final flush
- idle recovery
- 片选失败后的统一错误归类

### 3.3 Backend

backend 负责：

- 把 `SpiBus` 或 `SpiDevice` 投影到真实 HAL、mock、host backend 或 runtime glue
- 映射平台错误到公共错误语言
- 保证 transaction 边界内的硬件操作一致
- 保留平台私有细节，但不泄漏到公共 contract

## 4. Transaction Boundary

SPI proposed contract 必须优先冻结 transaction 责任，而不是函数数量。

一次 `SpiDevice` transaction 至少要能说明：

```text
lock bus if needed
assert CS if owned by this device
perform one or more operations
flush if backend requires it
deassert CS
unlock bus
return with bus/device in a defined state
```

当前不决定 transaction API 形状。

可选形态包括：

- 单次 full-duplex transfer
- write-only transfer
- read-only transfer
- operation list / transaction list
- command + payload + response helper

但无论 API 怎么演化，都不能把 CS 与 flush 责任重新推给每个 driver。

## 5. Execution Semantics

当前 SPI proposed contract 暂定为同步完成模型。

一次调用返回时，backend 应完成下列之一：

- transaction 成功完成
- transaction 明确失败并返回公共错误
- backend 表示能力不支持

当前不承诺：

- ISR-safe
- reentrant
- non-blocking
- reactor-managed
- DMA-backed
- timeout 由公共 contract 托管
- managed time / replay 可控制

如果未来需要 async、DMA 或 timeout，必须通过明确 reactor / timebase / resource contract 进入。

## 6. Error Semantics

SPI 公共错误语言尚未冻结。

candidate taxonomy 至少应考虑：

- `bus`
- `mode_fault`
- `overrun`
- `chip_select_fault`
- `timeout`
- `target_detached`
- `policy_violation`
- `unsupported`
- `unknown`

平台错误可以更具体，但公共 driver 不应只收到 `false`、裸整数或 vendor status。

在 `experimental` 前，不应为了单一 backend 草率扩大错误 taxonomy。

## 7. Facts

SPI proposed contract 未来至少需要能投影下面 facts：

- `spi.bus`
- `spi.controller`
- `spi.device`
- `spi.chip_select`
- `clock.domain`
- `pinmux`
- `power.domain`
- `dma.channel`
- `spi.backend`
- `spi.evidence`

这些 facts 在 v0 不做构建期执法。

它们应先服务：

- admission record
- artifact report
- evidence sample
- explain / unresolved binding 入口

## 8. Evidence Gaps

当前缺口明确保留：

- 没有 transaction mock
- 没有 HAL adapter backend
- 没有准真实 SPI driver
- 没有 no-hardware smoke
- 没有 `system_compiler.fact_evidence/v0` sidecar
- 没有 board/probe/bringup evidence

现有 `block::SpiFlashBinding` 只能作为“SPI 相关静态 binding 经验”参考。
它不能证明 SPI device contract 已经 experimental。

## 9. Non-goals

当前阶段明确不做：

- 不新增 `Modules/io/device/io.device_spi.cppm`
- 不修改 `hal_spi`
- 不重构 `block.spi_flash`
- 不宣布 SPI contract 为 `experimental`
- 不承诺 quad/dual/octal SPI
- 不承诺 memory mapped SPI
- 不承诺 DMA
- 不承诺 async / timeout / managed time
- 不把 CS 暴露为每个 driver 的私有 GPIO 手艺活

## 10. Next Steps

最值当的下一步是：

1. 保持本文件为 `proposed` card。
2. 先设计 transaction mock 的脚本语义，但不急着写代码。
3. 选择一个小型准真实 driver 作为 future evidence，例如 SPI NOR ID probe、display command device、sensor register device。
4. 再决定 `SpiDevice` 是否需要 operation list，还是先用最小 transfer helper。
5. 与 [`device_contract_admission_matrix_v0.md`](device_contract_admission_matrix_v0.md) 同步准入状态。

在这些完成前，SPI 仍保持 `proposed`。
