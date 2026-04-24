# DAPLink Port 分层契约

这份契约描述 `Examples/project/daplink` 当前认可的分层边界。

目标不是为了把 STM32 抽象成“没有差异”，而是把差异固定在应该出现的位置，
避免随着移植次数增加，把芯片细节重新污染回公共层。

## 1. 分层职责

### `app/`

- 负责 DAPLink 主流程、CMSIS-DAP 状态机、CDC 桥接策略、调度与协议逻辑。
- 默认不直接依赖具体芯片宏，不直接感知某一颗 MCU 的寄存器名字或 HAL 句柄名。
- 如果后续继续增加端口，`app/` 应尽量保持无需改动，或者只改纯策略逻辑。

### `frontends/`

- 负责“主机如何看到这台 DAPLink 设备”。
- 当前实现是 USB CDC / HID。
- 未来如果要接入无线链路、WebUSB、BLE、Wi-Fi 等，也应该优先扩展这里，而不是回写到 `app/`。

### `port/`

- 负责统一契约、通用 glue，以及跨多个端口可复用的帮助层。
- `daplink::port::*` 是公共层唯一应该依赖的底层 API 入口。
- `daplink::board_support::*` / `daplink::backend_support::*` 负责把板级与后端能力拆成可组合积木。
- 当某个能力已经被两个及以上 STM32 端口复用时，优先抽到 `port/stm32/`。

### `f103/`、`g431/`、`h503/`

- 这些目录承担“端口后端”角色，而不是继续扩散芯片特判。
- 端口目录只保留该端口独有的事实：
  - CubeMX 生成代码
  - 板级 pin map
  - UART / USB 句柄绑定
  - 初始化顺序差异
  - PMA / USB 布局差异
  - 某颗芯片独有的 workaround

## 2. 组合方式

当前推荐通过 traits 组合描述端口，而不是在公共层写 `#if defined(STM32xxxx)`。

### 板级 traits

- `Core/Inc/daplink_board.hpp` 只声明 `daplink::board_target::Traits`。
- traits 优先由 `board_support` 的能力积木组合得到，例如：
  - 通用 SWD / RESET pin map
  - USB connect switch 能力
  - LED 能力
  - reset 行为能力

### 后端 traits

- `Core/Inc/daplink_backend.hpp` 只声明 `daplink::backend_target::Traits`。
- traits 优先由 `backend_support` 的能力积木组合得到，例如：
  - UART1 / UART2 初始化与句柄
  - USB PCD 句柄类型
  - DMA-before-UART2 这类初始化顺序能力

### 端口 API

- `Core/Inc/daplink_port_api.hpp` 负责把具体 HAL / SDK 暴露成统一的 `daplink::port::*`。
- 如果未来移植到非 STM32 平台，也应优先实现同名契约，而不是把新平台判断写入公共逻辑。

## 3. 新端口接入规则

新增一个端口时，默认按下面顺序处理：

1. 先实现 `daplink_port_api.hpp`，把底层能力接到 `daplink::port::*` 契约。
2. 再实现 `daplink_board.hpp`，只描述板级 pin、LED、reset、USB connect 等事实。
3. 再实现 `daplink_backend.hpp`，只描述 UART/USB 句柄与初始化顺序。
4. 如果发现已有两个以上端口在重复同一段 glue，回抽到 `port/` 或 `port/stm32/`。
5. 除非是协议策略本身变化，否则不要为移植新芯片去修改 `app/`。

## 4. 允许存在的差异

这套分层并不要求所有端口长得完全一样。

下面这些差异是允许而且合理的：

- 不同 USB PMA / FIFO / endpoint 布局
- 不同 reset 保持策略
- 不同 USB 连接开关电路
- 不同 UART FIFO / ORE / 数据寄存器细节
- 不同芯片族的特殊 workaround

关键要求只有一个：差异应该收敛在端口后端或公共 helper，不应蔓延回协议与应用层。

## 5. 面向后续扩展

如果以后继续增加 STM32 端口，这套分层应该让新增工作更接近“组合已有能力，再补少量新能力”。

如果以后增加非 STM32 端口，甚至是无线 DAPLink 之类的前端变化，也应优先满足以下目标：

- `app/` 继续保持芯片无关
- `frontends/` 承接主机接入方式变化
- `port/` 承接平台契约和可复用 glue
- 具体端口目录只保留该平台不可避免的事实
