# Ink 示例入口

本目录收纳单色 / 墨水屏 UI 相关的共享 app 模块、宿主侧验证与板级端口样例。

如果你还没先看 UI 文档，建议先回到：

- [`../../docs/ui/README.md`](../../docs/ui/README.md)
- [`../ui/README.md`](../ui/README.md)

## 按任务进入

### 我想先看最小绘制与控件 demo

先看：

- [`demo/main.cpp`](demo/main.cpp)

### 我想看共享 app 模块

先看：

- [`app/`](app/)

### 我想看 Windows 宿主侧验证

先看：

- [`windows/main.cpp`](windows/main.cpp)
- [`windows/CMakeLists.txt`](windows/CMakeLists.txt)

### 我想看 STM32F103C8 端口

先看：

- [`stm32f103c8/`](stm32f103c8/)

### 我想看 H747 端口 / CM7 路径

先看：

- [`stn32h747xi/`](stn32h747xi/)

## 使用提醒

- 这条线更偏墨水屏 / 1bpp UI 与板级移植，不要和 Vivid 的 SDL3 主机示例混成一层。
- 如果你改的是共享 app 模块，宿主侧和板级端口都可能需要一起回看。
