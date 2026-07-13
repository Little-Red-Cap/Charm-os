# Scope Project 示例

该示例验证一个小型项目如何分离 portable app 与平台 backend，不定义通用 Project 模型。

## 当前边界

- [`scope.app.cppm`](scope.app.cppm) 生成模拟波形、触发、测量与图表数据，不依赖 Win 入口。
- [`win/main.cpp`](win/main.cpp) 提供当前唯一 backend，使用 SDL3/Vivid。
- 构建入口由根与 [`win/CMakeLists.txt`](win/CMakeLists.txt) 维护。

当前没有 MCU backend 或跨平台运行证据；Win 示例通过不能证明板级显示、输入、采样或实时性。
