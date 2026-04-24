# IO 示例入口

本目录收纳输入采样、pump、网络协议栈、协议传输和输出链路相关的验证样例。

如果你还没先看 IO / 输入文档，建议先回到：

- [`../../docs/io/README.md`](../../docs/io/README.md)
- [`../../docs/input/README.md`](../../docs/input/README.md)

## 按任务进入

### 我想看输入采样 / pump

先看：

- [`input_pump_win_demo/README.md`](input_pump_win_demo/README.md)
- [`raw_input_win_demo/main.cpp`](raw_input_win_demo/main.cpp)

### 我想看网络协议栈 / reactor / socket

先看：

- [`net/README.md`](net/README.md)

### 我想看协议传输样例

先看：

- [`canopen_sdo_demo/main.cpp`](canopen_sdo_demo/main.cpp)
- [`xymodem_demo/main.cpp`](xymodem_demo/main.cpp)

### 我想看输出 / bringup console / 格式化

先看：

- [`out/README.md`](out/README.md)

## 使用提醒

- 这里偏 IO 路径与协议/适配层验证，不直接替代系统装配或产品化应用入口。
- 如果你在看 USB、bootloader 或 UI 组合场景，优先回到对应专题目录，而不是把这里当成总入口。
