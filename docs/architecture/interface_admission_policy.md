# Implementation Interface Review

## 文档角色

本文是 `supporting` 的实现接口审查规则，适用于 driver、backend、middleware 和 platform 之间需要复用的 API。它不定义 Charm Core 准入，也不建立第二套公共语言法律。

上位裁决：

- [`../../CONSTITUTION.md`](../../CONSTITUTION.md)：Core 六问与唯一裁决等级；
- [`charm_core_contract.md`](charm_core_contract.md)：Capability Contract、Requirement、Provision、Binding；
- `Interface` 在当前裁决中属于 `Implementation / Tool`。只有确需长期互操作的边界才可另行申请 `Stable Boundary`。

旧 `proposed/experimental/candidate/admitted` 流程、台账和晋级队列见 [`../archive/device-contract-admission-v0/README.md`](../archive/device-contract-admission-v0/README.md)。这些标签没有 Core 含义。

## 适用范围

使用本文审查：

- I2C/SPI/GPIO/Block/Stream/Timebase 等 driver-facing 投影；
- host fake、QEMU backend、真实板 backend 的共同调用面；
- 动态设备导出的稳定 slot/service；
- platform 与可复用 component 之间的窄接口。

不需要公共化：

- vendor SDK 和寄存器适配；
- 单板 bring-up handle；
- demo 内部 glue；
- 只有一个消费者的 project policy；
- 未形成跨实现语义的试验类型。

这些实现可以存在并被测试，但应留在其所有权目录。

## 审查问题

### 消费者

- 谁必须依赖这个行为？
- 为什么不能使用更小的已有接口？
- 消费者依赖的是行为，还是 provider 身份和配置便利？

没有真实消费者时，不建立共享接口。

### 行为与责任

- 输入、输出、不变量和失败后状态是什么？
- 谁拥有 bus、address/CS/endpoint、buffer 和 transaction？
- 谁负责互斥、flush、恢复和 teardown？
- backend 可否在不泄露 vendor 类型的情况下实现？

函数列表不能替代责任边界。

### 执行语义

- 调用是否同步完成、可能阻塞或返回 `would_block`？
- 是否允许 ISR、task、线程重入或跨 execution domain？
- timeout 由谁推进，依赖哪个 clock/reactor/scheduler？
- callback 生命周期和并发规则是什么？

不要把某个现有 Channel、HAL 或 scheduler 的局部纪律扩写成所有接口的全局规则。

### 错误

- 消费者需要区分哪些失败类别？
- backend 错误怎样映射，哪些信息可保留为 stage/context？
- 哪些失败可重试、表示 detached、违反 policy 或需要重建对象？

`util::Result/Errc` 是当前常用实现，不是所有领域接口的 Core 强制。`bool` 也不是自动禁止；关键是失败语义是否足够且可测试。

### 资源与平台事实

- 是否需要 heap、DMA、IRQ、clock、power、cache coherence 或特定 memory region？
- 哪些属于 Project Fact，哪些必须在启动前解析？
- 缺少资源时怎样稳定失败？

资源 facts 和 system compiler projection 只有在真实消费者需要时才引入，不是每个接口的固定文档负担。

### 生命周期

- 创建、绑定、启用、detach、unexport 和销毁由谁负责？
- 已发放 handle 在 target 消失后怎样表现？
- API/ABI 兼容范围和废弃方式是什么？

动态设备优先参考 [`driver_model.md`](driver_model.md) 的 stable-slot 边界。

## 证据

证据按问题组合，不使用自动晋级状态机：

| 证据 | 用途 |
|---|---|
| contract-local fake/mock | 验证行为、错误和 transaction 顺序 |
| 第二个独立 backend | 暴露第一个实现偶然泄漏的语义 |
| 真实 consumer/driver | 证明接口由消费需求驱动 |
| host smoke | 快速验证平台无关语义 |
| QEMU smoke | 验证固件形态和可仿真的机器行为 |
| real-board evidence | 验证时序、DMA、cache、IRQ 和外设事实 |
| negative fixture | 验证缺失、冲突、detach 和错误映射 |

两个 backend、一个 smoke 或大量文档都不会自动把接口提升为 Core。若申请 `Stable Boundary` 或 Core，仍必须按 Constitution 单独裁决。

## Review Record

共享接口的专题文档保持短记录：

- scope 与消费者；
- 行为和非目标；
- ownership/lifecycle；
- execution/error/resource 语义；
- 实现与证据链接；
- 未覆盖风险；
- Constitution 裁决（如有）。

不要再维护跨文件的 maturity matrix、evidence ladder 和 promotion queue。排期放 issue/tracking；结果放专题契约和 evidence log。

## 当前 device 草案

以下是 implementation exploration，不因出现在列表中成为公共 ABI：

- [`i2c_device_contract_v0.md`](i2c_device_contract_v0.md)
- [`spi_device_contract_v0.md`](spi_device_contract_v0.md)
- [`gpio_device_contract_v0.md`](gpio_device_contract_v0.md)
- [`block_device_contract_v0.md`](block_device_contract_v0.md)
- [`stream_io_device_contract_v0.md`](stream_io_device_contract_v0.md)
- [`timebase_device_contract_v0.md`](timebase_device_contract_v0.md)

这些文档中的 `proposed/experimental/candidate` 是旧流程的本地证据标签，只能帮助理解当时进度，不能替代当前代码和裁决。

## 非目标

- 不建立统一 Driver 基类或全局 Interface Registry。
- 不要求所有设备共享同一错误 taxonomy、执行模型或生命周期。
- 不把 system compiler/artifact/explain 作为接口成立前提。
- 不把 host mock 当作真板证据。
- 不把 interface review 重新包装成 Core admission。
