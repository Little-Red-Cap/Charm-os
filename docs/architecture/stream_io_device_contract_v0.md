# Stream IO Device Contract v0

## 定位

本文记录 Charm 设备契约窄腰中的 Stream IO proposed contract card。

它不是 admitted 公共 ABI，也不是当前
[`../io/io_channel_contract.md`](../io/io_channel_contract.md)
的替代品。

它只回答一个更窄的问题：

> **一个 stream-like driver、protocol adapter 或 runtime channel glue 如果要成为可复用设备契约，未来最小应该承诺哪些非阻塞、等待、错误、生命周期和 facts 语义。**

当前代码中已经存在：

- `Modules/io/channel/io.channel.cppm`
- `Modules/io/channel/io.channel.node.cppm`
- `Modules/io/channel/io.channel.slot.cppm`
- `Modules/io/channel/io.channel.slot_export.cppm`
- `Modules/io/registry/io.registry.cppm`
- `Modules/io/reactor/io.reactor.cppm`
- `Modules/io/usb/host/usb.host.runtime_channel.cppm`

这些说明 Charm 已经有比较明确的 channel / registry / reactor 纪律。

但它们仍然不等价于一个已经 admitted 的公共 driver-facing Stream IO device contract。

## 1. 当前等级

当前等级是 `proposed`。

它已经具备：

- `io.channel` 的 read / write / flush 基础形状
- `io_channel_contract.md` 的非阻塞硬规则
- `io.registry` 的 endpoint 注册与 `PublishState`
- `io.reactor` 的 notify / drain 分工
- `io::ChannelBinding` 的 init.graph 静态装配入口
- `io::ChannelSlot` 的稳定转发槽位
- `io::ChannelSlotExport` 的 `missing / detached / attached` 导出状态
- runtime channel slot demo
- USB Host CDC runtime channel smoke

它还不是 `experimental`，因为仍然缺：

- 面向 driver / component 作者的正式 Stream IO contract 责任卡
- 专门服务公共契约的 stream mock / fault script
- contract-local stream facts vocabulary
- 更窄的 Stream domain error taxonomy
- adapter 行为一致性盘点
- artifact / evidence pipeline 中正式的 facts 投影
- 一个只依赖该 contract 的准真实 stream driver / middleware evidence

## 2. Contract Shape

当前 v0 不新增 C++ API。

Stream IO proposed contract 的最小语义面应围绕下面对象收敛：

- `StreamEndpoint`
- `StreamChannel`
- `StreamEventSource`

`StreamChannel` 表示一个 byte-oriented read/write endpoint。

它至少需要表达：

- `read(out)`
- `write(in)`
- `flush()`
- endpoint caps
- non-blocking result

`StreamEndpoint` 表示对外可消费的 capability endpoint，例如：

- `io.console0`
- `io.uart1`
- `io.usb0`
- `io.cdc0`
- `io.tcp0`

`StreamEventSource` 表示 stream readiness 的事件来源。

它至少需要表达：

- readable
- writable
- closed
- error
- reactor / scheduler / EDA 如何被唤醒

当前仓库里的 `io::Channel`、`io.registry` 与 `io.reactor`
已经分别提供了这些方向的胚胎。
但 proposed contract 仍需要把它们收束成面向公共设备契约的准入记录。

## 3. Ownership And Responsibility

### 3.1 Stream Backend

Stream backend 负责：

- 提供非阻塞 read / write / flush
- 把平台错误映射成公共错误语言
- 不保存调用方传入的外部 buffer 指针
- 在暂不可读或暂不可写时返回 `Errc::would_block`
- 在 closed / detached / error 时返回明确状态

Stream backend 不应该泄漏：

- vendor SDK handle
- UART / USB / socket 私有对象
- runtime discovery 的 `DeviceDesc`
- board 私有 channel handle
- protocol 层私有状态机

### 3.2 Stream Endpoint

Stream endpoint 负责：

- 用 capability name 暴露稳定入口
- 记录 readable / writable / duplex / isr_safe 等 endpoint caps
- 保证同名同 cap 不重复注册
- 对动态后端保持稳定指针或稳定槽位
- 在后端 detach 后避免悬挂指针

当前 `ChannelSlotExport` 已经验证了重要方向：

```text
runtime device attach -> stable stream endpoint -> runtime device detach
                         -> old pointer remains safe and returns noent
```

这条经验很接近 runtime Stream IO endpoint contract，
但仍需要进入公共准入语言。

### 3.3 Reactor / Waiting Layer

Reactor / waiting layer 负责：

- 在 ISR 或 backend notify 时只入队事件
- 在 task context 中 drain 并调度 callback
- 用固定 budget 推进协议层
- 通过 scheduler / EDA / runtime pump 处理等待

Stream backend 和 protocol layer 不应该：

- busy-spin
- sleep
- 自建 timeout loop
- 在 `notify()` 中直接执行协议 callback
- 在 protocol callback 中阻塞等待更多数据

## 4. Non-blocking Semantics

Stream IO contract 必须把非阻塞纪律放在第一层。

当前硬规则来自 [`../io/io_channel_contract.md`](../io/io_channel_contract.md)：

- `read(out)` 成功时返回 `Ok(n)`，其中 `0 < n <= out.size()`
- `write(in)` 成功时返回 `Ok(n)`，其中 `0 < n <= in.size()`
- 暂不可读或暂不可写返回 `Errc::would_block`
- closed / EOF 返回明确 closed / end-of-stream 类错误
- `flush()` 不支持时返回 `Errc::not_supported`
- `flush()` 正忙时返回 `Errc::would_block`
- read / write 不允许返回 `Ok(0)`
- Channel 不保存外部 buffer 指针到调用结束之后

当前 `io::Channel` 已经对 read / write 的 `Ok(0)` 做了 hard fault 保护。

但 proposed contract 仍需要记录一个一致性缺口：

- 当前 `ChannelAdapter::flush` 在缺失 flush callback 时返回 `ok(0)`。

本轮不修改实现，只把它列为 contract consistency gap。

## 5. Execution Semantics

当前 Stream IO proposed contract 暂定为非阻塞调用模型。

一次 read / write / flush 调用返回时，backend 应完成下列之一：

- 操作成功推进了至少 1 byte 或完成了 flush
- 当前暂不可推进并返回 `Errc::would_block`
- endpoint 已关闭或失活
- backend 表示能力不支持
- backend 返回明确 I/O 错误

当前不承诺：

- reentrant
- multi-producer / multi-consumer
- callback 内可阻塞
- protocol layer 可自建等待循环
- timeout 由 Stream IO contract 自己托管
- managed time / replay 可控制

ISR 安全必须由 endpoint caps 明确声明。

默认规则是：

- channel 不可重入
- 默认一读一写
- 多生产者 / 多消费者必须由上层序列化
- subscribe / unsubscribe 属于 task context
- notify 只入队，不运行协议 callback

如果未来需要 timeout、managed time 或 replay，必须通过明确 timebase / reactor / scheduler contract 进入。

## 6. Error Semantics

当前 Stream IO 路径主要复用 `util::Errc`。

这比 `bool` 风格更好，但 proposed contract 仍缺一组更窄的 Stream domain error taxonomy。

candidate taxonomy 至少应考虑：

- `would_block`
- `closed`
- `end_of_stream`
- `target_detached`
- `not_supported`
- `framing_error`
- `overrun`
- `flow_control`
- `timeout`
- `policy_violation`
- `io_fault`
- `unknown`

现有经验可以映射为：

- 暂不可用返回 `Errc::would_block`
- detach 后访问返回 `Errc::noent`
- 缺失 flush 返回 `Errc::not_supported`
- registry 缺失返回 `Errc::noent`
- registry 重复返回 `Errc::exist`

在 `experimental` 前，不应为了某个单一 backend 草率冻结 taxonomy。

## 7. Facts

Stream IO proposed contract 未来至少需要能投影下面 facts：

- `stream.endpoint`
- `stream.channel`
- `stream.backend`
- `stream.direction`
- `stream.event_source`
- `stream.reactor`
- `stream.registry`
- `irq.line`
- `clock.domain`
- `dma.channel`
- `power.domain`
- `stream.evidence`

这些 facts 在 v0 不做构建期执法。

它们应先服务：

- admission record
- artifact report
- evidence sample
- explain / unresolved binding 入口
- runtime channel detach 解释
- protocol waiting discipline audit

## 8. Evidence Inventory

当前已有的 Stream IO 证据主要分成四类。

### 8.1 Channel Contract Evidence

已有：

- `io.channel`
- `io_channel_contract.md`
- read / write 禁止 `Ok(0)` 的 hard fault 保护
- caller-owned buffer lifetime 规则
- no busy-spin / no sleep / no internal retry 规则

这证明 stream byte endpoint 已经有强纪律。

### 8.2 Registry And Capability Evidence

已有：

- `io.registry`
- `ChannelBinding`
- `ChannelAliasBinding`
- `RegistryBinding`
- 唯一 cap / name 注册
- `PublishState::missing / published`
- endpoint caps：`readable / writable / duplex / isr_safe`

这证明 stream endpoint 可以进入 capability / init.graph 装配语言。

### 8.3 Reactor Evidence

已有：

- `io.reactor`
- `notify()` enqueue-only
- `drain()` task-context dispatch
- waker hook
- overflow / dropped event 统计
- fixed budget drain 入口

这证明等待与事件推进可以从 protocol layer 抽离。

### 8.4 Runtime Slot Evidence

已有：

- `ChannelSlot`
- `ChannelSlotExport`
- `ExportState::missing / detached / attached`
- attach / detach / unexport transition observer
- `device_runtime_channel_slot_demo`
- `usb_host_runtime_channel_smoke`

这证明 runtime discovered stream 可以通过稳定 IO capability 对外收口。

但这些仍然是系统装配、运行观察和现有 IO 纪律证据。
它们不等价于一张已经完成准入的 driver-facing Stream IO contract。

## 9. Evidence Gaps

当前缺口明确保留：

- 没有专门的 stream contract mock / fault script
- 没有 contract-local facts vocabulary
- 没有 Stream domain error kind
- 没有 adapter 行为一致性审计
- 没有 contract-level timeout / timebase 投影
- 没有 protocol waiting discipline 的自动检查
- 没有把 stream facts 正式投影进 artifact report 的样例
- 没有真实硬件 bringup evidence 与 contract facts 的组合报告

这些缺口补齐前，Stream IO 仍保持 `proposed`。

## 10. Non-goals

当前阶段明确不做：

- 不新增 `Modules/io/device/io.device_stream.cppm`
- 不修改 `io.channel`
- 不修改 `io.reactor`
- 不修改 `io.registry`
- 不修改 `ChannelAdapter::flush`
- 不修改 USB Host CDC runtime glue
- 不修改 Net reactor smoke
- 不宣布 Stream IO contract 为 `experimental`
- 不把 timeout 放入基础 Stream IO contract
- 不把 protocol framing 放入基础 Stream IO contract
- 不承诺 async / managed time / replay

## 11. Next Steps

最值当的下一步是：

1. 保持本文件为 `proposed` card。
2. 先设计 stream mock / fault script 的语义，例如 would_block、closed、detach、short write、flush busy。
3. 把 `ChannelAdapter::flush` 缺失 callback 的行为纳入一致性审计。
4. 建立 contract-local stream facts 草案，只做报告，不做执法。
5. 选择一个准真实 stream middleware evidence，例如 line reader、frame codec、CDC echo adapter。
6. 与 [`device_contract_admission_matrix_v0.md`](device_contract_admission_matrix_v0.md) 同步准入状态。

在这些完成前，Stream IO 仍保持 `proposed`。
