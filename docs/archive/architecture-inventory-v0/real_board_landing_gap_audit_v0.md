# 真实板级落地脱节审计 v0 保留笔记

> `status`: `archived`

本文保留早期 `stm32h747-player` 暴露的默认路径与装配反例。项目路径和迁移排期已失效；现行审计
方法见 [`real_board_landing_gap_audit_v0.md`](../../architecture/real_board_landing_gap_audit_v0.md)。

## 当时的装配快照

工程同时存在板级直写 `h747::console::*`，以及
`ConsoleCaps -> io.uart1 -> io.console0 -> io::Channel` 路径；`board/port`、`board/services`、`apps` 和
`charm_landing` 已分层，并有 `charm_smoke/system_probe` 等 probe app。

这只能说明组件和局部桥接存在，不能证明它们形成默认路径。具体文件名属于历史快照，当前状态应从
source、CMake、profile 与运行证据重新确认。

## 暴露的问题

- **Console path 并行**：`io.console0` 已建立，runtime 仍大量直写板级 console；early/fault 例外没有
  与正常输出路径分开。
- **App 私有诊断重复**：probe app 各自维护 banner、help、status 和 alive log，缺少统一的只读
  status/snapshot consumer boundary。
- **默认接线未闭合**：目标链 `board facts -> caps -> bringup -> io.console0 -> out` 可见，但 template、
  profile 和 sample 没有把它设为新 board/app 默认入口。
- **规则未工程化**：文档要求使用 `out.*`、clock、`init.graph` 与 registry，构建、模板和 failure check
  却未执行这些约束。

## 当时的归因

这不是“没有 console 能力”。仓库已有 out channel/API、bring-up console 和 platform board 组件；主要
缺口是能力不可发现、接缝成本高、可行接线不是默认路径，以及规则没有进入工程入口。

真实板项目提供了 timing、cache、peripheral 和 composition 的反例，但其 HAL、shell 和 workaround 不
构成跨平台 contract。可复用结论只有：组件存在不等于默认路径成立；默认行为必须由 source、CMake、
profile、template 和 runtime evidence 共同证明。

当时使用过 `DefaultConsolePath`、`EarlyConsole`、`ServiceSnapshotContract` 和 `EvidenceRig` 等局部术语。
它们没有获得 Core vocabulary 准入资格，现行文档只保留对应审计边界。
