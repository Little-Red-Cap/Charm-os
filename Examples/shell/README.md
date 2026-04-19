# Shell 示例入口

本目录收纳 shell、stream console 与 modulex 相关的主机侧验证样例。

当前建议先看：

- [`service_shell/main_module.cpp`](service_shell/main_module.cpp)
- [`service_shell/CMakeLists.txt`](service_shell/CMakeLists.txt)

## 当前示例

### `service_shell`

这个目录里目前有三条常用入口：

- [`service_shell/main_module.cpp`](service_shell/main_module.cpp)：modulex 镜像装载、重定位、外部符号绑定与依赖校验。
- [`service_shell/main.cpp`](service_shell/main.cpp)：shell 命令解析、`echo/help`、stream console 验证。
- [`service_shell/main_time.cpp`](service_shell/main_time.cpp)：时间 / delay 适配接口的最小接线。

## 使用提醒

- 当前 CMake 默认编译的是 `main_module.cpp`。
- 如果你切换示例入口，记得同步调整 [`service_shell/CMakeLists.txt`](service_shell/CMakeLists.txt)。
