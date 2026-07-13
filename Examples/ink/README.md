# Ink 示例入口

本目录收纳单色 / 墨水屏 UI 相关的共享 app 模块、宿主侧验证与板级端口样例。

## 入口

| 任务 | 入口 |
|---|---|
| UI/Ink 契约 | [`docs/ui`](../../docs/ui/README.md)、[`Examples/ui`](../ui/README.md) |
| 最小绘制与控件 | [`demo/main.cpp`](demo/main.cpp) |
| 共享 app | [`app/`](app/) |
| Windows Host | [`windows/main.cpp`](windows/main.cpp)、[`windows/CMakeLists.txt`](windows/CMakeLists.txt) |
| STM32F103C8 | [`stm32f103c8/`](stm32f103c8/) |
| H747 / CM7 | [`stn32h747xi/`](stn32h747xi/) |

## 使用提醒

- Ink/1bpp 与 Vivid SDL3 是不同实现路径；一方通过不证明另一方成立。
- 修改共享 app 时必须核对受影响的 Host 与板级 consumer。
