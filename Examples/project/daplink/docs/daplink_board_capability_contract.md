# DAPLink Board Capability Contract

这份说明描述 `Examples/project/daplink` 当前认可的板级边界。

目标很明确：

- 让 `app/` 保持协议与调度视角
- 让 `frontends/` 保持主机接入视角
- 让具体板级事实只停留在端口后端与能力组合层

## 能力拆分

当前板级 support 被刻意拆成三组能力：

- `TargetPins`
  - 负责 `SWCLK`、`SWDIO`、`nRESET`、Hi-Z 行为与 reset 策略
  - SWD 引擎只应该依赖这一组能力
- `Indicators`
  - 负责 connect LED / running LED 等状态指示
  - 没有 LED 不是特例，而是合法的 no-op capability
- `UsbConnect`
  - 负责板级 USB connect switch、pull-up enable 等硬件差异
  - 没有硬件开关同样应表现为合法的 no-op capability

## 组合规则

每个具体端口应暴露：

- `daplink::board_target::TargetPins`
- `daplink::board_target::Indicators`
- `daplink::board_target::UsbConnect`
- `daplink::board_target::Support`

其中：

- `Support` 更像一个便于现有调用点落地的 facade
- 新代码应尽量依赖自己真正需要的最小能力，而不是一把抓整块 `Support`

## 为什么这样拆

这样做的价值不只是“代码好看”，更关键的是为后续移植收边界：

- 新增一个 STM32 端口时，优先只补板级事实
- 新增一个非 STM32 端口时，也能先按同一组能力补齐板级契约
- 避免把“有没有 LED / 有没有 USB 开关”这类差异重新写回 `app/`

## 端口接入建议

如果继续增加 STM32 端口：

- 优先复用 `platform/stm32/*` 帮助层
- 端口目录只补 pin map、LED、USB connect 这类事实

如果未来增加非 STM32 端口，例如 `ESP32`：

- 先提供三组等价板级能力
- 再讨论是否需要更高层改动

换句话说，板级能力先对齐，协议层不要先被迫感知新平台。
