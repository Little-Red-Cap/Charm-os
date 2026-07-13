# HAL 示例入口

本目录收纳 HAL 抽象与驱动接口的最小验证样例。

当前建议先看：

- [`hal_demo/main.cpp`](hal_demo/main.cpp)
- [`hal_demo/CMakeLists.txt`](hal_demo/CMakeLists.txt)
- [`HAL ops backend 模板`](../../Modules/io/hal/hal_ops_template_guide.md)

## 当前示例

### `hal_demo`

这个示例当前覆盖：

- `WinGpio` / `WinUart` / `WinTimer` 这类宿主侧 stub driver
- `hal::GpioDriver` / `hal::UartDriver` / `hal::TimerDriver` 的接口形状验证
- 最小 `init / write / start` 路径

## 使用提醒

- 这里偏 HAL 抽象与移植骨架验证，不等同于板级 bring-up 文档。
- 如果你要看板级上下文与硬件上板资料，请回到 [`../../docs/board/README.md`](../../docs/board/README.md)。
