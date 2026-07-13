# Driver / Device 模型

## 文档状态

- `status`: `supporting`
- `scope`: 静态设备装配与动态 discovery 的实现边界
- `authority`: [`CONSTITUTION.md`](../../CONSTITUTION.md)

本文描述现有 driver/device 机制，不把 `Driver`、`Device`、`Bus` 或 `Registry` 定义为 Charm
Core。旧 discovery 讨论见 [`device-model-v0`](../archive/device-model-v0/README.md)。

## 两条路径

| 对象 | 路径 | 约束 |
|---|---|---|
| 板级已知控制器和固定设备 | `board facts -> init.graph -> registry/service` | 资源、依赖和顺序在 bring-up 前已知 |
| 运行期枚举或热插拔设备 | `bus -> DeviceDesc -> Registry/Driver -> stable export` | 实例和生命周期在运行期确定 |

两条路径可以发布到相同消费面，但不是同一套生命周期。固定 UART/SPI/I2C、SDMMC 或 SPI flash
继续使用 `BoardCaps` 和 init binding；不为表面统一增加 `enumerate -> match -> probe`。

## 动态 discovery

`Modules/system/device` 当前提供固定容量、无分配的实验实现：

| module | 职责 |
|---|---|
| `device.desc` / `device.types` | descriptor、device、driver、状态与 lifecycle ops |
| `device.registry` | 注册、匹配、初始化、事件和移除 |
| `device.bus` / `device.manager` | 枚举及 bus/registry 聚合 |
| `device.runtime_driver` | typed context 到 `device::Driver` 的适配 |

### 匹配

`Registry` 的当前分数为 class `+4`、vendor `+3`、product `+2`、type `+1`；同分时比较
`Driver::priority`，仍同分则保留先注册者。全空 descriptor 是 score `0` 的通用 driver。
这些分值是实现细节，不是稳定 ABI。

### 生命周期与错误

```text
detected -> probe -> init -> running
                    +-> suspend -> resume
                    +-> shutdown -> stopped
                    +-> remove -> detected
```

`try_probe/try_init/try_suspend/try_resume` 返回 `util::Result<void>`；兼容 `bool` hook 的失败折叠为
`util::Errc::bad_state`。`shutdown/remove/on_event` 没有返回值。`BusManager::try_enumerate_all()` 与
`Registry::try_*` 保留首个错误，不提供事务回滚。

## 稳定导出

动态 `Device` 不能把裸指针长期发布给共享消费者。当前使用：

- `io::ChannelSlotExport`
- `block::DeviceSlotExport`

registry 管名称发布，slot 管 live target：

| 状态 | 含义 |
|---|---|
| `missing` | 未发布或已 unexport |
| `detached` | 已发布但无 live target |
| `attached` | 已发布且 target 可用 |

`detach()` 后旧 slot 指针保持有效，但 IO 返回 `noent`；`unexport()` 再移除名称。该机制只避免
最小悬挂指针，不提供 lease、引用回收、失效广播、exactly-once teardown 或跨核协议。

## 所有权边界

- platform/board 处理 clock、pinmux、IRQ、寄存器和板级差异。
- HAL 定义控制器接口，不承担领域协议。
- `system/device` 服务运行期发现，不替代静态 bring-up。
- App 消费稳定 endpoint、registry handle 或 service，不消费 `BoardCaps`、`DeviceDesc`、match
  score 或 driver context。
- USB Host 是动态路径样本；USB Device controller 的存在仍是静态平台事实。

当前模型不承诺完整 revoke、热插拔事务、统一电源管理、跨核 manager、driver ABI 或可加载
driver module。

## 证据入口

- bus/registry：`Examples/system/device_bus_demo`、`device_registry_demo`
- detach 语义：`device_runtime_block_slot_demo`、`device_runtime_channel_slot_demo`
- USB Host glue：`Examples/usb/usb_host_runtime_*_smoke`

示例只证明对应 fixture。静态装配见 [`init_graph_contract.md`](../system/init_graph_contract.md)，
IO registry 见 [`io_registry_contract.md`](../io/io_registry_contract.md)，interface 准入见
[`interface_admission_policy.md`](interface_admission_policy.md)。
