# 设备模型草案（Driver/Device Registry）

目标：为 USB/TCPIP/FS/Audio/IO 提供统一的设备生命周期与注册表，避免模块孤岛化。

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
- `probe(dev)`：匹配 + 资源检查（不分配大资源）。
- `init(dev)`：初始化并进入 running。
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
- `suspend(dev)`
- `resume(dev)`

## 6. 设备模型与现有模块的对接方向

### USB
- USB Device/Host 枚举 -> 生成 `DeviceDesc`
- CDC/MSC/UAC -> 作为 Driver

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
