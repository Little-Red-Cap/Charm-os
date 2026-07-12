# 网络栈分层摘要

> **文档状态：`supporting`**

完整起步设计见
[`../archive/net-stack-v0/net_stack_dual_surface_design.md`](../archive/net-stack-v0/net_stack_dual_surface_design.md)。当前 socket 行为以
[`net_socket_v0_contract.md`](net_socket_v0_contract.md) 和源码为准。

## 对外边界

应用优先使用 `net.api` 的 `TcpClient`、`TcpListener`、`UdpSocket` 和端点类型。backend handle、packet pool、ARP table、route table 与 reactor driver 不进入普通应用调用面。

## 对内边界

- `net.socket`：socket 状态与 backend type erasure；
- `net.backend.*`：stub、Win 等环境实现；
- `net.api`：轻量用户 facade；
- `net.arp/ipv4/udp/icmp/forward`：自研 packet/data plane；
- `net.reactor*`：事件推进与 channel 接入；
- `net.posix`：fd/errno 投影。

这些模块属于实现层，不构成 Charm Core。自研 packet stack 和 OS socket backend 是两种承载路径，不能假设内部状态或完整协议行为一致。

## 固定约束

- socket 调用默认非阻塞；暂不可推进返回 `would_block`；
- buffer 生命周期不超出一次调用，除非具体接口明确拥有数据；
- backend 差异不得改变 v0 状态机和错误类别；
- IPv6、完整 Linux socket options、完整 TCP/IP 和产品级网络安全不在 v0 承诺内。
