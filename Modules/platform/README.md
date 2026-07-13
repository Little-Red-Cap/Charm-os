# `Modules/platform`

## 文档状态

- `status`: `supporting`
- `scope`: platform/board module 与 host fixture
- `source`: `cmake/sources/CharmPlatformSources.cmake`、`platform.cmake`、`baremetal.cmake`

这些 module 是现有 target 的 platform/board 实现，不因目录名称成为 Charm Core 或稳定 backend contract。

| 路径 | 当前职责 |
|---|---|
| `platform.board.cppm` | `BoardCaps` 与 boot/load/exec 等板级 capability shape |
| `platform.board_facts.cppm` | board fact 数据形状 |
| `platform.irq.cppm` | IRQ guard 抽象 |
| `boards/*_stub/` | ARMv7-A、STM32 与 Windows board fixture |
| `win/` | time、wakeup、power 与 IRQ host helper |

`CharmPlatformSources.cmake` 收集公共 platform modules；其它 source collection 仍显式接入 Windows
fixture。公共语义准入、backend ownership 或 project/BSP 归属从
[`architecture route`](../../docs/agent/routes/architecture.md) 进入。
