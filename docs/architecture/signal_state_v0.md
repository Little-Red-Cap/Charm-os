# Signal / State v0 历史设计说明

状态：exploration summary。

原文包含 signal、state、poster、`init.connection`、scheduler 与 UI 表面的阶段性组合讨论。
其中部分语义已经由源码和 smoke 落地，另一些仍只是设计推演。为避免它与现行契约形成
双重入口，完整版本已移入：

- [`../archive/signal-state-v0/signal_state_v0.md`](../archive/signal-state-v0/signal_state_v0.md)

当前实现与使用边界只读：

- [`signal_state_contract_v0.md`](signal_state_contract_v0.md)

归档内容可以用于追溯取舍，但不得据此声称自动 wiring、跨上下文调度或 UI 观察面已经由
Signal / State 原语统一实现。
