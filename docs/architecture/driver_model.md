# 驱动模型（Capability First / Dual Plane）

本页用于收敛 Charm 当前更合适的驱动模型。

它回答的不是“所有设备都该放进哪一个 Registry”，而是：

- 哪些能力应按静态装配建模
- 哪些设备应按运行期发现建模
- 两条线最终如何收敛到统一 capability 语言

## 1. 一句话结论

Charm 的驱动模型不是“`device::Registry` 一统天下”，而是：

> **静态 capability 装配为主，动态 discovery 生命周期为辅，两者最终用 capability 语言收口。**

也就是说：

- 片上控制器与板级已知资源：走 `BoardCaps + init.graph`
- 上层可用能力：走 `ControllerBinding -> ServiceAdapter -> registry/capability export`
- 运行期枚举设备：走 `device::Bus / device::Driver / device::Registry`
- 但最终对系统与用户暴露时，仍应回到统一 capability / registry 语言

## 2. 为什么不是单模型

Charm 当前已经同时存在两条气质不同的主线。

### 2.1 静态 capability 平面

适用对象：

- UART / SPI / I2C / Timer / GPIO 等片上控制器
- 板级已知的 CAN / SDMMC / SPI Flash / Input 等资源
- 不需要运行期枚举即可确定的控制器与设备

关键特征：

- 资源在板级就已知
- 依赖关系在构建期或 bringup 时已知
- 必须服从 `init.graph` 的唯一性、依赖、phase、拓扑顺序纪律
- 强调确定性装配，而非运行期匹配

典型路径：

```text
BoardCaps
  -> ControllerBinding
  -> ServiceAdapter
  -> io.registry / block.registry / capability export
```

### 2.2 动态 discovery 平面

适用对象：

- USB Host 这类运行期枚举设备
- 后续虚拟总线、热插拔设备、外部枚举出来的逻辑设备

关键特征：

- 设备在运行期 attach / detach
- 需要 `enumerate / match / probe / activate / remove`
- 生命周期与匹配规则比静态装配更重要

典型路径：

```text
RuntimeBus
  -> discovered device
  -> RuntimeDriver
  -> capability export
```

## 3. 核心术语

### 3.1 `BoardCaps`

`BoardCaps` 负责声明板级资源与绑定关系，不负责生命周期推进与系统装配。

它可以包含：

- live handle
- ops 指针
- capability name
- 板级默认配置

它不应该承担：

- 隐式初始化
- probe/match
- 偷偷注册全局状态

### 3.2 `ControllerBinding`

`ControllerBinding` 表示“板级已知控制器能力”的装配节点。

职责：

- 持有 HAL handle / config
- 在 `init.graph` 中导出底层 capability，例如 `hal.uart1`
- 处理控制器级初始化、启用、最小错误映射

说明：

- 这里沿用当前代码已经广泛使用的 `*Binding` 命名体系
- 它对应的是控制器装配节点，不等价于运行期 `device::Driver`

### 3.3 `ServiceAdapter`

`ServiceAdapter` 消费底层控制器 capability，对上导出真正可消费的系统能力。

典型例子：

- `hal.uart1` -> `io.uart1`
- `io.uart1` -> `io.console0` alias
- `hal.sdmmc0` / board block handle -> `block.sd0`
- raw input driver -> `input.service`

职责：

- 语义适配
- 缓冲与状态机
- 与 `io.registry` / `block.registry` / `io.reactor` 对接

它不应该承担：

- 板级枚举
- 平台寄存器细节
- 领域层语义下沉

### 3.4 `RuntimeBus`

`RuntimeBus` 是运行期发现设备的入口。

它负责：

- enumerate
- attach
- detach

它不负责：

- 直接决定上层最终如何消费设备

### 3.5 `RuntimeDriver`

`RuntimeDriver` 是 discovery 平面上的驱动实体。

在代码里，当前通常对应 `device::Driver`。

职责：

- 根据 `DeviceDesc` 进行匹配
- 参与 `probe / init / suspend / resume / remove`
- 把发现到的设备激活成系统可用能力

### 3.6 `Capability Export`

无论能力来自静态平面还是动态平面，只要它要被系统其它部分消费，就必须落到统一 capability 语言。

这意味着：

- 要么成为 `init.graph` 中的 capability
- 要么注册到统一 registry 中，使用稳定 cap name / endpoint name
- 要么以明确的 capability 生命周期进入和退出系统

### 3.7 当前代码映射

这套术语不是凭空发明的，它对应的是仓库里已经存在的代码形状。

| 设计术语 | 当前代码落点 | 说明 |
| --- | --- | --- |
| `BoardCaps` | `platform::board::BoardCaps`、各板级 `make_board_caps()` | 板级资源声明与绑定关系 |
| `ControllerBinding` | `hal::UartBinding`、`hal::SpiBinding`、`hal::I2cBinding` | 导出 `hal.*` capability 的控制器装配节点 |
| `ServiceAdapter` | `driver::usart::ChannelBinding`、`io::ChannelAliasBinding` | 把底层控制器能力导出为 `io.*` 等上层可消费能力 |
| 折叠式 `ServiceAdapter` | `block::SdmmcBinding`、`block::SpiFlashBinding` | 当前把控制器初始化与 block capability 导出折叠在同一个 binding 中 |
| `RuntimeBus` | `device::Bus`、`usb::host::HostBus` | 运行期发现入口 |
| `RuntimeDriver` | `device::Driver`、`device::make_runtime_driver<ContextT>(...)`、`usb::device::make_device_driver(...)` | 运行期匹配与生命周期驱动 |
| 静态装配组合器 | `CoreSystemChain`、`UsartInitChain`、`BringupMinimal` | 负责把静态 capability 平面组合成主装配路径 |

说明：

- `driver::usart::ChannelBinding` 虽然名字带 `driver`，但从职责上更接近 `ServiceAdapter`
- 存储路径当前有“折叠式 binding”现象，这是一种可接受的阶段性实现，不改变它属于静态 capability 平面的事实

## 4. 静态平面的标准路径

静态平面是 Charm 驱动模型的主轴。

对于 MCU 片上控制器，不推荐引入 `enumerate -> match -> probe` 这类总线式复杂度。

原因：

- 资源已知
- 顺序已知
- 依赖已知
- `init.graph` 已经提供了更强的唯一性与拓扑约束

推荐路径：

```text
BoardCaps
  -> ControllerBinding（导出 hal.*）
  -> ServiceAdapter（导出 io.* / block.* / input.*）
  -> registry / reactor / consumer
```

### 4.1 UART 样板

推荐理解为：

```text
BoardCaps.uart1
  -> hal.uart1
  -> io.uart1
  -> io.console0
```

其中：

- `hal.uart1` 是控制器能力
- `io.uart1` 是运行期可读写 channel
- `io.console0` 是用户侧更友好的别名或默认入口

对应当前代码的大致映射是：

```text
platform::board::BoardCaps::uart1
  -> hal::UartBinding
  -> driver::usart::ChannelBinding
  -> io::ChannelAliasBinding
```

从用户视角看，最终应该优先消费：

- `io.uart1`
- `io.console0`

而不是直接感知 HAL handle、板级 ops 或内部适配器细节。

### 4.2 存储路径样板

当前 block 路径已经体现了“静态平面优先”的思路，只是部分实现是折叠形态。

推荐理解为：

```text
BoardCaps.sdmmc0 / BoardCaps.flash0
  -> block::SdmmcBinding / block::SpiFlashBinding
  -> block.registry
  -> block.sd0 / block.flash0
```

这里的现状说明两件事：

- 静态 capability 平面不要求每条链路都必须机械地拆成两个文件或两种类型
- 只要边界仍清晰、能力仍通过 registry/capability 暴露，就允许阶段性折叠实现

### 4.3 这条线的硬规则

- 必须走 `init.graph`
- capability 必须唯一提供者
- 初始化必须可拓扑排序
- init 内禁止阻塞
- 不允许用 discovery 生命周期替代静态装配纪律

## 5. 动态平面的标准路径

动态平面只用于运行期发现出来的设备。

推荐路径：

```text
RuntimeBus
  -> DeviceDesc
  -> RuntimeDriver match/probe/init
  -> capability export
  -> suspend/resume/remove
```

### 5.1 USB Host

USB Host 明确属于动态 discovery 平面。

原因：

- 设备在运行期插拔
- class / vendor / product 只能在枚举后获得
- attach / detach 是模型本体的一部分

### 5.2 USB Device

USB Device 不能简单地整体归入动态平面。

更准确的说法是：

- USB Device controller / DCD 装配，仍属于静态 capability 平面
- USB class lifecycle 可以借用 `device::Driver` 风格语义
- 但这不意味着 USB Device controller 本身要退化成运行期发现模型

### 5.3 动态平面的 capability export 规则

动态平面中的对象，不能停留在 `DeviceDesc / match_score / probe` 语言里。

如果它要被系统其它部分消费，必须进一步导出为稳定的系统能力。

最低规则：

1. discovered device 不应作为对外最终抽象
2. export 时必须生成稳定 name / cap id
3. export 目标必须明确是 `io.registry`、`block.registry`，或其他明确的 capability 容器
4. attach 与 detach 都必须有对应的进入/退出策略
5. 调用方不应直接依赖 discovery 内部字段

### 5.4 当前边界：有最小 unregister，但还没有完整 revoke 语义

当前 `io.registry` 与 `block.registry` 已具备：

- `register_*`
- `replace_*`
- `open_*`
- `unregister_*`

但还没有：

- `remove_*`
- 统一的 revoke 语义
- 对已发放指针的失效广播

这意味着一个重要边界：

> **动态 discovery 平面当前仍不适合把“短生命周期、可热拔插”的子设备直接裸导出到共享 registry。**

否则会出现：

- registry 中保留失效指针
- 旧 capability 名称仍可被打开
- detach 后缺少统一失效语义
- 已经发放出去的原始指针缺少统一收回机制

### 5.5 在 revoke 语义落地前的推荐策略

在 `unregister/revoke` 能力补齐之前，动态设备推荐使用下面两种策略之一：

1. 只导出“长期存在的 manager/service capability”

例如：

- `usb.host`
- `usb.host.storage`
- `usb.host.net`

由这个长期存在的 service 内部持有 discovered device 句柄，并负责转发访问。

2. 使用“稳定槽位 + 内部存活位”模型

也就是：

- capability 名称固定
- registry 中的对象本身长期存在
- attach / detach 只改变内部 live state

当前仓库里可以优先复用这类基础件：

- `io.channel.slot`
- `io.channel.slot_export`
- `block.device.slot`
- `block.device.slot_export`
- `usb.host.runtime`
- `usb.host.runtime_block`
- `usb.host.runtime_channel`
- `usb::host::DeviceListRuntimeBus`
- `usb.host.runtime_manager`

仓库内已经有两个最小样板可直接参考：

- `Examples/system/device_runtime_block_slot_demo`
  它演示了 runtime discovery 通过 `block::DeviceSlotExport` 向 `block.registry`
  暴露稳定 capability，并在 detach 后让旧指针返回 `Errc::noent`
- `Examples/system/device_runtime_channel_slot_demo`
  它演示了 runtime discovery 通过 `io::ChannelSlotExport` 向 `io.registry`
  暴露稳定 capability，并在 detach 后让旧指针返回 `Errc::noent`
- `Examples/usb/usb_host_runtime_block_smoke`
  它演示了 USB Host discovery 如何通过 `usb::host::MscBlockRuntimeBinding`
  把 `SingleDeviceRuntimeBus` 收敛进 `usb::host::RuntimeManager`，走完
  `HostBus -> RuntimeDriver -> block::DeviceSlotExport -> block.registry`
  这条正式 glue 路径
- `Examples/usb/usb_host_runtime_channel_smoke`
  它演示了 USB Host discovery 如何通过 `usb::host::CdcChannelRuntimeBinding`
  把 `SingleDeviceRuntimeBus` 收敛进 `usb::host::RuntimeManager`，走完
  `HostBus -> RuntimeDriver -> io::ChannelSlotExport -> io.registry`
  这条正式 glue 路径
- `Examples/usb/usb_host_runtime_multi_smoke`
  它演示了 `usb::host::RuntimeManager` 如何把同一条 USB Host runtime bus
  上的多个 discovered device 编排到统一 runtime registry，
  并分别导出到 `block.registry` 与 `io.registry`

这样可以避免把短命对象指针直接暴露到全局 registry。

在当前阶段，不推荐：

- 把每个热插拔子设备都直接注册成新的短命 endpoint
- 在没有 revoke 语义的情况下，把 raw device 指针长期暴露给外部

## 6. 双平面如何收口

双平面不能变成“两套货币体系”。

必须明确：

- 静态 capability：走 `init.graph`
- 动态 discovered device：走 runtime lifecycle
- 但两者最终对外暴露时，必须回到统一 capability / registry 语言

### 6.1 统一语言的最低要求

- 稳定的 capability name / endpoint name
- 明确的注册与回收路径
- 调用方优先通过 capability / registry 获取资源
- 用户代码不依赖 `match_score`、`DeviceDesc` 等 discovery 内部细节

### 6.2 对运行期发现设备的额外要求

运行期发现出来的设备，最终仍应回答下面几个问题：

1. 它对外提供的 capability 名称是什么？
2. 它注册到哪个 registry？
3. attach / detach 时如何进入和退出系统可见状态？
4. 释放后调用方如何感知失效？

这些问题不回答清楚，双平面就会退化成两个互不相通的子系统。

## 7. 各层职责边界

### 7.1 `platform/*`

- 提供板级差异、IRQ、clock、pinmux、寄存器与底层绑定
- 允许接触真实平台细节
- 不直接承担领域语义

### 7.2 `io/hal/*`

- 定义稳定硬件接口与最小语义
- 不负责领域层协议与业务语义
- 是 `ControllerBinding` 的直接基础

### 7.3 `io/driver/*`

当前更适合作为：

- 控制器后端实现
- 控制器到通用运行期语义之间的适配层

这里不建议把所有 `io/driver/*` 都理解成 runtime discovery plane 的 `device::Driver`。

### 7.4 `io/service/*` 或等价适配层

- 对 HAL 或 controller capability 做运行期语义封装
- 导出对用户更友好的能力
- 是 `ServiceAdapter` 的主要落点

### 7.5 `system/device/*`

- 仅承担 runtime discovery plane 的 device / bus / driver 模型
- 不应反向定义整个系统的静态装配路径

## 8. 错误模型与契约要求

当前 `device::*` 仍以 `bool` 风格为主。

这意味着：

- 它暂时更像运行期子系统草案
- 还不适合作为全局唯一驱动模型

后续如需增强，应优先收敛到：

- `util::Result<T>`
- `util::Errc`
- 明确的 capability export / revoke 结果语义

但这项收敛不影响静态平面继续以 `init.graph` 为主轴。

## 9. 非目标

以下都不是本模型的目标：

- 把所有片上外设都改造成 `probe/match` 风格
- 用 `device::Registry` 替代 `init.graph`
- 让用户直接面向复杂 driver lifecycle 编程
- 为了统一而牺牲 MCU bringup 的确定性

## 10. 推荐演进路径

### 第一阶段：先冻结语言，不急着重写代码

- 明确静态 capability 平面 / 动态 discovery 平面
- 明确 `BoardCaps / ControllerBinding / ServiceAdapter / RuntimeBus / RuntimeDriver`
- 把现有代码映射到这套命名

### 第二阶段：把 `device::*` 收敛为动态平面

- 文档上明确其作用域
- 后续逐步把 `bool` 返回改进为统一错误模型
- 为 attach / detach 后的 capability export 建立明确规则

### 第三阶段：选两个样板链路

- 静态样板：UART `BoardCaps -> hal.uart1 -> io.uart1 -> io.console0`
- 动态样板：USB Host `RuntimeBus -> discovered device -> RuntimeDriver -> exported capability`

### 第四阶段：再讨论统一模板与代码收敛

- 再决定 `io/driver` 与 `io/service` 的目录命名是否需要进一步收敛
- 再决定是否为 discovery 平面增加 plan / observe / trace 支撑

## 11. 当前结论

Charm 当前最合理的设备模型，是：

> **静态 capability 装配为主，动态 discovery 生命周期为辅，两者最终用 capability 语言收口。**

这套模型既保留了 MCU 系统装配的确定性，也给运行期枚举设备留出了合适的位置。

它的重点不是让框架内部看起来简单，而是让用户侧调用路径保持简单、稳定、低心智负担。
