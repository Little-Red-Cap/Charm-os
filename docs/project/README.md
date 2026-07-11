# 项目文档入口

本目录主要收纳 Charm 的项目规范、协作约定、推进材料、工具说明，以及若干项目级提案或设计记录。

这里默认不是系统行为真相的第一来源。  
如果某篇 `docs/project/*` 文档和系统契约、架构总览或当前代码冲突，优先以：

1. [`../architecture_overview.md`](../architecture_overview.md)
2. [`../system/README.md`](../system/README.md)
3. 当前代码与构建入口

为准。

## 按需求进入

### 我要开始改代码

先读：

- [`standards/README.md`](standards/README.md)
- [`standards/项目C++编码要求.md`](standards/项目C++编码要求.md)
- [`standards/嵌入式C++编程实践指南.md`](standards/嵌入式C++编程实践指南.md)
- [`standards/project_conventions.md`](standards/project_conventions.md)

### 我要看协作约定

先读：

- [`collaboration/《协作期待与规范》.md`](collaboration/《协作期待与规范》.md)
- [`collaboration/《现代 C++ 单片机代码协作认知》.md`](collaboration/《现代 C++ 单片机代码协作认知》.md)
- AI / Agent 相关入口仍在 [`../agent/README.md`](../agent/README.md)

### 我要看当前推进状态、认领和 backlog

先读：

- [`tracking/README.md`](tracking/README.md)
- [`tracking/主框架全仓审查与收敛_backlog.md`](tracking/主框架全仓审查与收敛_backlog.md)
- [`tracking/推进TODO与分工.md`](tracking/推进TODO与分工.md)

### 我要处理终端 / 编码 / PowerShell 环境

读：

- [`tooling/Powershell设置utf8.md`](tooling/Powershell设置utf8.md)

### 我要处理 CMake preset / 构建目录 / 工具链入口

先回到：

- [`../../README.md`](../../README.md) 里的 `CMake Presets` 构建入口
- [`../agent/routes/build.md`](../agent/routes/build.md)

### 我要看项目级提案或特定方向设计

本目录里有一批明显属于“提案 / 草案 / 设计讨论”的文档，例如：

- `charm_*草案 / 提案`
- `escape_hatches.md`
- `other.md`
- `player_design.md`
- `player_工程变体模型第一轮落地草案.md`
- `usb_storage_bundle_设计草案.md`

这些文档通常有参考价值，但默认不应被当成“现行系统契约”。

## 先怎么理解这个目录

- `standards/`
  偏现行项目规范，是改代码前最值得先看的部分。
- `collaboration/`
  偏协作期待、沟通方式和工程协同认知。
- `tracking/`
  偏推进、认领、问题清单和阶段性 backlog。
- `tooling/`
  偏环境与工具使用说明。
- 根目录若干 `草案 / 提案 / design`
  偏方向讨论、项目设计或特定子线记录。

## 当前使用建议

- 需要“当前该怎么写代码”，优先回到 `standards/`。
- 需要“现在正在推进什么”，优先回到 `tracking/`。
- 需要“为什么会有这些提案”，再去看根目录的草案与设计记录。
- 如果你只是第一次进入项目，不要先从 `tracking/` 或提案文档开始建立认知。
