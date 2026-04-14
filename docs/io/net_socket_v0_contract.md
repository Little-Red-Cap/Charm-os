# 网络 socket v0 契约

这份文档只定义当前阶段已经准备锁定的最小 socket 契约，用来约束：

- `net.socket`
- `net.backend.stub`
- `net.backend.win`
- 以及建立在它们之上的 `net.api` / POSIX bridge / reactor 承载面

它不是完整 Linux socket 兼容说明，也不是未来 TCP/IP roadmap。

---

## 1. 地址与端点边界

当前 v0 只承诺 **IPv4**。

- `AddressFamily::unspecified`：视为调用参数非法，返回 `invalid_arg`
- `AddressFamily::ipv6`：明确视为当前未支持，返回 `not_supported`
- `bind()`：接受 IPv4 端点；地址可以是 `ipv4_any()`；端口可以是 `0`
- `connect()` / `send_to()`：要求目标端点是“具体 IPv4 对端”，也就是：
  - 不是 `unspecified`
  - 不是 IPv6
  - 端口不为 `0`
  - 地址不是 `any`

---

## 2. 状态机边界

当前 v0 锁定下面这组最小状态迁移：

- `open -> bind`
- `open|bind -> connect`
- `bind -> listen`
- `listen -> accept`
- `connected -> send/recv`

额外约束：

- `bind()` 只允许在 `opened` 状态调用
- `listen()` 只允许 TCP 且只允许在 `bound` 状态调用
- `accept()` 只允许 TCP listener 调用
- `send_to()` / `recv_from()` 只允许 UDP socket 调用
- UDP 的 `send()` 语义收口为“已 connect 的默认对端发送”；未 connect 时返回 `bad_state`

---

## 3. backend 对齐口径

`net.backend.stub` 与 `net.backend.win` 在 v0 需要对齐到同一组外部可观察语义：

- 同样的非法端点输入，返回同类错误
- 同样的状态机违例，返回 `bad_state`
- 同样的 UDP/TCP 能力边界，返回 `not_supported` 或 `bad_state`
- backend 差异尽量留在实现细节，不泄漏到 `Socket` / `TcpClient` / `UdpSocket` 调用面

其中一个具体收口是：

- `bind(ipv4_xxx(0))` 在 v0 允许成功
- Windows backend 通过系统分配临时端口
- stub backend 现在也会在 bind 阶段物化一个临时端口，避免 host/stub 语义漂移

---

## 4. POSIX fd bridge 最小语义

当 socket 被投影进 POSIX fd 世界后，当前阶段锁定下面这组最小行为：

- `dup()` / `dup2()` 复制的是“同一底层 socket 句柄”的另一个 fd 观察面
- 关闭其中一个 duplicate fd，不应提前关掉底层 socket
- 只有最后一个 fd 关闭时，底层 socket 才真正释放
- `fstat(socket_fd)` 返回 `S_IFSOCK`
- `fstat(invalid_fd)` / `fstat(closed_fd)` 返回 `EBADF`
- TCP 对端正常关闭后，`read()` / `recv()` 走 EOF 语义，返回 `0`
- `spawn()` / `stdio` / `file_actions` 可以把带 `FD_CLOEXEC` 的 socket fd 作为复制源；复制出来的目标 fd 保留，原 cloexec 源 fd 在子进程执行面被裁掉

---

## 5. 当前不承诺的内容

下面这些内容当前仍然故意不进入 v0 承诺面：

- 完整 Linux socket 行为兼容
- IPv6
- `getsockname()` / `getpeername()` 风格的完整查询面
- nonblocking / flag / socket option 的完整可观察语义
- 更细的 errno 级兼容

---

## 6. 回归面

这份契约当前主要由下面几条回归路径承载：

- `Examples/io/net/socket_contract_smoke`
- `Examples/io/net/win_loopback_smoke`
- `Examples/io/net/posix_socket_bridge_smoke`
- `Examples/io/net/reactor_loopback_smoke`

执行口径很简单：

> 先把 `socket/backend` 这一层的最小共同语义钉牢，再让 POSIX bridge、reactor、typed session 持续建立在同一块稳定地板上。
