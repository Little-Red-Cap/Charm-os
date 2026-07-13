# 真实板级落地脱节审计 v0 保留笔记

> status: `archived`
>
> scope: 早期 `stm32h747-player` 的默认路径与装配问题

本文中的项目路径和 P0-P4 排期已经过期。现行审计方法见
[`../../architecture/real_board_landing_gap_audit_v0.md`](../../architecture/real_board_landing_gap_audit_v0.md)。

## 当时的证据

早期 H747 工程同时包含：

- 板级直写输出 `h747::console::*`；
- `ConsoleCaps -> io.uart1 -> io.console0 -> io::Channel`；
- `board/port`、`board/services/*`、`apps/*` 与 `charm_landing` 分层；
- `charm_smoke`、`system_probe` 及多个独立 probe app。

对应文件包括：

- `board/services/console/console.cpp`；
- `board/services/charm_landing/h747_charm_landing.cppm`；
- `apps/charm_smoke/main.cpp`；
- `apps/system_probe/main.cpp`。

这些路径属于当时的 `stm32h747-player` 快照，不能用于判断当前仓库状态。

## 暴露的问题

### 控制台路径并行

`charm_smoke` 和 `system_probe` 已建立 `io.console0`，但运行期仍大量直写
`h747::console::*`。`out.channel`、`out.api` 和 board console 之间没有形成默认接线。

### app 私有诊断面重复

多个 probe app 各自维护 banner、help、status、alive 日志和命令命名。共享协调层因缺少统一的
只读 status/snapshot 接口，无法稳定比较或汇总各 service 状态。

### 分层存在但默认接法未闭合

当时可见的目标链为：

```text
board facts -> BoardCaps/ConsoleCaps -> bringup -> io.console0 -> out
```

组件和部分桥接已经存在，但模板、profile 与示例没有把这条链设为新 board/app 的默认路径。

### 规则没有进入工程入口

文档已要求输出走 `out.*`、时间走 `charm.system.clock`、初始化走 `init.graph`、通道经
`io.registry`，但模板、最小样例和构建检查没有执行这些约束。仅写“推荐”没有改变工程默认行为。

## 当时的归因

该问题不能归为“没有控制台能力”。当时仓库已包含：

- `Modules/io/out/out.api.cppm`；
- `Modules/io/out/out.channel.cppm`；
- `Modules/system/bringup/system_bringup_console.cppm`；
- `Modules/system/bringup/system_bringup.cppm`；
- `Modules/platform/platform.board.cppm`。

主要缺口是能力不可发现、接缝成本高、已有接线不是默认路径，以及规则没有进入模板或检查。
真实板项目提供了这些问题的运行证据，但其局部 HAL、shell 和 workaround 不构成跨平台契约。

## 历史局部术语

- `DefaultConsolePath`：当时建议的
  `BoardCaps/ConsoleCaps -> io.console0 -> out.channel_sink -> out.api/out.logger`；
- `EarlyConsole`：限于 pre-graph、fault 和极早期生存证据的 UART 直连例外；
- `ServiceSnapshotContract`：供共享协调层读取的无副作用、可重复 status/snapshot；
- `EvidenceRig`：用于暴露时序、cache、外设和装配问题的真实板实验工程。

这些名称没有获得 Constitution/Core vocabulary 准入资格。现行文档只保留其中的审计边界。

## 历史回收顺序

1. `P0`：统一局部术语和问题分类；
2. `P1`：把运行期输出收敛到 `io.console0 -> out.*`，限制 early console；
3. `P2`：统一 bringup、shell、status 与只读 snapshot；
4. `P3`：让 `BoardCaps`、`ConsoleCaps` 和 bringup 成为 board landing 默认入口；
5. `P4`：再按同一准入方式处理 Audio、Storage、Net 与 Display。

该顺序只记录当时的迁移判断，不是当前 roadmap。可保留的结论是：发现组件存在不等于默认路径
已经成立；默认行为必须由 source、CMake、profile、模板和运行证据共同证明。
