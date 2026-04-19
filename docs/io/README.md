# IO 文档入口

本目录收纳 Charm 当前与 `io.channel / io.reactor / io.registry` 核心三件套，以及网络协议栈起步设计相关的材料。

如果你是第一次进入仓库，先读：

1. [`../overview.md`](../overview.md)
2. [`../architecture_overview.md`](../architecture_overview.md)
3. [`../README.md`](../README.md)

再回到这里按任务进入。

## 先怎么理解这个目录

- `io_layering_overview.md`
  负责说明 IO 在仓库里的分层位置和主题边界，但不替代 `io.channel / io.reactor / io.registry` 的硬契约。
- `*_contract.md`
  优先视为现行规则、硬约束或阶段性收口契约。
- `net_*design/review/tasklist/checklist`
  主要服务网络协议栈这条线的设计、复盘与推进。

## 按任务进入

### 我想先建立 IO 的整体认知

先读：

- [`io_layering_overview.md`](io_layering_overview.md)

### 我想看 IO 核心三件套的现行规则

建议顺序：

1. [`io_channel_contract.md`](io_channel_contract.md)
2. [`io_reactor_contract.md`](io_reactor_contract.md)
3. [`io_registry_contract.md`](io_registry_contract.md)

这三篇更适合回答：

- 非阻塞 channel 允许什么、不允许什么
- reactor 如何承接事件驱动 IO
- registry 如何作为统一发现与打开入口

### 我想看网络协议栈这条线

建议顺序：

1. [`net_stack_dual_surface_design.md`](net_stack_dual_surface_design.md)
2. [`net_socket_v0_contract.md`](net_socket_v0_contract.md)
3. [`net_stack_stage_review.md`](net_stack_stage_review.md)

如果你在看推进状态，再继续：

- [`net_stack_foundation_tasklist.md`](net_stack_foundation_tasklist.md)
- [`net_stack_v0_closure_checklist.md`](net_stack_v0_closure_checklist.md)

## 当前建议阅读顺序

- 看总览：
  `io_layering_overview.md`
- 看硬规则：
  `io_channel_contract.md` → `io_reactor_contract.md` → `io_registry_contract.md`
- 看网络起步线：
  `net_stack_dual_surface_design.md` → `net_socket_v0_contract.md`

## 使用提醒

- 如果这里的说明和 `docs/system/*`、`docs/architecture/*` 或当前代码冲突，优先回到更上位入口复核。
- `io_layering_overview.md` 更适合先建立主题边界；真正的行为约束仍应回到三篇 `*_contract.md`。
- 当 channel / reactor / registry 的行为边界变化，或网络协议栈的现行入口发生变化时，应同步更新本目录入口。
