# DAPLink STM32 平台样板说明

这份说明记录当前 `platform/stm32/` 这一层在 `Examples/project/daplink` 里的定位。

它的目标不是再解释一遍 `STM32` 怎么用，而是把这层明确成未来 `platform/<vendor>/` 的参考模板。

## 当前角色

`platform/stm32/` 现在承担三件事：

1. 提供 `STM32` 家族的平台实现
2. 提供 `STM32` 家族共用的 USB layout / board / backend helper
3. 作为未来新增 `platform/esp32/`、`platform/rp2040/` 时的目录模板

换句话说，它不是“默认世界”，而是“第一个平台实现样板”。

## 样板结构

### `daplink_platform_stm32_api_support.hpp`

负责平台原语实现：

- runtime
- gpio
- uart
- usb device backend

这里定义的是 `Platform` 能力面，而不是端口事实。

### `daplink_platform_stm32_usb_layout_support.hpp`

负责 USB layout 表达：

- `UsbPmaLayout`
- `UsbPmaLayoutTraits`
- `F1ScaledUsbPmaLayoutTraits`

当前具体端口只需要选择自己的 `UsbLayout`，而不需要重新发明一套平台 API。

### `daplink_platform_stm32_contract_check.hpp`

负责集中编译期校验：

- `STM32 Platform`
- 一个默认 `UsbLayout`
- 满足 `platform_contract::PortPlatform`

以后新增别的平台，也应优先保留一个同职责的集中 contract check 文件。

### `daplink_platform_stm32_port_bridge.hpp`

负责把平台实现桥接到当前 `daplink::port::*` 公共桥面。

这层存在的意义是：

- 让 `app/`、`port/`、`frontends/` 仍然依赖统一桥面
- 让 `platform/stm32/` 可以继续作为独立平台实现存在

未来如果新增 `platform/esp32/`，也应优先提供自己的 `port bridge`，而不是回头改公共层调用风格。

## 端口目录应该做什么

当前 `f103 / g431 / h503` 端口目录在平台接入面只做一件事：

- 选择 `platform/stm32/` 这个平台样板
- 选择自己的 `UsbLayout`

这正是我们希望未来继续保持的模式：

- 平台目录负责平台能力
- 端口目录负责端口事实

## 对未来非 STM32 平台的启发

如果未来加 `ESP32`，推荐直接对照这份样板补齐：

1. `platform/esp32/daplink_platform_esp32_api_support.hpp`
2. `platform/esp32/daplink_platform_esp32_usb_layout_support.hpp`
3. `platform/esp32/daplink_platform_esp32_contract_check.hpp`
4. `platform/esp32/daplink_platform_esp32_port_bridge.hpp`

即使第一版实现不完全一样，职责划分也应尽量对齐，而不是重新把平台差异打散到公共层。
