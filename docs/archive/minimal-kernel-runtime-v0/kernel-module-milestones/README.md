# Kernel module milestone 保留笔记

> `status`: `archived`

本目录只保留早期 kernel module 讨论中仍可独立消费的语义约束，不约束当前实现。

## Sync/IPC 竞争语义

- notify、timeout 与 cancel 中先成功移除 waiter 的操作决定结果；后续操作应稳定失败，而不是重复唤醒。
- notify 或 cancel 移除带 timeout 的 waiter 时，还必须取消对应 timeout event。
- wait list 满、timeout 排程失败或目标 token 不存在时必须显式失败。
- 同一 sync 实例中的 wait token 必须唯一；否则按 token 删除会产生歧义。

## Thread 职责

- cooperative thread 适合由 event/tick 驱动的单步逻辑。
- blocking thread 在 blocked 状态只接收明确允许的事件；sync、init、terminate 是早期默认放行集合。
- unblock mask 是任务策略，不应被隐藏成 scheduler 的全局规则。

旧 kernel/util 目录草案、Config 伪代码、M1/M2/M3 里程碑、Windows demo 路径、CSV 字段和 feature
freeze 已删除。现行行为必须读取 [`docs/system/README.md`](../../../system/README.md) 与
[`Modules/system/kernel`](../../../../Modules/system/kernel/) 源码，并以当次测试为准。
