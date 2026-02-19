# Scope Project 示例

此目录用于便携式示波器“项目化”验证，平台无关代码放在项目根目录，平台相关代码放在子目录（如 win、stm32）。

## 目录结构

```
Examples/project/scope/
    CMakeLists.txt
    README.md
    scope.app.cppm
    win/
        CMakeLists.txt
        main.cpp
```

## 说明

- `scope.app.cppm`：平台无关的最小应用封装，包含模拟波形、触发、测量与图表数据整理。
- `win/`：PC 端实现（SDL3 / Vivid UI）相关代码。
- 后续移植到 MCU 时，在 `stm32/` 下新增平台实现即可。
