# Project 示例入口

本目录收纳“项目化”示例，也就是不止验证单个模块，而是验证一条更接近真实产品形态的组合路径。

当前最值得优先看的仍然是：

- `player/`

## 当前项目线

### Player

入口：

- [`player/README.md`](player/README.md)

这是当前最完整、最活跃的一条项目化示例线，覆盖：

- 平台无关应用层
- Ink / Vivid UI 变体
- Windows host 路径
- STM32 板级路径
- profile / runtime / bring-up 组合

### Scope

入口：

- [`scope/README.md`](scope/README.md)

这条线更轻，适合看一个相对克制的项目化结构样例。

### DAPLink（独立仓库）

DAPLink 项目已迁移到独立仓库：

- [Little-Red-Cap/Charm-dap](https://github.com/Little-Red-Cap/Charm-dap)

它继续承载 MCU / 板级工程化路径和 CMSIS-DAP 实现，不再作为 Charm 的内置项目目录。

### Nocturne（独立仓库）

STM32G431 USB Audio 项目已迁移到独立仓库：

- [Little-Red-Cap/Nocturne](https://github.com/Little-Red-Cap/Nocturne)

它继续承载 Nocturne 的 STM32 固件、硬件与产品资料，不再作为 Charm 的内置项目目录。

## 建议怎么用这个目录

- 想看“当前最接近真实产品”的主线：
  先看 `player/`
- 想看一个更小一点的项目化示例：
  看 `scope/`
- 想看 MCU 工程化 / DAP 工具链方向：
  进入独立的 `Charm-dap` 仓库
- 想看 STM32 USB Audio 产品固件：
  进入独立的 `Nocturne` 仓库

## 使用提醒

- 项目化示例会同时包含平台无关代码、板级实现、构建预设、资产甚至硬件资料，不要把它们误当成纯模块示例。
- 如果项目化示例里的做法与仓库总契约冲突，优先回到 [`../../docs/README.md`](../../docs/README.md) 和 [`../../docs/system/README.md`](../../docs/system/README.md) 复核。
