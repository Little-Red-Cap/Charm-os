# 设备模型草案（Driver/Device Registry）

目标：为 USB/TCPIP/FS/Audio/IO 提供统一的设备生命周期与注册表，避免模块孤岛化。

> 注：本页现在只描述 **动态 discovery 平面** 的设备发现模型草案。  
> 如果你要看 Charm 当前更完整的驱动模型收敛结论，请先读 `docs/architecture/driver_model.md`。
>
> 当前还要注意一个边界：`io.registry` / `block.registry` 现在只有最小 `unregister` 能力，
> 但还没有统一的 `revoke` 与已发放指针失效语义，
> 因此本页模型更适合描述“发现、匹配、激活”过程，而不宜被误读为
> “已经具备完整热插拔 capability 导出/回收闭环”。
>
> 另外，`device::Registry` / `device::System` 现在已经开始补
> `try_add_device` / `try_add_driver` / `try_add_bus` 这类
> `util::Result<void>` 风格入口；旧 `add_*` 仍保留为兼容包装。
>
> 目前代码里的收敛进度可以简要理解为：
> - `device::Registry` / `device::System`：已有 `try_dispatch` / `try_match_detected` /
>   `try_suspend_all` / `try_resume_all` / `try_enumerate_all`
> - `device::Bus` / `usb::host::HostBus`：已有可选 `try_enumerate` callback bridge，
>   旧 `enumerate` 仍保留
> - `device::DriverOps` / `device::RuntimeDriverHook`：已有可选
>   `try_probe / try_init / try_suspend / try_resume`，
>   旧 `bool` hook 仍保留为兼容入口
> - `device::make_runtime_driver(...)` 与 `usb::device::make_device_driver(...)`
>   这两条主要驱动适配入口，已经开始默认接入上述 `try_*` 语义
>
> 因此，这一页更适合被理解为“动态 discovery 平面的当前实现快照 + 后续演进方向”，
> 而不是一份完全独立于现有代码状态的理想化白纸设计。
>
> 如果当前 discovered device 需要导出为稳定 capability，
> 仓库里可以优先参考：
> `io.channel.slot_export`、`block.device.slot_export`，
> `device::make_runtime_driver<ContextT>(...)`、
> `usb::host::SingleDeviceRuntimeBus`、
> `usb::host::DeviceListRuntimeBus`、
> `usb::host::RuntimeManager`、
> `usb::host::MscBlockRuntimeBinding`，
> `usb::host::CdcChannelRuntimeBinding`，
> `Examples/system/device_runtime_block_slot_demo`、
> `Examples/system/device_runtime_channel_slot_demo`、
> `Examples/usb/usb_host_runtime_block_smoke`、
> `Examples/usb/usb_host_runtime_channel_smoke`、
> `Examples/usb/usb_host_runtime_multi_smoke`
> 这条“稳定槽位 + 内部存活位”路线。
>
> 这条路线当前还补上了一层最小退出能力：
> `io::ChannelSlotExport::unexport()` /
> `block::DeviceSlotExport::unexport()`，
> 它们会先 detach 稳定槽位，再把 capability 从 registry 中撤下；
> 但这仍不等价于完整 revoke 语义。
>
> 对 USB Host runtime glue，目前还多了一层 manager 级组合动作：
> `usb::host::RuntimeManager::try_remove(binding)`、
> `try_unexport(binding)`、
> `try_forget(binding)`。
> 可以把它们分别理解为：
> “移除 runtime device”、
> “撤下稳定 capability”、
> “连同 bus record 一并忘掉”。
>
> 此外，当前还补上了一层最小状态语言：
> `io::ExportState::{missing, detached, attached}` /
> `block::ExportState::{missing, detached, attached}`。
> 它们用于表达“稳定 capability 是否还在 registry 中，以及当前是否 live”，
> 补足 `open_*` / `find_*` 只能表达“是否已发布”的缺口。
>
> 与之对应，registry 自身现在也开始显式暴露
> `io::PublishState::{missing, published}` /
> `block::PublishState::{missing, published}`，
> 用来表达 capability 是否仍处于 published 视图中。
> 这层 published 视图也可以继续由 runtime binding / manager 向上转发，
> 让上层调用不必总是手动回到 registry 查询。
> 如果要继续收敛用户侧查询接口，更推荐 manager 返回组合状态快照，
> 而不是把 published / live / tracked 粗暴揉成一个枚举。

## 1. 核心概念

### Device
- 表示“硬件或逻辑设备实例”。
- 由 bus/driver 发现或创建。

### Driver
- 提供匹配规则与生命周期回调。
- 不直接持有全局资源。

### Bus（可选）
- 描述一类设备的枚举/挂载/卸载方式（USB/PCI/I2C/虚拟总线）。
- 负责发现设备并注册到 Registry。

## 2. 生命周期（最小闭环）

```
register_driver -> probe -> init -> running -> shutdown -> remove
```

推荐回调：
- 新代码优先提供 `try_probe(dev)` / `try_init(dev)`：
  直接返回 `util::Result<void>`，便于保留更精确错误。
- 兼容代码仍可提供 `probe(dev)` / `init(dev)`：
  返回 `bool`，失败时由 Registry 统一折叠为 `util::Errc::bad_state`。
- `shutdown(dev)`：停设备（可重复调用）。
- `remove(dev)`：释放资源并解绑。

## 3. 事件表（Registry 统一派发）

| 事件 | 触发点 | 期望效果 | 失败处理 |
| --- | --- | --- | --- |
| attach | add_device | 进入 detected | 仅记录 |
| probe | match 后 | 允许 init | 失败则换驱动 |
| init | probe 通过 | 进入 initialized | 失败 -> remove |
| start | init 成功 | 进入 running | 标记 error |
| suspend | 电源/策略 | 进入 suspended | 保持 running |
| resume | 电源/策略 | 回到 running | 保持 suspended |
| shutdown | 系统退出 | 进入 stopped | 继续执行 |
| remove | 解绑设备 | 回到 detected | 清 driver |
| error | 运行失败 | 进入 stopped | 交由上层 |

## 4. 匹配策略（统一规则）

匹配因子：
- `class_id / vendor_id / product_id / type`

评分规则（由 Registry 计算）：
- class 匹配 +4
- vendor 匹配 +3
- product 匹配 +2
- type 匹配 +1
- 全部为空视为“通用驱动”，score=0

冲突处理：
1. 先按 `match_score` 选择最高者。
2. 分数相同则按 `driver.priority` 选择。
3. 仍相同：取先注册的驱动（稳定性）。

## 5. 状态与电源管理（最小）

### DeviceState
- `detected`
- `initialized`
- `running`
- `suspended`
- `stopped`

### Power Hooks
- 新代码优先提供 `try_suspend(dev)` / `try_resume(dev)`
- 兼容代码仍可提供 `suspend(dev)` / `resume(dev)`

## 6. 设备模型与现有模块的对接方向

### USB
- USB Device/Host 枚举 -> 生成 `DeviceDesc`
- CDC/MSC/UAC -> 作为 Driver

当前仓库里，USB Host 侧已经有一条最小正式 glue 可以参考：

- `usb.host.core`：把 host 入口包装成 `device::Bus`
- `usb.host.runtime`：提供单 discovered device 的最小 runtime bus 骨架
- `usb::host::DeviceListRuntimeBus`：提供一条 host bus 枚举多个 discovered device 的骨架
- `usb.host.runtime_manager`：把 host runtime bus、runtime registry 和增量扫描编排收敛为一处
- `usb.host.runtime_block`：把 host MSC 设备导出到 `block::DeviceSlotExport`
- `usb.host.runtime_channel`：把 host CDC 设备导出到 `io::ChannelSlotExport`
- `Examples/usb/usb_host_runtime_block_smoke`：最小跑通样板
- `Examples/usb/usb_host_runtime_channel_smoke`：最小跑通样板
- `Examples/usb/usb_host_runtime_multi_smoke`：单 bus 多设备样板

### FS
- BlockDevice -> Device
- FatFs/RamFs -> Driver

### Audio
- I2S/SDL3 -> Device
- Sink -> Driver

### Kernel/ModuleX
- Driver 作为“可装载模块”注册到 Registry

## 7. USB 注册示例（最小可运行骨架）

```cpp
import device.manager;
import device.registry;
import device.types;
import device.desc;
import usb.device;
import usb.device_driver;
import usb.driver;

using DeviceSystem = device::System<8, 8, 2>;

static usb::device::Device usb_dev{};
static usb::device::DeviceDriver usb_dev_driver(usb_dev, /*dcd_ctx*/ nullptr, /*dcd_ops*/ {});

static usb::device::DeviceModelHook usb_hook{
    .dev = &usb_dev,
    .driver = &usb_dev_driver,
    .start = nullptr,
    .stop = nullptr,
};

static device::Driver usb_driver = usb::device::make_device_driver(
    &usb_hook,
    device::DeviceDesc{
        .class_id = 0x00,
        .vendor_id = 0,
        .product_id = 0,
        .type = "usb.device",
    },
    "usb.device",
    10);

void register_usb(DeviceSystem& sys) {
    sys.add_driver(usb_driver);
    sys.add_device(device::DeviceDesc{.type = "usb.device"}, &usb_hook);
    sys.init_all();
}
```

> 注：DCD/EP0 实际回调由平台层补齐，上层只依赖统一的 Driver/Device 生命周期。

## 8. 约束（硬规则）
- Driver 不得直接依赖具体平台实现（必须走 Device/Bus 接口）。
- Registry 不得阻塞。
- 匹配失败不影响其它设备。
- 新增 `try_*` 入口时，优先使用 `util::Result<T>` + `util::Errc`，
  不要再引入第三套错误返回约定。

## 9. VSF 对照表（接口形状参考）

仅参考分层与接口形状，不参考宏体系与实现细节。

| Charm 设备模型 | VSF 对应概念 | 说明 |
| --- | --- | --- |
| Device | vsf_device_t / vsf_dev_t | 设备实例与描述 |
| Driver | vsf_driver_t | 设备驱动抽象 |
| Bus | vsf_hal/usb host stack | 发现/枚举/挂载 |
| Registry | vsf_device_registry | 设备注册与查找 |
| probe/init/remove | vsf_xxx_init / vsf_xxx_fini | 生命周期钩子 |
| suspend/resume | vsf_pm 相关 | 电源管理入口 |

## 10. 迁移清单（从易到难）

### 第一阶段（高收益/低风险）
1. USB Device：CDC/MSC/UAC 作为 Driver，USB 枚举作为 Bus
2. FS：BlockDevice -> Device，FatFs/RamFs -> Driver
3. Audio：I2S/SDL3 -> Device，Sink -> Driver

### 第二阶段（中风险）
1. USB Host：Host 枚举 + Hub 作为 Bus
2. TCP/IP 抽象层：socket/endpoint 作为 Device

### 第三阶段（高风险）
1. Power/PM：suspend/resume 联动 Kernel/Driver
2. 多总线协同：热插拔 + 统一事件流
