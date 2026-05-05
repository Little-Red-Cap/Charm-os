# 设备契约证据阶梯 v0

## 定位

本文是设备契约准入台账的证据阶梯。

它不定义新的 C++ API，不升级任何契约等级，也不替代
[`interface_admission_policy.md`](interface_admission_policy.md) 的准入政策。

它只回答一个执行问题：

> **一张 proposed contract card 接下来要补哪些证据，才有资格进入 experimental / candidate / admitted 的讨论。**

当前台账入口是：

- [`device_contract_admission_matrix_v0.md`](device_contract_admission_matrix_v0.md)

当前路线总览是：

- [`device_contract_narrow_waist_v0.md`](device_contract_narrow_waist_v0.md)

当前下一批证据工作的优先队列是：

- [`device_contract_promotion_queue_v0.md`](device_contract_promotion_queue_v0.md)

## 1. 为什么需要证据阶梯

第一批设备契约卡已经覆盖：

- I2C
- SPI
- GPIO
- Block
- Stream IO
- Timebase

这些卡的价值不是把接口一次性立法完，而是先把每个契约的保护对象、责任边界、执行语义、错误语言、facts 与证据缺口摆上桌面。

下一步如果继续只写单卡，很容易出现两个问题：

- 每条契约各自发明升级标准。
- proposed / experimental / candidate 的判断变成临场感觉。

证据阶梯的作用是把升级路线变成同一把尺子。

## 2. 等级不等于成熟度口号

### 2.1 `proposed`

`proposed` 表示：

```text
我们知道为什么需要这个公共契约，
也知道它不应该直接等同于现有 HAL / registry / demo。
```

最低证据：

- proposed card
- 保护对象
- 语义面
- 责任边界
- 执行语义
- 错误语言草案
- facts 草案
- 明确非目标

当前 SPI、GPIO、Block、Stream IO、Timebase 都处在这个等级。

### 2.2 `experimental`

`experimental` 表示：

```text
这个契约已经有最小可运行窄链，
可以在受控路径中验证 driver-facing 语义。
```

最低证据：

- 一个 contract-local API / facade / reference type
- 一个 backend 或 mock
- 一个 driver / component / middleware 使用者
- 一条 no-hardware smoke 或 bringup evidence
- 错误路径不再只是 `bool` 或平台私有整数
- 执行语义已经能被测试或观察

当前 I2C 是第一张 experimental 样板卡。

### 2.3 `candidate`

`candidate` 表示：

```text
这个契约已经有跨后端、跨使用者、跨测试路径的证据，
可以进入公共契约层讨论。
```

最低证据：

- 至少两个 backend
- 至少一个真实或准真实 driver
- 至少一条无硬件测试路径
- 明确 domain error taxonomy
- 明确资源与执行语义
- contract-local facts 能投影到 artifact / evidence
- unresolved / missing facts 可以被报告

candidate 不是 admitted。
它只是说明新 driver 可以优先试用，但仍需要变更审查。

### 2.4 `admitted`

`admitted` 表示：

```text
这个契约已经成为 Charm 设备公共窄腰的一部分。
```

最低证据：

- 满足 candidate 要求
- 文档中有稳定语义边界
- mock / test helper 随契约维护
- artifact / explain / evidence 入口能说明它如何参与系统成立
- 有迁移与弃用规则
- 架构索引中有明确阅读入口

admitted 接口才可以作为驱动生态的公共依赖。

## 3. 证据包结构

每条契约从 proposed 往上走时，都应尽量补齐同一组证据包。

### 3.1 Contract Shape Evidence

回答：

```text
driver / component 作者实际依赖什么对象？
```

可接受形式：

- facade type
- reference type
- concept
- ops table
- endpoint wrapper
- contract-local value type

反例：

- vendor SDK handle
- board 私有结构
- demo 内部 glue
- 直接暴露 runtime discovery 字段

### 3.2 Backend Evidence

回答：

```text
这个契约能由什么后端支撑？
```

证据等级：

- mock backend
- HAL adapter backend
- hosted backend
- simulator backend
- real hardware backend
- runtime-discovered backend

experimental 至少需要一个 backend。
candidate 至少需要两个 backend。

### 3.3 User Evidence

回答：

```text
是否已经有可复用代码只依赖这个契约？
```

可接受形式：

- driver
- component
- middleware
- service adapter
- protocol adapter

优先级：

1. 真实芯片 driver
2. 准真实 driver
3. 小型 middleware
4. contract smoke helper

contract smoke helper 只能帮助进入 experimental，不能单独支撑 candidate。

### 3.4 No-hardware Evidence

回答：

```text
没有真实硬件时，契约语义能否被验证？
```

可接受形式：

- transaction mock
- loopback backend
- fault script
- fake registry / slot export
- host smoke
- QEMU smoke
- artifact report sample

无硬件 evidence 不是替代真实硬件。
它保护的是 driver 作者和 CI 路径。

### 3.5 Error Evidence

回答：

```text
失败是否能被公共语言解释？
```

最低要求：

- 不返回 `bool`
- 不只返回 vendor integer
- 能映射到 `util::Errc`
- 能区分 policy violation、target detached、unsupported、timeout 或 transport fault

candidate 前应形成 contract-local domain error taxonomy。

### 3.6 Execution Evidence

回答：

```text
调用发生在哪个执行宇宙里？
```

至少记录：

- synchronous
- non-blocking
- task-context only
- ISR-readable / ISR-notify
- reactor-managed
- scheduler-managed
- timeout 由谁推进
- reentrancy / ownership 规则

如果一条契约依赖时间，必须说明它依赖 Timebase，而不是自建时间源。

### 3.7 Facts Evidence

回答：

```text
system compiler 应该能看见什么事实？
```

最低路径：

- contract-local fact vocabulary
- required / provided / missing / optional unknown 状态
- fact resolution summary
- artifact report sample
- evidence sidecar 或等价输入

experimental 可以只有 contract-local facts。
candidate 应能投影到 artifact / evidence 链。

### 3.8 Bringup / Probe Evidence

回答：

```text
真实系统里这个契约如何成立？
```

可接受形式：

- board bringup evidence
- probe evidence
- runtime attach evidence
- capability export evidence
- materialized graph
- runtime observe sidecar

这类 evidence 不是每条契约进入 experimental 的硬前置。
但它通常是 candidate / admitted 的关键证据。

## 4. 第一批契约的下一跳

### 4.1 I2C

当前等级：`experimental`

下一跳不是继续补文档，而是补 candidate 证据：

- 真实芯片 driver
- 真实硬件或板级 bringup evidence
- probe evidence
- 更正式的 artifact / evidence 投影

I2C 不应在这些证据补齐前升级为 `candidate`。

### 4.2 SPI

当前等级：`proposed`

下一跳是 experimental 窄链：

- `SpiDevice` 责任边界
- transaction mock
- HAL adapter backend
- 准真实 driver，例如 SPI NOR ID probe 或 display command device
- no-hardware smoke

不应先追求 quad / DMA / memory mapped mode。

### 4.3 GPIO

当前等级：`proposed`

下一跳是三面最小窄链：

- `GpioInput`
- `GpioOutput`
- `GpioEdgeSource`
- level / edge mock
- LED output、button input 或 edge counter evidence

debounce、click、long press、UI intent 仍留在 input service / domain 层。

### 4.4 Block

当前等级：`proposed`

下一跳是 stable endpoint + fault mock：

- block mock / fault script
- media state language
- read fault / write protect / detach / flush fault
- contract-local block facts
- 一个准真实 storage driver 或 RAM disk fault evidence

VFS / USB MSC / registry 经验不能单独把 Block 升成 experimental。

### 4.5 Stream IO

当前等级：`proposed`

下一跳是 non-blocking stream mock：

- would_block / closed / detach / short write / flush busy script
- adapter behavior consistency audit
- contract-local stream facts
- line reader、frame codec 或 CDC echo adapter evidence

Net 相关 smoke 目前只作为背景，不作为本路线的主要证据。

### 4.6 Timebase

当前等级：`proposed`

下一跳是 read-only timebase facts：

- monotonic / resolution / wrap / context / managed facts
- manual time source mock
- timeout-aware middleware evidence
- `Clock` read-only 语义与 runtime sleep / scheduler timeout 分界

不要把 busy-wait sleep、Vivid replay 或 kernel timer queue 直接升成基础 Timebase contract。

## 5. 升级前检查

任何契约升级前，至少要同步检查：

- 台账等级是否更新
- contract card 是否写清证据变化
- 准入政策是否仍然适用
- 新证据是否真的 driver-facing
- 是否误把 HAL / demo / registry 经验当成 admitted evidence
- 是否有 no-hardware 验证路径
- 是否有错误语言和执行语义
- 是否有 facts / artifact / evidence 投影方向

如果这些问题无法回答，应保持原等级。

## 6. 当前非目标

当前阶段明确不做：

- 不升级任何契约等级
- 不新增 C++ API
- 不修改 `Modules/`
- 不修改 `Examples/`
- 不修改 `schemas/`
- 不修改 `scripts/`
- 不把 evidence ladder 当成构建期执法入口
- 不把 proposed 候选写成 admitted 公共 ABI

## 7. 当前结论

设备契约窄腰的下一步不是“写更多漂亮接口”，而是“让证据能沿同一条梯子生长”。

这能保护 Charm 的两个核心目标：

- driver 作者面对的是极小公共契约
- system compiler 面对的是可解释的系统事实
