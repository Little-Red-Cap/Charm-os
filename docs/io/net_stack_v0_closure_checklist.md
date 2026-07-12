# 网络栈 v0 关单摘要

> **文档状态：`archive summary`**

完整关单清单见
[`../archive/net-stack-v0/net_stack_v0_closure_checklist.md`](../archive/net-stack-v0/net_stack_v0_closure_checklist.md)。

v0 “关单”只表示以下底座已有可重复 Host 证据：

- socket facade 与状态机；
- stub/Win backend 的最小共同语义；
- POSIX fd bridge；
- reactor 和 typed service 的代表路径；
- 部分 ARP/IPv4/UDP/ICMP/forwarding 数据面。

它不表示网络栈完整、所有 smoke 当前绿色、真实板已验证或公共 ABI 永久冻结。新增协议和 backend 必须维护各自契约与测试，不重新扩展本关单文档。
