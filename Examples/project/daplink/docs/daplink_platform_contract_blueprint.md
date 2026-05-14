# DAPLink 平台契约与非 STM32 接入蓝图

这份文档不是“平台无关愿景”，而是未来真要新增 `platform/esp32/`、`platform/rp2040/` 这类目录时的实际接线蓝图。

## 目标

新增一个非 STM32 平台时，默认新增的是：

- `platform/<vendor>/`
- 对应端口目录

而不是回头修改：

- `app/`
- 现有 `STM32` 端口目录
- 已经稳定的前端主流程

## 平台契约分层

未来每个平台实现都应满足下面五组契约。

### 1. `PlatformRuntime`

负责最小运行时能力：

- `runtime_init`
- `fail_fast`
- `delay_ms`
- `nop`
- `system_core_clock_hz`
- `tick_ms`

这组能力只表达“运行时最小原语”，不表达任何平台业务语义。

### 2. `PlatformGpio`

负责 GPIO 与引脚电平能力：

- `GpioPort`
- `PinState`
- `GpioConfig`
- `gpio_init`
- `gpio_write`
- `gpio_read`

这组能力必须足以支撑：

- SWD bitbang
- reset 控制
- LED / USB connect 开关

### 3. `PlatformUart`

负责 UART 原语：

- `UartHandle`
- `uart_post_init_default`
- `uart_apply_line`
- `uart_clear_overrun`
- `uart_rx_ready`
- `uart_rx_pending`
- `uart_tx_ready`
- `uart_data_read`
- `uart_data_write`

如果某个平台没有完全等价的 ORE 语义，也应该在平台层内把它折成“清理接收错误状态”的同类能力，而不是回推到公共层。

### 4. `PlatformUsbDevice`

默认未来第一批非 STM32 平台仍走 USB DAPLink 路线，因此当前平台契约仍要求提供 USB device backend：

- `UsbPcdHandle`
- `UsbEndpointType`
- `usb_init_pcd`
- `usb_start`
- `usb_pma_config_single_buffer`
- `usb_set_address`
- `usb_ep_open`
- `usb_ep_receive`
- `usb_ep_transmit`
- `usb_ep_set_stall`
- `usb_ep_rx_count`
- `usb_copy_setup_packet`

如果某个平台暂时无法满足这组能力，应视为“平台能力未完成”，而不是去修改 `app/` 或 `frontends/usb/` 的行为契约。

### 5. `UsbLayoutContract`

当前仍要求平台或端口给出 USB endpoint / buffer layout 常量：

- `kUsbPmaEp0Out`
- `kUsbPmaEp0In`
- `kUsbPmaHidIn`
- `kUsbPmaHidOut`
- `kUsbPmaCdcCmd`
- `kUsbPmaCdcOut`
- `kUsbPmaCdcIn`

即使未来非 STM32 平台不是 PMA 架构，也应该先在平台层里给出等价布局描述，而不是把具体控制器差异泄漏到公共逻辑。

## 目录职责

### `platform/<vendor>/`

这里承接平台共性，不承接板级事实。

应该放在这里的内容：

- 平台 API support
- 平台 contract check
- 平台 USB layout helper
- 平台 board/backend 公共帮助层
- 平台特有 workaround

### 具体端口目录

这里只保留端口事实：

- 板级 pin map
- LED / reset / USB connect 开关事实
- 具体 UART / USB 句柄绑定
- SDK / CubeMX / toolchain 接线
- 该板独有初始化顺序差异

## 哪些层不该动

如果未来接入 `ESP32`，下面这些层默认不该因为平台迁移而新增条件分支：

- `app/`
- `frontends/usb/`
- 现有 `f103 / g431 / h503` 端口目录
- 根入口 CMake 的端口选择模型

如果未来接入时必须改这些层，优先说明的是平台契约还不够完整，而不是新平台太特殊。

## 新平台接入顺序

推荐顺序：

1. 先实现 `platform/<vendor>/` 的五组契约
2. 再补平台 contract check
3. 再补端口目录里的 board/backend 事实
4. 最后才接入实际端口构建与实机验证

不要反过来先在端口目录硬写一堆平台特判，再回头抽象。

## 当前默认边界

这份蓝图默认建立在两个前提上：

- 当前首要路线仍是 `STM32 DAPLink`
- 未来第一批非 STM32 端口默认优先仍走 USB DAPLink，而不是无线 / 网络前端

如果未来要做无线 DAPLink，那会是 `frontends/` 的下一阶段扩展课题，不属于这轮平台契约收口范围。
