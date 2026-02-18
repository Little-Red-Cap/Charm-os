# HAL 平台绑定指南 (Draft)

## 目标
为每个平台提供最小 HAL 绑定，实现 hal_* 接口，不污染核心模块。

## 绑定位置
- 平台实现放在 Draft/Examples/targets 或未来 charm-hal 子仓库中。
- 核心 Modules 中只保留接口与概念。

## 需要实现的接口（MVP）
- hal_time: TimeSource / DelayProvider
- hal_irq: IrqGuard / IrqController
- hal_gpio: GpioDriver
- hal_uart: UartDriver
- hal_timer: TimerDriver
- hal_clock: ClockProvider（可选）

## 约定
- 阻塞/超时语义需文档化。
- ISR 上下文与线程上下文的调用约束必须明确。
- 若不支持功能，请返回 Status::unsupported。

## 示例
- Windows stub: Draft/Examples/hal_demo/main.cpp
- STM32 stub: 推荐放 Draft/Examples/stm32f103c8/Core/Src/hal_*.cpp

