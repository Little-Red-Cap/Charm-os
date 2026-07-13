# Shell 示例入口

| `service_shell` 入口 | 局部范围 |
|---|---|
| [`main_module.cpp`](service_shell/main_module.cpp) | ModuleX load、relocation、external binding 与 dependency 检查 |
| [`main.cpp`](service_shell/main.cpp) | shell command 与 stream console |
| [`main_time.cpp`](service_shell/main_time.cpp) | time/delay adapter |

[`CMakeLists.txt`](service_shell/CMakeLists.txt) 当前构建 `main_module.cpp`；切换入口必须显式修改该源清单。
