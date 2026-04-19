# Out 示例入口

本目录收纳 `out` 格式化、console 输出与宿主/MCU 端接线样例。

## 当前内容

### 共享示例主体

- [`example.cpp`](example.cpp)

这个文件集中演示：

- 基础格式化
- 自定义 formatter
- domain / level
- ANSI 样式
- 二进制与浮点格式开关

### Windows 宿主侧接线

- [`windows/`](windows/)

这里用 bringup console 把 [`example.cpp`](example.cpp) 跑起来。

### STM32F103C8 端口样例

- [`stm32f103c8/`](stm32f103c8/)

## 使用提醒

- 这里偏输出层能力验证，不等同于完整日志系统或应用侧诊断入口。
- 如果你修改 `out` API 形状，建议同步检查宿主和 MCU 两条接线是否都还能工作。
