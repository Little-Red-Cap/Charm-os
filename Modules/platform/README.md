# `Modules/platform` 实现状态

## 文档状态

- `status`: `supporting`
- `scope`: 当前 platform/board module 与 host fixture
- `source`: `cmake/sources/CharmPlatformSources.cmake`、`platform.cmake`、`baremetal.cmake`

本目录仍被现有 target 和示例使用，不是待删除目录，也不因名称成为 Charm Core 或稳定 backend
contract。

| 路径 | 当前职责 |
|---|---|
| `platform.board.cppm` | `BoardCaps` 与 boot/load/exec 等板级 capability shape |
| `platform.board_facts.cppm` | board fact 数据形状 |
| `platform.irq.cppm` | IRQ guard 抽象 |
| `boards/*_stub/` | ARMv7-A、STM32 与 Windows board fixture |
| `win/` | time、wakeup、power 与 IRQ host helper |

`CharmPlatformSources.cmake` 收集公共 platform modules；其它 source collection 仍显式接入 Windows
fixture。移动或删除前必须先迁移 import consumer、CMake source collection 和对应 smoke。

本页不规定未来目录布局。公共语义准入、backend ownership 或 project/BSP 归属应通过
[`architecture route`](../../docs/agent/routes/architecture.md) 裁决；现有 module 只按当前源码和
consumer 解释。
