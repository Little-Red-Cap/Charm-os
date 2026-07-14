# 网络示例入口

> `status`: `supporting`
>
> `scope`: network、reactor 与 socket fixture 的语义路由

网络契约与分层见 [`docs/io/README.md`](../../../docs/io/README.md)、
[`net_socket_v0_contract.md`](../../../docs/io/net_socket_v0_contract.md)。

## 覆盖范围

| 范围 | 证据重点 |
|---|---|
| facade、netif、driver、pump、ingress | ownership、packet 流向与 backpressure |
| ARP、IPv4、ICMP、UDP | 协议输入、状态与错误路径 |
| forwarding 与 route | precedence、metric、mutation、delete 与 churn |
| reactor、service 与 socket | ready dispatch、fd 投影与 loopback |
| packet pool 与 codec | 容量、layout 与边界检查 |
| diagnostics | failure、late reply、proxy ARP 与可观察字段 |

准确 fixture/target 集合、输入和断言由各子目录 CMake/source 维护，README 不复制目录 inventory。

## 使用规则

- 一条目录只证明对应语义 fixture，不代表完整协议栈或产品网络能力。
- 修改 socket contract、route precedence 或 packet ownership 时，同时核对专题契约与相关正反 smoke。
- Host loopback、synthetic packet 和真实 netif 是不同证据域，不能互相替代。
