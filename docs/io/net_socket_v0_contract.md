# 网络 Socket v0 契约

## 文档状态

- `status`: `supporting`
- `scope`: IPv4 socket、backend 可观察行为与 POSIX fd 投影
- `source`: [`net.socket`](../../Modules/io/net/net.socket.cppm)、
  [`net.backend.stub`](../../Modules/io/net/net.backend.stub.cppm)、
  [`net.backend.win`](../../Modules/io/net/net.backend.win.cppm)、
  [`net.posix`](../../Modules/io/net/net.posix.cppm)

本契约不承诺完整 Linux socket 兼容。

## 地址与端点

v0 只支持 IPv4。

| 输入 | 结果 |
|---|---|
| `AddressFamily::unspecified` | `invalid_arg` |
| `AddressFamily::ipv6` | `not_supported` |
| `bind(ipv4_any(), 0)` | 允许，由 backend 分配临时端口 |
| `connect()` / `send_to()` 的 unspecified、IPv6、零端口或 any 地址 | 拒绝 |

`connect()` 和 `send_to()` 的目标必须是具体 IPv4 对端。

## 状态与操作

| 操作 | 前置状态或类型 |
|---|---|
| `bind()` | `opened` |
| `connect()` | `opened` 或 `bound` |
| `listen()` | `bound` TCP socket |
| `accept()` | TCP listener |
| `send()` / `recv()` | `connected` |
| `send_to()` / `recv_from()` | UDP socket |

UDP `send()` 使用 `connect()` 设置的默认对端；未连接时返回 `bad_state`。状态或 socket 类型不满足
操作前置条件时返回 `bad_state` 或 `not_supported`，具体分类由对应操作契约固定。

## Backend 对齐

stub 与 Windows backend 对相同输入必须给出同类可观察结果：

- 非法端点返回同类参数错误；
- 状态违例返回 `bad_state`；
- UDP/TCP 能力缺失返回 `not_supported` 或 `bad_state`；
- `bind(ipv4_xxx(0))` 成功并物化临时端口。

平台句柄、系统调用和临时端口分配机制不进入 socket 调用面。

## POSIX fd 投影

- `dup()` / `dup2()` 创建同一底层 socket 的另一个 fd；最后一个 fd 关闭时才释放 socket。
- `fstat(socket_fd)` 返回 `S_IFSOCK`；无效或已关闭 fd 返回 `EBADF`。
- TCP 对端正常关闭后，`read()` / `recv()` 返回 `0`。
- `spawn()` 的 stdio/file actions 可复制带 `FD_CLOEXEC` 的 socket fd；目标 fd 保留，原 source fd
  在子执行面关闭。

## 不支持

- IPv6；
- 完整 socket option、flag 和 nonblocking 语义；
- 完整 `getsockname()` / `getpeername()` 查询；
- 完整 errno 兼容。

## 验证

基础 socket 与 fd 投影分别由
[`socket_contract_smoke`](../../Examples/io/net/socket_contract_smoke/main.cpp) 和
[`posix_socket_bridge_smoke`](../../Examples/io/net/posix_socket_bridge_smoke/main.cpp) 验证；
loopback 与 reactor 证据从 [`Examples/io/net`](../../Examples/io/net/README.md) 进入。
