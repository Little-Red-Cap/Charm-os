# 网络示例入口

网络契约与分层见 [`docs/io/README.md`](../../../docs/io/README.md)、
[`net_socket_v0_contract.md`](../../../docs/io/net_socket_v0_contract.md)。

## 分组

| 目标 | 目录 |
|---|---|
| facade、netif、driver、pump、ingress | `api_facade_smoke/`、`netif_smoke/`、`net_driver_smoke/`、`net_pump_smoke/`、`net_stack_ingress_smoke/` |
| ARP / IPv4 / ICMP | `net_arp_smoke/`、`net_ipv4_smoke/`、`net_icmp*_smoke/` |
| forwarding / route | `net_lab_forward_*_smoke/`、`net_lab_route_*_smoke/` |
| UDP 与 diagnostics | `net_udp*_smoke/`、`net_lab_udp_diag_forward*_smoke/` |
| reactor / service / socket | `reactor_*_smoke/`、`socket_contract_smoke/`、`posix_socket_bridge_smoke/`、`win_loopback_smoke/` |
| packet pool / codec | `packet_pool_smoke/`、`schema_codec_smoke/` |

route/UDP 变体通过后缀表达 default precedence、metric、mutation、delete、introspection、failure、
late reply、route churn 和 proxy ARP 等场景。具体输入与断言由各目录源码维护，不在总入口复制。

## 使用规则

- 一条目录只证明对应语义 fixture，不代表完整协议栈或产品网络能力。
- 修改 socket contract、route precedence 或 packet ownership 时，同时核对专题契约与相关正反 smoke。
- Host loopback、synthetic packet 和真实 netif 是不同证据域，不能互相替代。
