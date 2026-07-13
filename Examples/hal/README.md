# HAL 示例入口

[`hal_demo`](hal_demo/main.cpp) 以 `WinGpio/WinUart/WinTimer` stub 验证 GPIO/UART/Timer driver
形状和最小 `init/write/start` 路径；构建入口见 [`CMakeLists.txt`](hal_demo/CMakeLists.txt)。

移植形状见 [`HAL ops backend 模板`](../../Modules/io/hal/hal_ops_template_guide.md)。Host stub 不证明
板级 bring-up 或真实外设行为。
