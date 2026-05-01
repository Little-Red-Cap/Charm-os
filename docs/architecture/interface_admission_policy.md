# 接口准入政策：公共契约不是能力集合

## 定位

本文用于定义 Charm 中“公共接口契约”进入正式层级前必须满足的证据门槛。

这里讨论的对象包括但不限于未来可能出现的：

- `charm.device.concepts`
- `charm.io.concepts`
- `charm.contract`
- 面向驱动、组件与 middleware 作者的最小公共语义面

本文不定义某个具体 C++ API，也不宣布某组接口已经稳定。
它定义的是一条更重要的规则：

> **公共接口不是能力集合，而是生态债务。**

只要一个接口进入公共契约层，它就会长期约束驱动作者、后端实现者、测试替身、文档、示例、system compiler 事实投影与未来迁移成本。

因此 Charm 的原则是：

> **实现可以大胆，契约必须吝啬。**

## 1. 为什么需要准入政策

Charm 现在已经有多条强主线：

- `init.graph` 与 system compiler 结果物
- `BoardCaps` 与 capability 装配
- 静态 capability 平面与动态 discovery 平面
- runtime export / observe / explain
- 资源契约、bringup evidence 与 artifact report

这意味着 Charm 不应该把“看起来有用的接口”直接塞进公共层。

公共接口一旦过宽，会导致：

- 后端被迫模拟并不属于自己的语义
- 驱动作者依赖了无法跨平台成立的细节
- mock 测试只能检查调用形状，不能检查责任边界
- system compiler 无法判断这个接口需要哪些事实、资源和时间语义
- 后续移植不得不兼容历史上过早承诺的错误边界

准入政策要保护的不是抽象洁癖，而是 Charm 未来形成驱动生态时的可信度。

## 2. 适用范围

本文适用于准备进入公共契约层的接口，例如：

- GPIO 输入/输出/边沿源
- I2C bus / device
- SPI bus / device
- UART / stream IO
- block device
- timebase
- display surface
- input source
- power domain
- sensor source

本文不适用于：

- 平台私有 HAL 细节
- 某个 demo 内部临时适配器
- 尚未形成跨后端证据的实验代码
- vendor SDK 的直接包装
- 只服务单一 board bringup 的临时对象

这些对象可以存在，也可以很有价值，但不应因此自动进入公共契约层。

## 3. 准入等级

### 3.1 `proposed`

表示接口有明确动机，但还只是设计提案。

最低要求：

- 写清要保护哪类作者或调用方
- 写清它暴露的语义面，而不只是函数形状
- 写清为什么不能直接使用已有 capability / registry / driver model
- 写清至少一个预期后端和一个预期驱动

`proposed` 不应被稳定驱动依赖。

### 3.2 `experimental`

表示接口已经有最小实现，允许在受控路径里验证。

最低要求：

- 至少一个真实或 mock backend
- 至少一个使用该接口的 driver / component / middleware
- 至少一个 host 测试、mock transaction 或 bringup evidence 路径
- 明确阻塞、轮询、EDA、ISR、reentrancy 的当前边界
- 错误路径不再只是 `bool` 或平台私有整数

`experimental` 可以用于示例和试点，但不应被当成长期 ABI。

### 3.3 `candidate`

表示接口已经具备进入公共契约层的证据。

最低要求：

- 至少两个 backend
  例如 STM32 / PC mock、bare-metal / hosted、真实硬件 / simulator
- 至少一个真实或准真实 driver
  例如 sensor、codec、display、storage、radio、PMIC
- 至少一条无硬件测试路径
  例如 transaction mock、loopback、host CI、QEMU smoke
- 明确错误 taxonomy，并能映射到 `util::Errc` 或更窄的 domain error kind
- 明确执行语义
  包括是否可阻塞、是否可从 ISR 调用、是否可重入、是否由 reactor / scheduler 托管
- 明确资源语义
  包括是否需要 heap、DMA、clock、IRQ、power domain、monotonic clock
- 明确 system compiler 可见的事实投影
  包括 required facts、provided facts、capability export 与 evidence 入口

`candidate` 可以被新驱动优先依赖，但仍需要变更审查。

### 3.4 `admitted`

表示接口已经进入 Charm 公共契约层。

最低要求：

- 满足 `candidate` 的所有要求
- 文档中有稳定语义边界
- 至少有一个 mock / test helper 随契约一起维护
- 至少有一个 artifact / explain / evidence 入口能说明它如何参与系统成立
- 已在架构索引中标明阅读入口
- 已有迁移与弃用规则

`admitted` 接口可以作为驱动生态的公共窄腰。

### 3.5 `deprecated`

表示接口曾经进入公共契约层，但已不推荐新代码使用。

最低要求：

- 写清替代接口
- 写清废弃原因
- 写清保留周期或移除条件
- 若影响 system compiler / artifact / explain 结果物，应同步说明兼容策略

## 4. 准入检查表

一个接口从 `proposed` 走向 `candidate` 前，至少要回答下面问题。

### 4.1 它保护谁

必须明确主要受益者：

- driver 作者
- component 作者
- middleware 作者
- board port 作者
- runtime glue 作者
- application 作者

公共设备契约默认优先保护 driver / component / middleware 作者。

原因是这些作者贡献的是可复用资产。
如果他们必须理解具体 STM32Cube、PC mock、USB glue 或 board handle，生态就无法形成。

### 4.2 它抽象什么责任边界

接口不能只回答“有哪些函数”。

它还必须回答：

- 谁拥有总线
- 谁拥有片选 / 地址 / endpoint
- 谁负责互斥
- 谁负责 transaction 边界
- 调用返回前硬件是否必须 idle
- 是否允许 pipelining
- 谁负责 flush
- 谁负责 clock / power / IRQ enable
- 失败后对象处于什么状态

这类责任边界比函数名更重要。

例如 SPI 不应只讨论 `read/write/transfer`，而要区分：

- `SpiBus`
  表示对整条 bus 的独占或受管访问能力
- `SpiDevice`
  表示对某个片选设备的事务性访问能力

驱动作者面对带 CS 的设备时，通常应该依赖 `SpiDevice`，而不是手动管理 CS。

### 4.3 它的执行语义是什么

必须显式说明：

- 是否可能阻塞
- 是否返回 `Errc::would_block`
- 是否要求由 `io.reactor` 推进
- 是否要求 `charm.system.clock`
- 是否允许 ISR 调用
- 是否允许 task context 调用
- 是否允许嵌套调用
- 是否可重入
- 是否可能跨 execution domain

如果接口依赖时间，不得自建 `now_ms` / `now_us`。
时间必须通过统一时间源或明确的 timebase contract 进入。

### 4.4 它的错误语言是什么

平台错误可以具体，公共错误必须可归类。

一个合格契约至少要说明：

- domain error kind
- 如何映射到 `util::Errc`
- 哪些错误表示可重试
- 哪些错误表示设备失活
- 哪些错误表示 policy violation
- 哪些错误应进入 trace / evidence

例如 I2C 契约可以区分：

- `bus`
- `arbitration_lost`
- `nack_address`
- `nack_data`
- `overrun`
- `timeout`
- `peripheral_fault`
- `policy_violation`
- `unknown`

公共驱动不应只收到 `HAL_ERROR` 或 `false`。

### 4.5 它需要哪些系统事实

接口准入时必须说明 system compiler 应该能看见哪些事实。

典型事实包括：

- controller 是否存在
- bus / endpoint / address 是否存在
- clock domain 是否已提供
- IRQ line 是否已绑定
- DMA stream 是否可用
- pin alternate function 是否冲突
- power domain 是否可控
- memory region 是否适合 DMA
- capability name / registry slot 如何导出

这些事实不一定都在 v0 立刻强制检查。
但如果一个接口无法说明它依赖哪些事实，就不应进入公共契约层。

### 4.6 它如何被无硬件测试

进入公共层的接口必须有 mock 或 no-hardware 验证路径。

可接受形式包括：

- transaction mock
- loopback backend
- simulator backend
- QEMU smoke
- host CI
- fake registry / slot export
- artifact report sample

目标不是让 mock 替代硬件，而是让 driver 语义可以在没有硬件时被验证。

### 4.7 它如何进入 evidence / explain

如果接口会参与系统成立，它必须能被解释。

至少要能回答：

- 当前 capability 是否已声明
- 当前 binding 是否 resolved
- 当前 bringup order 中它依赖谁
- 当前 facts 是否满足
- 当前失败原因是什么
- 当前 mock / hardware evidence 来自哪里

这和 `artifact report`、`explain surface`、`bringup evidence` 是同一条线。

## 5. 不允许进入公共契约层的内容

下面这些内容默认不得进入公共契约层：

- vendor SDK 类型泄漏
- 寄存器地址或裸寄存器字段
- 单一 board 的私有 handle
- 隐式全局状态
- 隐式默认通道
- 自建时间源
- `bool` 风格错误
- 未说明 ISR / blocking / reentrancy 的接口
- 必须依赖 heap 才能工作的基础设备接口
- 只有一个 backend、没有 driver、没有测试路径的抽象

这些内容可以留在 platform / backend / experiment 层，但不能成为生态共同语言。

## 6. 与现有 Charm 文档的关系

- [`driver_model.md`](driver_model.md)
  定义静态 capability 平面与动态 discovery 平面的驱动模型。
- [`device_model_overview.md`](device_model_overview.md)
  保留 runtime discovery 设备模型的草案与补充说明。
- [`system_compiler_vocabulary_v0.md`](system_compiler_vocabulary_v0.md)
  定义 `SystemSpec / Profile / BoardPackage / Binding / Facet` 等系统编译器词汇。
- [`../system/artifact_report_v0.md`](../system/artifact_report_v0.md)
  定义系统编译器导出的结果物语言。
- [`../system/explain_surface_v0.md`](../system/explain_surface_v0.md)
  定义人和工具继续追问系统事实的问题面。
- [`../system/resource_contract_v0.md`](../system/resource_contract_v0.md)
  定义资源法律语言。
- [`../system/bringup_evidence_pipeline_v0.md`](../system/bringup_evidence_pipeline_v0.md)
  定义 bringup 证据流水线。

本文补上的不是第三条驱动平面，而是“公共接口如何成为可信契约”的准入规则。

## 7. 第一批建议候选

当前不建议一次性把所有外设接口都立法。

更稳的顺序是：

1. I2C transaction mock + 一个小型真实 driver
2. SPI `Bus / Device` 责任边界拆分
3. GPIO `Input / Output / EdgeSource`
4. `BlockDevice` 与稳定 slot / live state 对齐
5. `StreamIo` 与现有 `io::Channel` 非阻塞纪律对齐
6. `Timebase` 与 `charm.system.clock` / managed time 对齐

第一批候选应该优先选择能同时验证下面三件事的接口：

- driver 作者只依赖公共契约
- backend 可以是真实硬件，也可以是 host mock
- system compiler 能看到 required facts、binding result 与 evidence 入口

当前状态：

- I2C 已经有第一条 `experimental` 窄链：
  `io.device_i2c` + `io.device_i2c_mock` + `Examples/io/i2c_contract_mock_smoke`
- 它验证了 mock backend、准 driver 和 no-hardware smoke
- 它还不能升级为 `candidate`，因为还缺至少第二个 backend、真实 driver 与 system compiler facts/evidence 投影

## 8. 当前结论

Charm 可以大胆推进系统编译器、托管时间、bringup evidence、mock backend 与真实驱动。

但公共接口层必须慢一点、窄一点、硬一点。

> **Charm 不是只提供驱动 API，而是提供驱动接口进入生态的法律。**

当一个接口能跨后端、跨测试、跨 driver，并且能被 system compiler 看见、被 artifact report 导出、被 explain surface 追问时，它才值得进入公共契约层。
