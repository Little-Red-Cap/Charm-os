# System 示例入口

本目录收纳 system 装配、device model、runtime slot export、AppHost 与 power 相关样例。

如果你还没先看系统侧文档，建议先回到：

- [`../../docs/system/README.md`](../../docs/system/README.md)
- [`../../docs/architecture/driver_model.md`](../../docs/architecture/driver_model.md)

## 按任务进入

### 我想看 AppHost / poster / deferred signal

先读：

- [`app_host_poster_demo/README.md`](app_host_poster_demo/README.md)

### 我想看 runtime device 如何收口成稳定 capability

先读：

- [`device_runtime_block_slot_demo/README.md`](device_runtime_block_slot_demo/README.md)
- [`device_runtime_channel_slot_demo/README.md`](device_runtime_channel_slot_demo/README.md)

### 我想看 device bus / registry 的最小闭环

先看：

- [`device_bus_demo/`](device_bus_demo/)
- [`device_registry_demo/`](device_registry_demo/)

### 我想看 power 策略与 trace

先看：

- [`power_demo/`](power_demo/)

## 使用提醒

- 这里偏 system 能力验证，不要和 bootloader 主线或板级 bring-up 文档混成一层。
- 如果你在看启动链路，请回到 [`../boot/README.md`](../boot/README.md) 与 [`../../docs/boot/README.md`](../../docs/boot/README.md)。
