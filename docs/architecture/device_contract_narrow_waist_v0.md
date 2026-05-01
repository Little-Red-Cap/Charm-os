# 设备契约窄腰 v0

## 一句话结论

Charm 的设备契约层不应该成为一个大而全的 HAL。

它应该成为：

> **驱动生态的最小共同语言。**

rust-embedded 对 Charm 最深的启发不是 Rust 语言本身，也不是某个 HAL trait 的形状，而是它把碎片化硬件世界压缩成少数可被信任的公共契约。

Charm 要在这条路上再往前走一步：

> **公共设备契约不仅要让 driver 跨平台，还要进入 system compiler，成为系统成立前就能被检查、裁剪和解释的事实。**

## 1. 窄腰保护谁

设备契约窄腰优先保护：

- driver 作者
- component 作者
- middleware 作者

这些作者写出来的东西应该尽量不依赖：

- STM32Cube / vendor SDK
- 某块 board 的私有 handle
- PC mock 的内部实现
- runtime discovery 的内部 `DeviceDesc`
- `BoardCaps` 的具体布局
- 某个示例里的 glue 代码

他们应该依赖的是更窄的公共语义：

```text
这个对象能否完成一次 I2C transaction
这个 SPI endpoint 是否负责片选和 flush
这个 GPIO 是否是 edge source
这个 block device 是否有稳定 sector 语义
这个 stream 是否遵守非阻塞 read/write 纪律
```

这就是窄腰的价值。

它不消灭硬件差异，也不强行统一底层实现。
它只把上层可复用代码需要依赖的最小事实压稳。

## 2. 它不是另一个总线模型

Charm 已经在 [`driver_model.md`](driver_model.md) 中明确了当前主模型：

```text
静态 capability 装配为主
动态 discovery 生命周期为辅
两者最终用 capability 语言收口
```

设备契约窄腰不是第三套生命周期系统。

它位于更窄的位置：

```text
driver / component / middleware
        ↑
Device Contract Narrow Waist
        ↓
backend / HAL / mock / runtime glue / platform binding
```

也就是说：

- 静态片上控制器仍然走 `BoardCaps + init.graph`
- runtime discovered device 仍然走 `device::Bus / Driver / Registry`
- 对外暴露的稳定资源仍然回到 capability / registry
- 设备契约只定义“可复用驱动面对后端时能相信什么”

## 3. 窄腰的三层关系

### 3.1 Foundation Contract

Foundation Contract 是最底层身份边界。

它回答：

> **这个接口能否站在最小 Charm 环境里。**

默认要求：

- no exception
- no RTTI
- 默认不依赖 heap
- 不泄漏平台 header
- 不隐式依赖 thread / mutex
- 使用 `util::Result<T>` / `util::Errc`
- 时间不自建，进入统一 timebase / clock 语言

这类似 `no_std` 对 rust-embedded 的身份边界，但 Charm 的表达方式是 C++ Modules、constexpr 配置、统一错误模型与资源纪律。

### 3.2 Device Contract

Device Contract 是驱动生态的窄腰。

它回答：

> **一个平台无关 driver 最小需要相信哪些设备语义。**

它包括：

- API shape
- ownership / responsibility
- transaction boundary
- blocking / polling / EDA 语义
- ISR / task context 边界
- error kind
- resource facts
- mock / test path
- capability export path

### 3.3 System Formation Contract

System Formation Contract 是 system compiler 能看见的成立语言。

它回答：

> **这个设备契约如何参与系统成立。**

它包括：

- required facts
- provided facts
- required capability
- exported capability
- binding result
- bringup order
- resource contract
- evidence source
- explain query 入口

Charm 和普通 HAL 的分野就在这里：

```text
普通 HAL:
  driver 能调用接口

Charm:
  driver 能调用接口
  系统也能解释这个接口为什么成立或为什么没成立
```

## 4. 设备契约必须描述的维度

### 4.1 API 形状

API 形状仍然重要，但它只是第一层。

例如一个 I2C 契约可能需要表达：

```cpp
template<class Bus>
concept I2cBus =
    requires(Bus& bus,
             std::uint8_t addr,
             std::span<const std::byte> tx,
             std::span<std::byte> rx) {
        { bus.write(addr, tx) };
        { bus.read(addr, rx) };
        { bus.write_read(addr, tx, rx) };
    };
```

这段只是文档草图，不是当前冻结 API。

真正重要的是下面几层语义。

### 4.2 权责边界

设备契约必须说明：

- 谁拥有 bus
- 谁拥有 endpoint / address / chip select
- 谁负责 lock / unlock
- 谁负责 assert / deassert CS
- 谁负责 repeated start
- 谁负责 flush
- 谁负责让硬件回到 idle
- 谁负责失败后的恢复边界

推荐把这些责任变成明确概念，而不是藏在函数注释里。

候选责任概念包括：

- `ExclusiveBus`
- `SharedBusEndpoint`
- `TransactionalDevice`
- `ClockDomainBound`
- `IrqBoundResource`
- `DmaBackedStream`
- `IsrEdgeSource`
- `TaskContextService`

这些名字现在只是架构词，不表示仓库已经存在对应代码。

### 4.3 执行与时间语义

设备契约必须说明调用发生在哪个执行宇宙里：

- 同步立即完成
- 非阻塞轮询
- 由 reactor 推进
- 由 EDA / scheduler 托管
- 可从 ISR 投递，但不可在 ISR 完整执行
- 只能在 task context 使用
- 需要 monotonic clock
- 受 managed time / replay 影响

这会直接影响：

- driver 能否 busy wait
- timeout 谁负责
- 错误是否返回 `Errc::would_block`
- mock 是否需要时间脚本
- artifact report 是否能解释 timeout / blocked

### 4.4 错误语言

设备契约必须把错误变成可泛化语义。

原则是：

> **平台错误可以具体，公共错误必须可归类，系统错误必须可解释。**

例如 I2C 后端可以保留平台细节，但公共层至少需要能归类：

- bus fault
- arbitration lost
- address NACK
- data NACK
- overrun
- timeout
- policy violation
- target detached
- unknown

这些错误应能继续进入：

- `util::Result<T>`
- `util::Errc`
- trace / evidence
- explain surface

### 4.5 资源与硬件事实

设备契约不能只停在 C++ concept。

它还要能投影到硬件事实图：

- controller exists
- endpoint exists
- address belongs to bus
- pin alternate function is selected
- clock domain is enabled
- IRQ line is routed
- DMA stream is available
- power domain is on
- memory region is DMA-safe

这些事实未来可以来自：

- `BoardCaps`
- board/profile 配置
- SVD / vendor metadata
- handwritten board facts
- probe / runtime discovery
- mock backend script

重要的是它们最终要能进入 system compiler 的事实语言。

## 5. 候选第一批契约

### 5.1 `I2cBus` / `I2cDevice`

优先级较高。

原因：

- 设备驱动生态价值高
- transaction mock 容易做
- host CI 可验证
- 适合验证 error taxonomy
- 适合验证 bus sharing 与 address ownership

建议先做：

- `write`
- `read`
- `write_read`
- transaction script mock
- 一个小 sensor / codec / EEPROM 类 driver

### 5.2 `SpiBus` / `SpiDevice`

优先级较高，但要晚于 I2C 一点。

核心不是 `transfer` 函数，而是 `Bus / Device` 责任拆分：

- `SpiBus` 代表受管 bus 能力
- `SpiDevice` 代表带片选、互斥、flush、transaction 边界的 endpoint

带 CS 的外设 driver 默认应依赖 `SpiDevice`，避免每个 driver 手动处理 CS。

### 5.3 `GpioInput` / `GpioOutput` / `GpioEdgeSource`

适合作为 Foundation 级小契约。

需要特别区分：

- 读当前电平
- 设置输出电平
- 订阅边沿事件
- ISR edge source
- debounce 是否属于上层 service

不要把 GPIO pin 直接做成万能对象。

### 5.4 `BlockDevice`

应与当前 block registry、stable slot、live state 经验对齐。

关键语义：

- sector size
- read / write / flush
- media present
- attached / detached / missing
- write protect
- erase granularity
- failure evidence

### 5.5 `StreamIo`

应与当前 `io::Channel` 非阻塞纪律对齐。

关键语义：

- read / write 非阻塞
- 不返回 `Ok(0)`
- 暂不可用返回 `Errc::would_block`
- timeout 不由协议层 busy-spin
- 等待由 reactor / scheduler / EDA 负责

### 5.6 `Timebase`

应与 `charm.system.clock` 对齐。

关键语义：

- monotonic
- resolution
- wrap behavior
- managed time / replay 是否可控
- ISR 是否可读
- timeout 由谁推进

## 6. 与 SVD / PAC 的关系

Charm 不应该把目标降成 `svd2cpp`。

更合适的链路是：

```text
SVD / vendor metadata
  -> register facts
  -> peripheral facts
  -> capability facts
  -> binding constraints
  -> bringup evidence
```

也就是说，寄存器模型只是底层事实来源之一。

Charm 真正需要的是：

- `GPIOA exists`
- `PA5 exists`
- `PA5 supports output`
- `PA5 supports SPI1_SCK alternate function`
- `PA5 conflicts with another selected function`
- `SPI1 requires clock domain X`
- `DMA stream Y can serve SPI1_TX`

这些事实进入 system compiler 后，才会变成 Charm 版硬件事实编译器。

## 7. 与 mock 的关系

窄腰契约必须天然适合 mock。

一个合格的 device contract 应该能派生出：

- transaction mock
- no-op mock
- loopback mock
- fault injection mock
- timing / jitter mock
- detached target mock

这不是测试附属品，而是契约质量的一部分。

如果一个接口无法被 mock，大概率说明它泄漏了太多平台细节。

## 8. 与 capability / system compiler 的关系

设备契约不应该绕过 capability 体系。

推荐理解为：

```text
Device Contract
  -> required facts
  -> binding constraints
  -> backend selection
  -> capability export
  -> artifact / evidence / explain
```

例如一个 I2C sensor driver 未来可能表达为：

```text
driver requires:
  i2c.device(address = 0x18)
  monotonic_clock if timeout is enabled

board provides:
  i2c1 bus
  i2c1 device 0x18
  clock domain for i2c1
  irq line if async mode is enabled

system compiler reports:
  binding resolved / unresolved
  required facts satisfied / missing
  bringup order
  evidence source
```

这就是 Charm 和普通跨平台 driver HAL 的区别。

## 9. 非目标

当前阶段明确不做：

- 不做巨型通用 HAL
- 不承诺一次性覆盖所有 MCU 外设
- 不把 vendor SDK 类型带入公共层
- 不复制 Rust async / blocking / nb 拆分
- 不让 runtime discovery 反向统治静态 capability 装配
- 不把 SVD 代码生成当成终点
- 不让示例目录反过来主导核心契约
- 不在没有两后端、一 driver、一测试路径前把接口写成 admitted

## 10. v0 推荐落点

v0 最值得推进的是：

1. 先落实 [`interface_admission_policy.md`](interface_admission_policy.md)
2. 选择 I2C transaction mock 作为第一条窄链
3. 写一个小型真实 driver 或准真实 driver 验证契约
4. 把 required facts / error kind / execution semantics 写进文档
5. 让 artifact / evidence 能说明这条链如何成立

这条路线小，但很锋利。

它不会把仓库拖进“大一统驱动框架”的泥潭，却能开始让 Charm 拥有自己的驱动生态窄腰。

## 11. 当前结论

rust-embedded 的窄腰主要服务 driver portability。

Charm 的窄腰还要继续服务 system provability。

也就是说：

```text
前者回答：
  driver 如何跨平台

后者还要回答：
  系统为什么能成立
```

这就是 Charm 设备契约层最值得赌的方向。
