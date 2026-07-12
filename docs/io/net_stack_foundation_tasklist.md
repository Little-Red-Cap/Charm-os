# 网络栈底座任务摘要

> **文档状态：`archive summary`**

完整任务单见
[`../archive/net-stack-v0/net_stack_foundation_tasklist.md`](../archive/net-stack-v0/net_stack_foundation_tasklist.md)。它记录了 2026-04 阶段推进，不是当前 backlog。

## 已落地

- `charm.net` 聚合入口；
- socket v0、stub/Win backend 和 `net.api` facade；
- POSIX socket fd bridge；
- reactor/channel/typed session 承载；
- ARP、IPv4、UDP、ICMP、forwarding 和 route inspection 的 Host smokes。

## 仍需按具体契约判断

- 跨目标行为一致性；
- real-board NIC/driver/backend；
- 完整 TCP/IP、IPv6 与 Linux socket compatibility；
- 性能、资源上限、并发与安全边界。

新增工作应进入具体模块 issue、测试或契约，不继续向历史 tasklist 追加阶段日志。
