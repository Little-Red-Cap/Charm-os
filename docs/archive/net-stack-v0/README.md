# Network Stack v0 保留说明

> **状态：`archived`**
>
> 本文保留早期双表面设计的取舍，不定义当前 socket 行为。现行入口见
> [`net_stack_dual_surface_design.md`](../../io/net_stack_dual_surface_design.md) 和
> [`net_socket_v0_contract.md`](../../io/net_socket_v0_contract.md)。

## 双表面

应用表面只暴露 endpoint、TCP client/listener、UDP socket 与少量高频动作；backend handle、packet
pool、ARP/route table、driver 和 reactor adapter 留在实现层。这样可以让应用 API 保持稳定，同时允许
Host OS socket backend 与自研 packet/data plane 分别演进。

两种承载路径只共享外部状态与错误边界，不能假设内部连接状态、buffer ownership、协议覆盖或时序
一致。Host loopback 成功也不证明自研 IPv4/UDP/ICMP/TCP 路径成立。

## Ownership 与推进

- 调用方提供的 buffer 默认只在一次调用期间有效；需要异步保留时必须显式复制或转移 ownership。
- socket 操作暂不可推进时返回 `would_block`，不在协议层 busy-spin、sleep 或隐藏重试。
- readiness 由 backend 产生，经 reactor/channel ingress 进入 task context；基础 socket 不拥有 scheduler。
- short read/write、EOF、detach 与 backend fault 是不同结果，不能用统一“网络失败”折叠。
- packet pool、route/ARP state 和 retransmission storage 的容量与溢出策略属于具体实现，不进入应用 ABI。

## 分层边界

```text
app facade
-> socket state + backend type erasure
-> OS socket backend | self-hosted protocol/data plane
-> netif/driver
```

POSIX fd/errno 是 socket 行为的兼容投影，不反向定义内部 packet stack。device/driver 层负责 link 与
frame ingress，不承担 socket policy；reactor 只负责 readiness progression，不实现协议状态机。

## 未冻结内容

早期设计没有证明完整 TCP/IP、IPv6、socket options、动态路由、产品安全、零拷贝或跨平台 ABI。
公开/半公开/内部模块清单、typed session 路线和具体 API 示例均是阶段方案，不再作为扩展依据。

当前能力只能由源码、CMake 和当次 Host/QEMU/real-board 证据分别证明。
