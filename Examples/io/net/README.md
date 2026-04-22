# 网络示例入口

本目录收纳网络协议栈、reactor、socket bridge 与诊断链路相关的 smoke 示例。这里的组织方式基本是“一条语义链路一个目录”。

如果你还没先看网络设计文档，建议先回到：

- [`../../../docs/io/README.md`](../../../docs/io/README.md)
- [`../../../docs/io/net_stack_dual_surface_design.md`](../../../docs/io/net_stack_dual_surface_design.md)

## 按任务进入

### 我想看网络底座与接线

先看：

- [`api_facade_smoke/`](api_facade_smoke/)
- [`netif_smoke/`](netif_smoke/)
- [`net_driver_smoke/`](net_driver_smoke/)
- [`net_pump_smoke/`](net_pump_smoke/)
- [`net_stack_ingress_smoke/`](net_stack_ingress_smoke/)

### 我想看 ARP / IPv4 / ICMP

先看：

- [`net_arp_smoke/`](net_arp_smoke/)
- [`net_ipv4_smoke/`](net_ipv4_smoke/)
- [`net_icmp_smoke/`](net_icmp_smoke/)
- [`net_icmp_protocol_smoke/`](net_icmp_protocol_smoke/)
- [`net_icmp_roundtrip_smoke/`](net_icmp_roundtrip_smoke/)

### 我想看 forwarding / route 语义

先看：

- [`net_lab_smoke/`](net_lab_smoke/)
- [`net_lab_trace_smoke/`](net_lab_trace_smoke/)
- [`net_lab_forward_trace_smoke/`](net_lab_forward_trace_smoke/)
- [`net_lab_route_default_precedence_smoke/`](net_lab_route_default_precedence_smoke/)
- [`net_lab_route_precedence_smoke/`](net_lab_route_precedence_smoke/)
- [`net_lab_route_metric_smoke/`](net_lab_route_metric_smoke/)
- [`net_lab_route_table_mutation_smoke/`](net_lab_route_table_mutation_smoke/)
- [`net_lab_route_delete_smoke/`](net_lab_route_delete_smoke/)
- [`net_lab_route_precise_delete_smoke/`](net_lab_route_precise_delete_smoke/)
- [`net_lab_route_introspection_smoke/`](net_lab_route_introspection_smoke/)
- [`net_lab_udp_diag_forward_smoke/`](net_lab_udp_diag_forward_smoke/)
- [`net_lab_udp_diag_forward_failure_smoke/`](net_lab_udp_diag_forward_failure_smoke/)
- [`net_lab_udp_diag_forward_late_reply_smoke/`](net_lab_udp_diag_forward_late_reply_smoke/)
- [`net_lab_udp_diag_forward_route_churn_smoke/`](net_lab_udp_diag_forward_route_churn_smoke/)
- [`net_lab_udp_diag_forward_default_precedence_smoke/`](net_lab_udp_diag_forward_default_precedence_smoke/)
- [`net_lab_udp_diag_forward_proxy_arp_churn_smoke/`](net_lab_udp_diag_forward_proxy_arp_churn_smoke/)

### 我想看 UDP / 诊断链路

先看：

- [`net_udp_smoke/`](net_udp_smoke/)
- [`net_udp_egress_smoke/`](net_udp_egress_smoke/)
- [`net_udp_diag_smoke/`](net_udp_diag_smoke/)
- [`net_udp_diag_roundtrip_smoke/`](net_udp_diag_roundtrip_smoke/)
- [`net_udp_diag_client_smoke/`](net_udp_diag_client_smoke/)
- [`net_udp_service_codec_smoke/`](net_udp_service_codec_smoke/)
- [`net_lab_udp_diag_forward_smoke/`](net_lab_udp_diag_forward_smoke/)
- [`net_lab_udp_diag_forward_failure_smoke/`](net_lab_udp_diag_forward_failure_smoke/)
- [`net_lab_udp_diag_forward_late_reply_smoke/`](net_lab_udp_diag_forward_late_reply_smoke/)
- [`net_lab_udp_diag_forward_route_churn_smoke/`](net_lab_udp_diag_forward_route_churn_smoke/)
- [`net_lab_udp_diag_forward_default_precedence_smoke/`](net_lab_udp_diag_forward_default_precedence_smoke/)
- [`net_lab_udp_diag_forward_proxy_arp_churn_smoke/`](net_lab_udp_diag_forward_proxy_arp_churn_smoke/)

### 我想看 reactor / service / socket 行为

先看：

- [`reactor_loopback_smoke/`](reactor_loopback_smoke/)
- [`reactor_request_echo_smoke/`](reactor_request_echo_smoke/)
- [`reactor_service_echo_smoke/`](reactor_service_echo_smoke/)
- [`reactor_service_typed_smoke/`](reactor_service_typed_smoke/)
- [`socket_contract_smoke/`](socket_contract_smoke/)
- [`posix_socket_bridge_smoke/`](posix_socket_bridge_smoke/)
- [`win_loopback_smoke/`](win_loopback_smoke/)

### 我想看配套数据结构 / codec

先看：

- [`packet_pool_smoke/`](packet_pool_smoke/)
- [`schema_codec_smoke/`](schema_codec_smoke/)

## 使用提醒

- 这里的大多数目录都是最小 smoke，不默认各自再维护一份长 README。
- 如果你改的是协议栈契约，除了示例目录，也应同步回看上位文档和对应 contract。
