# Driver / Device 模型

## 文档角色

本文描述仓库当前的 driver/device supporting 模型。它约束局部实现与装配，不把 `Driver`、`Device`、`Bus` 或 `Registry` 自动提升为 Charm Core 概念；是否进入 Core 仍由 [`../../CONSTITUTION.md`](../../CONSTITUTION.md) 和 [`charm_core_contract.md`](charm_core_contract.md) 裁决。

旧 discovery 草案及未实施设想见 [`../archive/device-model-v0/README.md`](../archive/device-model-v0/README.md)。

## 结论

仓库当前有两条不同路径：

| 对象 | 路径 | 依据 |
|---|---|---|
| 板级已知控制器和固定设备 | 静态装配 | 资源、依赖和初始化顺序在 bring-up 前已知 |
| 运行期枚举或热插拔设备 | 动态 discovery | 实例和生命周期只能在运行期确定 |

两条路径可以把结果发布到 `io.registry`、`block.registry` 等稳定消费面，但不能因此被描述成同一套生命周期模型。

```text
static:
BoardCaps -> init.graph binding -> io/block registry -> consumer

dynamic:
Bus -> DeviceDesc -> Registry/Driver lifecycle -> stable slot export -> consumer
```

## 静态装配

片上 UART/SPI/I2C 和板级固定存储不需要 `enumerate -> match -> probe`。当前源码中的主要落点包括：

- `platform::board::BoardCaps`：声明 UART、SPI、I2C、SDMMC、SPI flash 等板级描述；
- `hal::UartBinding`、`hal::SpiBinding`、`hal::I2cBinding`：`init.graph` 控制器节点；
- `driver::usart::ChannelBinding`、`io::ChannelAliasBinding`：将底层 UART 适配成 channel 和 console alias；
- `block::SdmmcBinding`、`block::SpiFlashBinding`：初始化并发布固定 block device；
- `CoreSystemChain`、`UsartInitChain`、`BringupMinimal`：组合静态装配路径。

这里的 `Binding` 是装配节点，不等同于动态平面的 `device::Driver`。固定资源应继续服从 `init.graph` 的 provider 唯一性、依赖和拓扑顺序，不应为了表面统一而引入运行期匹配。

## 动态 Discovery

动态路径由 `Modules/system/device` 提供，目前是固定容量、无分配的实验性子系统：

| 模块 | 已实现职责 |
|---|---|
| `device.desc` | `class_id/vendor_id/product_id/type` 描述 |
| `device.types` | `Device`、`Driver`、状态和 lifecycle ops |
| `device.registry` | 注册、匹配、初始化、事件派发和移除 |
| `device.bus` | `enumerate/try_enumerate` 与 attach/detach hook |
| `device.manager` | 聚合 bus 与 registry |
| `device.runtime_driver` | 将 typed context 适配成 `device::Driver` |

### 匹配

`Registry` 当前按描述字段计算匹配分数：class `+4`、vendor `+3`、product `+2`、type `+1`。分数相同则取更高 `Driver::priority`；仍相同则保留先注册者。全空描述是 score `0` 的通用 driver。

这是当前实现细节，不是跨后端稳定 ABI。调用方不应依赖具体分值。

### 生命周期

当前成功路径是：

```text
detected -> probe -> init -> running
                    |
                    +-> suspend -> resume
                    +-> shutdown -> stopped
                    +-> remove -> detected (driver cleared)
```

`try_probe/try_init/try_suspend/try_resume` 优先返回 `util::Result<void>`；旧 `bool` hook 仍作为兼容入口，失败折叠为 `util::Errc::bad_state`。`shutdown/remove/on_event` 仍是无返回值 hook，因此错误模型尚未完全统一。

`BusManager::try_enumerate_all()` 和 `Registry::try_*` 保留遇到的首个错误，但不会提供事务回滚。

## 动态能力导出

发现到 `Device` 不等于已形成可长期消费的 capability。热插拔对象不应把裸指针直接长期发布到共享 registry。

当前可验证路径使用稳定槽位：

- `io::ChannelSlotExport`
- `block::DeviceSlotExport`

registry 管发布状态，slot 管 live target：

| 状态 | 含义 |
|---|---|
| `missing` | capability 未发布或已 unexport |
| `detached` | capability 已发布，但没有 live target |
| `attached` | capability 已发布，且存在 live target |

`detach()` 后，已经持有的稳定 slot 指针仍然有效，但读写返回 `noent`；`unexport()` 再从 registry 移除名称。transition observer 可观察 `ensure_exported/attach/detach/unexport`。

这只解决最小悬挂指针问题，不等于完整 revoke：

- 没有 lease 或引用回收；
- 没有向任意消费者广播失效；
- 没有 exactly-once teardown；
- 没有跨线程或跨核生命周期协议。

在这些能力出现前，动态设备应使用稳定槽位，或只导出长期存在的 manager/service。

## 边界

- `platform/*` 处理 clock、pinmux、IRQ、寄存器和板级差异。
- `io/hal/*` 定义控制器级接口，不承担领域协议。
- `io/driver/*` 当前既有控制器适配，也有领域 driver；目录名本身不决定其属于动态 discovery。
- `system/device/*` 只服务运行期发现，不定义静态 bring-up。
- 普通 App/业务代码消费稳定 endpoint、registry handle 或 service，不消费 `BoardCaps`、`DeviceDesc`、match score 和 driver context。

USB Host 是动态路径的主要样本。USB Device controller 的存在性仍是静态平台事实；class 生命周期可以复用部分 driver 机制，但不能把整个 controller 归入 discovery。

## 证据

当前直接证据包括：

- `Examples/system/device_bus_demo`
- `Examples/system/device_registry_demo`
- `Examples/system/device_runtime_block_slot_demo`
- `Examples/system/device_runtime_channel_slot_demo`
- `Examples/usb/usb_host_runtime_block_smoke`
- `Examples/usb/usb_host_runtime_channel_smoke`
- `Examples/usb/usb_host_runtime_multi_smoke`

前两个证明 bus/registry 基础语义；slot demos 证明 detach 后旧入口返回 `noent`；USB smokes 证明当前 host runtime glue。示例存在不代表所有平台、并发和错误路径均已覆盖。

## 非目标与未决项

当前模型不承诺：

- 用 `device::Registry` 替代 `init.graph`；
- 把 FS、Audio、网络、ModuleX 统一改造成 `Device/Driver`；
- 完整 revoke、lease、backpressure 或热插拔事务；
- 统一电源管理、跨总线协调或跨核 device manager；
- driver 二进制 ABI 或可加载 driver module。

这些方向可以继续讨论，但在源码、错误语义和独立证据出现前只能是 exploration。

## 相关契约

- [`interface_admission_policy.md`](interface_admission_policy.md)：可复用 implementation interface 审查。
- [`../system/init_graph_contract.md`](../system/init_graph_contract.md)：静态装配。
- [`../io/io_registry_contract.md`](../io/io_registry_contract.md)：IO registry。
- [`../storage/block_device_contract.md`](../storage/block_device_contract.md)：block device 消费面。
