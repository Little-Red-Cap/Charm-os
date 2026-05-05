# I2C Device Contract v0

## 定位

本文记录 Charm 当前第一条设备契约窄腰的 experimental 快照。

它不是 admitted 公共 ABI，也不是完整 HAL 规范。

它回答一个更窄的问题：

> **一个 I2C 外设 driver 在不理解 HAL、BoardCaps、mock 或 runtime discovery 的前提下，当前最小可以依赖什么。**

当前代码落点：

- `Modules/io/device/io.device_i2c.cppm`
- `Modules/io/device/io.device_i2c_facts.cppm`
- `Modules/io/device/io.device_i2c_mock.cppm`
- `Modules/io/device/io.device_i2c_hal.cppm`
- `Modules/io/driver/driver.i2c_register_device.cppm`
- `Modules/io/driver/driver.i2c_whoami_probe.cppm`

当前 smoke 证据：

- `Examples/io/i2c_contract_mock_smoke`
- `Examples/io/i2c_facts_smoke`
- `Examples/io/i2c_hal_adapter_smoke`
- `Examples/io/i2c_register_driver_smoke`
- `Examples/io/i2c_whoami_probe_smoke`

## 1. 当前等级

当前等级是 `experimental`。

它也是 [`device_contract_admission_matrix_v0.md`](device_contract_admission_matrix_v0.md)
中的第一张样板卡。

它已经具备：

- 一个 driver-facing contract：`io.device_i2c`
- 一个只读 fact vocabulary：`io.device_i2c_facts`
- 一个 transaction mock backend：`io.device_i2c_mock`
- 一个 HAL adapter backend：`io.device_i2c_hal`
- 一个准真实 register driver：`driver.i2c_register_device`
- 一个准真实 probe driver：`driver.i2c_whoami_probe`
- 五条 no-hardware smoke
- 两个 `system_compiler.fact_evidence/v0` sidecar 投影样例

它还不是 `candidate`，因为仍然缺：

- 真实硬件 evidence
- 真实芯片 driver
- 板级真实 probe evidence
- board bringup evidence
- evidence pipeline 里的真实板级 facts/probe 闭环
- 更完整的资源与执行语义冻结

## 2. Contract Shape

当前 I2C 窄腰由两层组成：

- `I2cBus`
- `I2cDeviceRef`

`I2cBus` 表示能对某个地址完成基本 transaction 的 bus backend。

当前要求：

```cpp
write(address, tx) -> util::Result<void>
read(address, rx) -> util::Result<void>
write_read(address, tx, rx) -> util::Result<void>
```

`I2cDeviceRef` 绑定一个 `I2cBusRef` 与一个 `I2cAddress`。

driver 作者优先依赖 `I2cDeviceRef`，而不是直接持有 HAL handle。

这意味着 driver 只表达：

```text
我需要和这个 I2C endpoint 做 transaction
```

而不是表达：

```text
我知道这个 endpoint 来自哪个 MCU、哪个 HAL、哪个 mock、哪个 BoardCaps 字段
```

## 3. Ownership And Responsibility

当前 v0 只冻结最小责任边界。

`I2cBusRef` 负责：

- 保存 backend 对象与操作表
- 检查操作表是否完整
- 把 `address + tx/rx span` 转发给 backend

`I2cDeviceRef` 负责：

- 保存 endpoint address
- 让 driver 不必重复传 address
- 保持 device-facing 调用形态

backend 负责：

- 完成实际 transaction
- 把平台错误映射成 `util::Errc`
- 保持调用返回前 transaction 已结束或失败

driver 负责：

- 只依赖 `I2cDeviceRef`
- 不直接访问 HAL / mock / platform private handle
- 不自建时间源
- 不在协议层 busy-spin

当前 v0 尚未冻结：

- bus sharing / locking 语义
- 10-bit address 语义
- async / reactor-driven transaction 语义
- repeated-start 的更细粒度契约
- recovery / bus reset 责任边界

## 4. Execution Semantics

当前 I2C v0 是同步完成模型。

一次调用返回时，backend 应当已经完成下列之一：

- transaction 成功完成
- transaction 明确失败并返回 `util::Errc`
- backend 表示当前能力不支持

当前不承诺：

- ISR-safe
- reentrant
- non-blocking
- reactor-managed
- timeout 由公共 contract 托管
- managed time / replay 可控制

这不是最终目标，而是 v0 边界。

如果未来加入 timeout 或 async 语义，必须通过明确 timebase / reactor contract 进入，不能让 driver 自己写 `now_ms`、sleep 或 busy wait。

## 5. Error Semantics

当前 domain error kind 是 `io::device::I2cErrorKind`。

它用于描述公共 I2C 层能理解的错误类别：

- `ok`
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

当前映射到 `util::Errc`：

- `ok` -> `Errc::ok`
- `timeout` -> `Errc::timeout`
- `target_detached` -> `Errc::noent`
- `policy_violation` -> `Errc::invalid_arg`
- `unsupported` -> `Errc::not_supported`
- 其他 I2C fault -> `Errc::io`

HAL adapter 额外保留现有 HAL status 语义：

- `hal::Status::busy` -> `Errc::busy`
- `hal::Status::timeout` -> `Errc::timeout`
- `hal::Status::unsupported` -> `Errc::not_supported`
- `hal::Status::error` -> `Errc::io`

注意：`busy` 当前没有提升为 `I2cErrorKind`。

原因是它来自现有 controller-facing HAL 状态，尚未决定是否属于公共 I2C taxonomy。
在 contract admitted 前，不应为了单个 adapter 草率扩大公共错误语言。

## 6. Backend Evidence

### 6.1 Transaction Mock Backend

`io.device_i2c_mock` 提供固定容量 transaction script。

它验证：

- expected write
- expected read
- expected write_read
- address 匹配
- tx bytes 匹配
- rx bytes 回填
- expected backend failure 也会消费 script
- unexpected transaction 会记录 `first_script_error`

这条 backend 服务 no-hardware CI 与 driver 语义测试。

### 6.2 HAL Adapter Backend

`io.device_i2c_hal` 提供 `HalI2cBus`。

它验证：

- 现有 `hal::I2cIoHandle` 可以投影成 `I2cBus`
- driver-facing contract 不需要知道 HAL ops 形状
- HAL status 可以映射到统一 `util::Errc`
- 缺失 ops 会返回 `Errc::not_supported`

这条 backend 不等价于真实硬件 evidence。

它只证明：

```text
controller-facing HAL 可以被收束到 driver-facing I2C contract。
```

## 7. Driver Evidence

`driver.i2c_register_device` 是当前准真实 driver evidence。

它提供：

- `read_u8(reg)`
- `write_u8(reg, value)`
- `read(start_reg, rx)`
- `write(start_reg, payload)`

它只依赖：

- `io.device_i2c`
- `util.core`
- `util.error`
- `util.expected`

它不依赖：

- HAL
- mock
- BoardCaps
- init.graph
- device registry
- platform headers

它的写入 payload 上限是模板参数：

```cpp
RegisterDevice8<MaxPayload = 8>
```

这让栈缓冲容量成为编译期契约，而不是隐藏运行期分配。

`driver.i2c_whoami_probe` 是当前准真实 probe driver evidence。

它提供：

- `read_id()`
- `probe()`
- `WhoAmIProbeConfig`

它只依赖：

- `io.device_i2c`
- `util.core`
- `util.error`
- `util.expected`

它不依赖：

- HAL
- mock
- BoardCaps
- init.graph
- device registry
- platform headers

它验证的是真实 I2C 外设里非常常见的 ID / WHOAMI 寄存器探测模式。
当前它仍然是 no-hardware probe evidence，不等价于真实板级 probe evidence。
它也已经通过 `i2c-whoami-probe-evidence-smoke` 进入
`system_compiler.fact_evidence/v0` sidecar 与 artifact report 导出链，
但该投影仍明确保留 `i2c.probe.board_real` 缺口。

## 8. Smoke Evidence

当前 smoke 覆盖：

- `i2c_contract_mock_smoke`
  验证基础 contract、mock script、expected failure 与 unexpected transaction。
- `i2c_hal_adapter_smoke`
  验证同一个 register driver 经 HAL adapter backend 运行，并覆盖 `busy / timeout / unsupported` 映射。
- `i2c_register_driver_smoke`
  验证 register driver 在 transaction mock backend 上完成单寄存器与 burst 读写。
- `i2c_whoami_probe_smoke`
  验证 probe driver 在 transaction mock backend 上覆盖成功、ID mismatch 与 backend failure。
- `i2c-whoami-probe-evidence-smoke`
  验证 WHOAMI no-hardware probe evidence 可以作为 `fact_only` case
  进入 `export_bundle -> artifact_report` 链。
- `materialized_graph_i2c_whoami_probe_evidence_compare_smoke.ps1`
  合成一份 candidate evidence，把 `i2c.probe.board_real` 从
  `missing` 推到 `satisfied`，验证 artifact report / inspector
  能解释这类 probe evidence drift。

当前已验证输出形态：

```text
i2c contract mock smoke: ok
i2c hal adapter smoke: ok
i2c register driver smoke: ok
i2c whoami probe smoke: ok
```

## 9. System Compiler Projection

当前已有只读 facts 草案，并已有一份可被现有 schema 校验的 artifact report sample。
它也已经以 `fact_only` case 的形式接入
`export_case_manifest -> export_bundle -> artifact_report` 真实导出链。
当前 facts 不再主要依赖 manifest 字面量，而是由
`system_compiler.fact_evidence/v0` sidecar 进入 bundle 后再投影到 artifact report。
这仍然只是报告证据，不会阻断构建。

当前 WHOAMI probe evidence 同样走这条 `fact_only` sidecar 路径。
它把 no-hardware probe 已有证据与真实板级 probe 缺口放进同一份事实库存：

- `i2c.evidence:whoami_probe` 已提供
- `i2c.probe.board_real` 缺失

这让 artifact report 能解释“为什么它还不是 candidate evidence”，
而不是只停留在 smoke 输出文本。
其中 `i2c.register:*`、`i2c.expected_id:*` 与 `i2c.probe.*`
当前只是 WHOAMI probe sidecar 的 probe-local evidence fact names，
不是 admitted I2C contract vocabulary。

当前还有一条 compare smoke 会在不改变 graph 的前提下合成
`i2c.probe.board_real` 的 `board.bringup` provider。
它证明的是 artifact report 对 probe evidence 缺口闭合的解释能力：
baseline 中该 fact 为 `missing`，candidate 中该 fact 为 `satisfied`。
这仍不等价于真实硬件 evidence，也不会把 I2C 从 `experimental`
升级为 `candidate`。

`io.device_i2c_facts` 当前定义了最小 fact vocabulary：

- `i2c.bus`
- `i2c.controller`
- `i2c.device`
- `clock.domain`
- `pinmux`
- `power.domain`
- `i2c.backend`
- `i2c.evidence`

每条 fact 当前至少带出：

- kind
- required / optional
- provided / missing / unknown
- address
- name
- source

它可以形成最小 `I2cFactResolution`：

- required count
- provided count
- missing count
- optional unknown count
- satisfied / unsatisfied

当前 smoke 覆盖：

```text
i2c facts smoke: ok
required=6 provided=5 missing=1 optional_unknown=1
```

这仍然是 contract-local 草案，不是全局 fact engine。

当前 artifact report sample：

- `schemas/examples/system_compiler.artifact_report.v0.i2c_facts.sample.json`
- `schemas/examples/system_compiler.fact_evidence.v0.i2c_facts.sample.json`

当前真实导出 case：

- `i2c-device-contract-facts-smoke`

这份样例把 I2C facts 投影进现有字段：

- `artifacts.fact_evidence`
- `structure.declared_facts`
- `structure.required_facts`
- `resource_contract.provided_facts`
- `fact_resolution.fact_inventory`
- `fact_resolution.required_fact_resolution`
- `fact_resolution.resource_hotspots`

其中 `pinmux:pb8/pb9.af4` 在 sidecar 中被保留为 required 但未 available，
用于表达“contract 已经知道需要这个事实，但当前报告仍缺证据”。
`required_fact_resolution` 会进一步说明每条 required fact 当前是 `satisfied` 还是 `missing`，
以及它来自哪个 fact source bucket；如果 `fact_evidence.raw_facts` 里存在 provider，
报告还会带出对应的 `source / role / kind`。

这一步仍然只做报告投影，不做构建期强制。

未来该 contract 至少应进一步进入以下 system compiler 事实语言：

```text
required facts:
  i2c.bus exists
  i2c.device(address = X) exists or is declared
  controller clock domain is enabled before use
  pinmux facts are satisfied if applicable
  power domain is on if applicable

binding result:
  driver endpoint resolved to backend
  backend resolved to controller capability
  unresolved address / controller / clock / pinmux can be reported

evidence:
  mock script evidence
  HAL adapter smoke evidence
  I2C fact resolution evidence
  no-hardware WHOAMI probe evidence
  WHOAMI fact_evidence sidecar
  board bringup evidence
  real driver probe evidence
```

这正是 Charm 和普通 HAL contract 的分野：

```text
普通 HAL:
  driver 可以调用

Charm:
  driver 可以调用
  系统也能解释这个调用条件为什么成立
```

## 10. Non-goals

当前阶段明确不做：

- 不把 I2C v0 宣布为 admitted 公共 ABI
- 不承诺 async / timeout / managed time 语义
- 不承诺 ISR-safe
- 不处理 bus recovery / reset
- 不处理 10-bit address
- 不处理 bus sharing / locking
- 不把 HAL adapter 当作真实硬件 evidence
- 不让 driver 直接依赖 HAL 或 BoardCaps
- 不为了单个 backend 扩大公共错误 taxonomy

## 11. Next Steps

最值当的下一步是：

1. 把 no-hardware `driver.i2c_whoami_probe` 继续接到真实芯片或板级 probe。
   例如 sensor / EEPROM / codec / PMIC。
2. 把当前 smoke 级 `fact_evidence` sidecar 推进到更真实的 evidence pipeline
   当前 board/package fact source 已由 `board-package-facts-smoke` 接入，
   I2C contract-required facts 与 board/package/adapter audit facts 的组合
   也已由 `board-i2c-fact-composition-smoke` 接入；
   WHOAMI no-hardware probe evidence 也已由 `i2c-whoami-probe-evidence-smoke`
   接入 artifact report；
   其 `i2c.probe.board_real` 缺口闭合路径也已有 compare smoke 钉住；
   下一步更适合继续推进真实 probe evidence 或 board bringup evidence，不做执法。
3. 评估是否需要 `I2cDevice` ownership type
   用于未来 bus sharing / lock / transaction 边界。
4. 持续同步 [`device_contract_admission_matrix_v0.md`](device_contract_admission_matrix_v0.md)，
   不在证据补齐前把 I2C 升级为 `candidate`。

在这些完成前，I2C 仍保持 `experimental`。
