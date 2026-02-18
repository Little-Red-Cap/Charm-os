# VSF TCPIP 体系映射（netdrv / socket / protocol）

目标：抽取 VSF TCP/IP 组件的结构与接口模式，为 Charm 的网络层规划提供可迁移骨架。

## 1) 目录分层（VSF）

- `component/tcpip/netdrv`
  - `vsf_netdrv.h/.c`
  - 适配网络链路与 netif
  - driver: wpcap

- `component/tcpip/socket`
  - `vsf_socket.h/.c`
  - socket 抽象，驱动层可选 lwip / winsock

- `component/tcpip/protocol`
  - http client（示例）

## 2) VSF netdrv 核心结构

关键点（`vsf_netdrv.h`）：

- `vk_netdrv_t`：网络设备对象（mac/mtu/hwtype/adapter/netlink）
- `vk_netlink_op_t`：链路层接口（init/fini/can_output/output）
- `vk_netdrv_adapter_op_t`：netif 适配接口（alloc/free/read/header/on_inputted 等）

特性：
- 支持 adapter 线程回调
- 支持 PnP 回调（netdrv 新建/删除/连接）

对 Charm 的启示：
- netdrv 类似“网卡驱动 + netif 适配”的中间层
- adapter/op 可映射为 `io/net/adapter` 与 `io/net/driver` 两层

## 3) VSF socket 抽象

关键点（`vsf_socket.h`）：

- `vk_socket_t` + `vk_socket_op_t`
  - open/close/bind/listen/connect/accept/send/recv
  - 支持 DNS 的 `gethostbyname`
- 驱动可选：lwip / win

对 Charm 的启示：
- socket 层可作为 `io/net/socket` 统一接口
- 底层可插拔 lwip / winsock / future netstack

## 4) Charm 映射建议（骨架层次）

建议结构：

- `io/net/driver`
  - netdrv（link/adapter）
  - pcap/wpcap stub

- `io/net/socket`
  - socket 接口层（open/recv/send/…）
  - driver：lwip / win

- `io/net/protocol`
  - http client / future protocols

## 5) 迁移优先级建议

1) netdrv 抽象（Link/Adapter 分层）
2) socket 接口层（lwip/win）
3) protocol 示例（http client）

备注：网络复杂度较高，建议以“参考 + 接口骨架”为主，不做全量迁移。

---

参考来源：
- `Draft/vsf/source/component/tcpip/netdrv/vsf_netdrv.h`
- `Draft/vsf/source/component/tcpip/socket/vsf_socket.h`
- `Draft/vsf/source/component/tcpip/protocol/http/client/vsf_http_client.h`
