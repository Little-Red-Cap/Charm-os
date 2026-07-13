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

`Modules/system/device` 当前提供固定容量、无分配的实验实现：bus 枚举 descriptor，registry
负责匹配和 lifecycle，runtime-driver adapter 将 typed context 接到 driver hook。具体 module
拆分以源码为准。

### 匹配

Registry 按 descriptor 匹配具体程度选择 driver，同等匹配时比较 priority，仍相同时保留先注册者；
全空 descriptor 可作为通用 driver。具体分值是实现细节，不是稳定 ABI。

### 生命周期与错误

匹配成功后依次执行 probe、init 和 start。Probe 失败会解绑 driver；init 失败会执行 remove 并回到
detected。Suspend/resume、shutdown 和 remove 只按当前 hook 与状态推进，不提供事务回滚。

`try_*` hook 返回 `util::Result<void>`；兼容 `bool` hook 的失败折叠为 `util::Errc::bad_state`。
`shutdown/remove/on_event` 没有返回值。Bus/registry 聚合操作保留首个错误，但可能已有其它设备完成
推进。

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

Bus/registry 与 detach fixture 位于 `Examples/system/device_*_demo`，USB Host glue 位于
`Examples/usb/usb_host_runtime_*_smoke`。示例只证明对应 fixture。静态装配见
[`init_graph_contract.md`](../system/init_graph_contract.md)，
IO registry 见 [`io_registry_contract.md`](../io/io_registry_contract.md)，interface 准入见
[`interface_admission_policy.md`](interface_admission_policy.md)。
