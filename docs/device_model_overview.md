# 设备模型草案（Driver/Device Registry）

目标：为 USB/TCPIP/FS/Audio/IO 提供统一的 **设备生命周期与注册表**，避免模块孤岛化。

## 1. 核心概念

### Device
- 表示“硬件或逻辑设备实例”
- 由 bus/driver 发现或创建

### Driver
- 提供匹配规则与生命周期回调
- 不直接持有全局资源

### Bus（可选）
- 描述一类设备枚举与管理方式（如 USB/PCI/I2C/虚拟总线）
- 负责发现、挂载、卸载

## 2. 生命周期（最小闭环）

```
register_driver -> probe -> init -> running -> shutdown -> remove
```

推荐回调：
- `probe(dev)`：匹配 + 资源检查（不分配大资源）
- `init(dev)`：初始化并进入 running
- `shutdown(dev)`：停设备（可重复调用）
- `remove(dev)`：释放资源并解绑

## 3. 状态与电源管理（最小）

### DeviceState
- `detected`
- `initialized`
- `running`
- `suspended`
- `stopped`

### Power Hooks（可选）
- `suspend(dev)`
- `resume(dev)`

## 4. 匹配规则

支持两类（先做最简单）：
1) **ID 匹配**：`vendor_id / product_id / class_id`
2) **字符串匹配**：`type == "usb.cdc" / "fs.fatfs"`

匹配数据挂在 `DeviceDesc` 上：

```
DeviceDesc {
  class_id
  vendor_id
  product_id
  type_name
}
```

## 5. 注册表与索引

最小注册表：
- `DeviceRegistry`：设备列表
- `DriverRegistry`：驱动列表

匹配流程（伪码）：
```
for dev in devices:
  for drv in drivers:
    if drv.match(dev.desc):
      drv.probe(dev)
      drv.init(dev)
```

## 6. 与现有模块的对接方向

### USB
- USB Device/Host 枚举 -> 生成 DeviceDesc
- CDC/MSC/UAC -> 作为 Driver

### FS
- BlockDevice -> Device
- FatFs/RamFs -> Driver

### Audio
- I2S/SDL3 -> Device
- Sink -> Driver

### Kernel/ModuleX
- Driver 作为“可装载模块”注册到 Registry

## 7. 最小实现建议（后续落地）

先做 **header-only 草案**（模块化）：
- `device.desc`（ID/类型）
- `device.driver`（回调接口）
- `device.registry`（固定容量数组）

不引入动态分配，容量固定。

## 8. 约束（硬规则）

- Driver 不得直接依赖具体平台实现（必须走 Device/Bus 接口）
- Registry 不得阻塞
- 匹配失败不得影响其它设备

---

## 9. VSF 对照表（接口形状参考）

仅参考 **分层与接口形状**，不参考宏体系与实现细节。

| Charm 设备模型 | VSF 对应概念 | 说明 |
| --- | --- | --- |
| Device | vsf_device_t / vsf_dev_t | 设备实例与描述 |
| Driver | vsf_driver_t | 设备驱动抽象 |
| Bus | vsf_hal/usb host stack | 发现/枚举与挂载 |
| Registry | vsf_device_registry | 设备注册与查找 |
| probe/init/remove | vsf_xxx_init / vsf_xxx_fini | 生命周期钩子 |
| suspend/resume | vsf_pm 相关 | 电源管理入口 |

注：VSF 大量依赖宏与配置体系，Charm 只保留抽象层次与生命周期语义。

## 10. 迁移清单（从易到难）

### 第一阶段（高收益/低风险）
1. **USB Device**：CDC/MSC/UAC 作为 Driver，USB 枚举作为 Bus  
2. **FS**：BlockDevice -> Device，FatFs/RamFs -> Driver  
3. **Audio**：I2S/SDL3 作为 Device，Sink 作为 Driver

### 第二阶段（中风险）
1. **USB Host**：Host 枚举与 Hub 作为 Bus  
2. **TCP/IP 抽象层**：socket/endpoint 作为 Device

### 第三阶段（高风险）
1. **Power/PM**：suspend/resume 联动 Kernel 与 Driver  
2. **多总线协同**：设备热插拔 + 统一事件流
