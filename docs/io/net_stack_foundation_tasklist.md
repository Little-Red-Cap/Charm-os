# 网络协议栈底座收口任务单（v0）

## 目标

在不提前扩大外部 API 承诺的前提下，把当前网络底座收口到下面这个状态：

- host 路径稳定可回归
- ARM / QEMU 路径可构建、可解释
- socket 与 POSIX fd bridge 语义继续钉牢
- reactor / channel / typed service 承载面可持续复用
- 对外 facade 从“方向正确”收敛到“调用简单”

这份任务单服务于当前阶段，不是完整 TCP/IP roadmap，也不是未来所有协议能力的总任务表。

---

## 当前基线

- 网络双表面设计已明确：见 `docs/io/net_stack_dual_surface_design.md`
- 统一网络入口已建立：`Modules/io/charm.net.cppm`
- `api_facade_smoke` 已锁住 `TcpClient/TcpListener/UdpSocket` 的 `connected/listening/bound` 工厂式入口，并保持与 `*_loopback/*_any` 便捷入口、`u8` 数组直传 `send/recv` 兼容
- `Examples/io/net` 中面向用户的 reactor / request / service / typed service / Win smoke 已大面积收敛到 `TcpClient::connected_loopback`、`TcpListener::listening_loopback` 与默认构造后 `bind()` 的 `SocketChannelBinding` 用法；旧式 `listener.listen(...loopback...)`、`client.connect(...loopback...)`、`SocketChannelBinding{client.raw()}` 已基本从示例层退出
- socket / stack / endpoint / event 等基础抽象已存在
- reactor / session / codec / service 分层已落地
- `net.posix` 已开始把 socket 投影到 POSIX fd 体系
- `posix_socket_bridge_smoke` 已可作为一条直接回归路径
- host 侧 smoke 已经形成矩阵
- `net.pump` 已开始把 `ARP / IPv4 / UDP ingress/egress` 收口到统一推进面，`net_pump_smoke` 可覆盖最小闭环
- `net.protocol.diagnostic_udp` 已把最小 UDP 诊断协议的服务端 / 客户端一起挂到 `UdpStackPump` 数据面上，`net_udp_diag_smoke` 可覆盖服务端 `ping / count / slow_count / meta` 回复以及 `unsupported / bad_request` 错误回复，`net_udp_diag_client_smoke` 可覆盖客户端请求发送、响应分发、错误回复、超时与取消后 stray reply 丢弃，`net_udp_diag_roundtrip_smoke` 可覆盖双端 `client -> ARP -> server -> reply` 真实往返闭环
- IPv4 / UDP 数据面已补上 limited broadcast 的 ingress/egress：接收侧可接受 `255.255.255.255` 目标地址的 IPv4/UDP 包，发送侧对 UDP broadcast 不再依赖 ARP，`net_udp_smoke` 与 `net_udp_egress_smoke` 可直接回归这条能力
- `net.icmp` 已补上最小 echo codec/service/egress，并接入 `IcmpStackPump`、`bind_icmp_protocol` 与最小 ping facade：`Stack -> Ipv4Service -> IcmpEchoService` ingress 可解析 echo request/reply，ARP 解析后可完成 ICMP echo request 发送，`net_icmp_smoke` 可回归 codec、unsupported drop、checksum 校验与 ARP 后发送成功，`net_icmp_roundtrip_smoke` 可回归 `ARP -> echo request -> echo reply` 的最小真实往返闭环，`net_icmp_protocol_smoke` 可回归 `echo::Probe` 的结果视图/状态快照调用面、`has_value()` / `identifier()` / `sequence()` / `payload_size()` / `value_payload()` 这组更贴近用户语言的结果摘要辅助、protocol binding、`echo::Client::bind()` / `echo::AutoReplyServer::bind()` 这层更顺手的调用面，并覆盖 echo client 的自动编号、请求级 reply/timeout hook、pending 清理、取消、超时与 late reply 丢弃
- `reactor_listener_close_smoke` 已锁住 accepted socket 会继承请求的 persistent events，且 watched listener 的本地关闭会向 reactor 收口为 `closed`
- `reactor_listener_win_close_smoke` 已把这条 listener / accept / watch 语义扩展到真实 WinProvider：accepted socket 会继承请求的 persistent events，且 watched listener 的本地关闭仍会向 reactor 收口为 `closed`
- `reactor_write_close_smoke` 已锁住 transport 进入终态后 sender 不再继续排队
- `reactor_write_reset_close_smoke` 已把这条语义扩展到真实 WinProvider：即便先采样到 `writable`，peer abortive close / reset 后真正 flush 时仍会统一收口为 `closed`，而不是反弹成 `error`
- `reactor_close_drain_win_smoke` 已把 close-drain 语义扩展到真实 WinProvider：peer 在发送最后一帧后正常断开时，driver 会先把尾帧交给 session，再统一收口为 `closed`
- `reactor_request_close_smoke` 已锁住 closed transport 不再接受新的 request
- `reactor_request_close_win_smoke` 已把这条语义扩展到真实 WinProvider client 侧：peer 在收到 request 后正常断开时，pending 会被清理，late request 会被拒绝，session/driver 会统一收口为 `closed`
- `reactor_request_reset_close_smoke` 已锁住 WinProvider 在 peer abortive close / reset 下仍把 transport 收口为 `closed`，而不是误报成 `error`
- `reactor_request_error_smoke` 已锁住 error transport 不再接受新的 request，且 pending 状态会被清理
- `reactor_request_error_win_smoke` 已把这条语义扩展到真实 WinProvider client 侧：peer 触发异常带外事件后，pending 会被清理，late request 会被拒绝，session/driver 会统一收口为 `error`
- `reactor_service_close_smoke` 已锁住 closed transport 后 deferred reply 会被拒绝，且 deferred 状态会被清理
- `reactor_service_close_win_smoke` 已把这条语义扩展到真实 WinProvider server 侧：peer 在送达 request 后正常断开时，deferred token 会失效，late reply 会被拒绝，session/driver 会统一收口为 `closed`
- `reactor_service_typed_close_win_smoke` 已把这条语义扩展到真实 WinProvider server 侧：peer 在送达 typed request 后正常断开时，typed deferred token 会失效，late typed reply 会被拒绝，session/driver 会统一收口为 `closed`
- `reactor_service_reset_close_smoke`、`reactor_service_typed_reset_close_smoke` 已把这条语义扩展到真实 WinProvider server 侧：peer abortive close / reset 后 deferred token 会失效，late reply 会被拒绝，session/driver 统一收口为 `closed`
- `reactor_service_error_smoke` 已锁住 error transport 后 deferred reply 会被拒绝，且 deferred 状态会被清理
- `reactor_service_error_win_smoke` 已把这条语义扩展到真实 WinProvider server 侧：peer 触发异常带外事件后，deferred token 会失效，late reply 会被拒绝，session/driver 会统一收口为 `error`
- `reactor_service_request_close_smoke` 已锁住 closed transport 不再接受新的 service request，且 pending 状态不会泄漏
- `reactor_service_request_close_win_smoke` 已把这条语义扩展到真实 WinProvider client 侧：peer 在收到 service request 后正常断开时，pending 会被清理，late request 会被拒绝，session/driver 会统一收口为 `closed`
- `reactor_service_request_reset_close_smoke`、`reactor_service_typed_request_reset_close_smoke` 已把这条语义扩展到真实 WinProvider client 侧：peer abortive close / reset 后 pending 会被清理，late request 会被拒绝，session/driver 统一收口为 `closed`
- `reactor_service_request_error_smoke` 已锁住 error transport 不再接受新的 service request，且 pending 状态不会泄漏
- `reactor_service_request_error_win_smoke` 已把这条语义扩展到真实 WinProvider client 侧：peer 触发异常带外事件后，pending 会被清理，late request 会被拒绝，session/driver 会统一收口为 `error`
- `reactor_service_typed_error_smoke` 已锁住 error transport 后 typed deferred reply 会被拒绝，且 typed deferred 状态会被清理
- `reactor_service_typed_error_win_smoke` 已把这条语义扩展到真实 WinProvider server 侧：peer 触发异常带外事件后，typed deferred token 会失效，late typed reply 会被拒绝，session/driver 会统一收口为 `error`
- `reactor_service_typed_request_close_smoke` 已锁住 closed transport 不再接受新的 typed request，且 typed pending 状态不会泄漏
- `reactor_service_typed_request_close_win_smoke` 已把这条语义扩展到真实 WinProvider client 侧：peer 在收到 typed request 后正常断开时，typed pending 会被清理，late typed request 会被拒绝，session/driver 会统一收口为 `closed`
- `reactor_service_typed_request_error_smoke` 已锁住 error transport 不再接受新的 typed request，且 typed pending 状态不会泄漏
- `reactor_service_typed_request_error_win_smoke` 已把这条语义扩展到真实 WinProvider client 侧：peer 触发异常带外事件后，typed pending 会被清理，late typed request 会被拒绝，session/driver 会统一收口为 `error`
- ARM / QEMU 路径当前已恢复稳定构建，之前围绕 `std::span` / module 边界的阻塞已在 `net.common`、`net.posix`、`net.stack` 这一层收住

---

收口判定：`docs/io/net_stack_v0_closure_checklist.md`

## 当前进度判断

- `M1`：已完成最小收口；host 与 ARM/QEMU 路径都已能稳定构建，跨模块标准库视图边界已做过一轮压实
- `M2`：已完成 v0 契约锁定；`stub / win` backend、`Socket` 状态机与 contract smoke 已对齐
- `M3`：已完成第一阶段收口；socket fd 的 `dup / close / fstat / EOF / spawn` 最小语义已经落地并有 smoke 支撑
- `M4`：主体能力已基本落地；reactor / channel / driver 的 close / reset / error / accepted 语义在 `stub / win` 两条路径上都已有 contract smoke，当前更偏向回归矩阵补齐、示例收口与文档归档
- `M5`：主体能力已基本落地；`request_session / service_session / typed service / deferred reply / schema codec` 已形成可复用骨架，当前更偏向边界钉牢、回归矩阵补齐与协议层复用入口收口
- `M6`：已满足收口条件；`TcpClient / TcpListener / UdpSocket` 的工厂式 façade 已建立，主流用户面示例与关单回归证据已经形成闭环，`网络底座 v0` 现可视为关单，后续重心切到维护式回归与自研数据面推进

---

## 里程碑与任务拆分

### M1. 构建与模块边界卫生

#### 目标

先把“能不能稳定构建”这件事做扎实，避免后续所有推进都被 modules/import 问题反复打断。

#### 任务

- 清理 `net.reactor` 在 ARM/QEMU 路径上的 imported declaration 冲突
- 检查 `net.common`、`io.channel`、`io.reactor` 的跨模块标准库暴露边界
- 明确哪些模块可以直接 re-export，哪些只能内部 import
- 在不扩大功能面的前提下，压实 CMake / toolchain 对网络模块的构建路径

#### 验收

- 网络相关目标在 host 路径可稳定全量构建
- ARM/QEMU 路径至少能稳定解释当前支持边界，最好可直接通过构建
- 同类 imported declaration 冲突不再以“随机碰撞”的方式出现

### M2. backend / socket v0 契约锁定

#### 目标

把最底层真正要稳定的 socket contract 钉牢，避免上层 reactor / service 建在滑动地板上。

#### 任务

- 锁定 `Endpoint` / `SocketKind` / `ShutdownMode` / `NetEvent` 的最小稳定含义
- 对齐 `net.backend.stub` 与 `net.backend.win` 的最小能力面
- 明确 `open/bind/connect/listen/accept/send/recv/sendto/recvfrom/shutdown/close` 的 v0 行为边界
- 明确 error/result 映射策略，避免不同 backend 下语义漂移

#### 验收

- `win_loopback_smoke` 稳定通过
- 原始 socket 基本流程在 host backend 可重复回归
- backend 差异被明确限制在实现层，而不是泄漏到外部调用面

### M3. POSIX fd bridge 收口

#### 目标

继续把 socket 真正纳入 POSIX fd 世界，而不是停留在“能挂进去”的程度。

#### 任务

- 锁定 socket fd 的创建、关闭、dup 族共享语义
- 明确 `fstat(socket_fd)`、invalid fd、closed fd 的最小 contract
- 补齐 `bind/connect/listen/accept/send/recv/sendto/recvfrom/shutdown` 的 bridge 行为覆盖
- 检查 socket fd 与现有 `FdTable` / `FdEntry` 生命周期模型的耦合是否清晰
- 明确 `spawn / stdio / file_actions` 下 socket fd 的继承、裁剪与 cloexec 复制源语义
- 只在确有需要时再扩展 status flag / nonblocking 可观察面

#### 验收

- `posix_socket_bridge_smoke` 继续保持通过
- socket fd 至少在 `close`、`dup`、`fstat`、EOF、`spawn` 基础路径上行为稳定
- socket 没有长成 POSIX 外的一套旁路对象模型

### M4. reactor / channel / driver 承载面收口

#### 目标

把网络事件语义和 `io.reactor` 之间的接缝压实，让“网络是通用 I/O 的一种”真正站住。

#### 任务

- 锁定 `NetEvent` 到 `io::Event` 的映射规则
- 明确 readable / writable / accepted / closed / error 的最小观察语义
- 收口 `SocketChannelBinding` 与 `reactor_driver` 的职责边界
- 保持 close/error 路径的可解释性，避免 service/session 上层被底层事件毛刺污染

#### 验收

- `reactor_loopback_smoke` 稳定通过
- `reactor_listener_close_smoke`、`reactor_listener_win_close_smoke` 稳定通过
- `reactor_write_close_smoke`、`reactor_write_reset_close_smoke` 稳定通过
- `reactor_close_drain_smoke`、`reactor_close_drain_win_smoke` 稳定通过
- `reactor_line_echo_smoke` 与 `reactor_frame_echo_smoke` 能持续说明高层承载面可靠
- reactor 事件含义不会因为 backend 变化而大幅漂移

### M5. typed session / service v0 收口

#### 目标

把 request/reply 这一层沉淀成真正可复用的公共骨架，而不是一组松散 demo。

#### 任务

- 锁定 `line_session`、`frame_session`、`request_session`、`service_session` 的职责边界
- 明确 request id、response token、timeout、deferred reply 的最小 contract
- 把 `schema_codec` / `service_codec` 的编码边界收紧到可复用、可 smoke 的程度
- 约束 typed service 层不要过早绑死具体业务协议

#### 验收

- `reactor_request_echo_smoke`、`reactor_request_close_smoke`、`reactor_request_close_win_smoke`、`reactor_request_reset_close_smoke`、`reactor_request_error_smoke`、`reactor_request_error_win_smoke` 稳定通过
- `reactor_service_echo_smoke`、`reactor_service_deferred_smoke`、`reactor_service_close_smoke`、`reactor_service_close_win_smoke`、`reactor_service_reset_close_smoke`、`reactor_service_error_smoke`、`reactor_service_error_win_smoke`、`reactor_service_request_close_smoke`、`reactor_service_request_close_win_smoke`、`reactor_service_request_reset_close_smoke`、`reactor_service_request_error_smoke`、`reactor_service_request_error_win_smoke`、`reactor_service_typed_smoke`、`reactor_service_typed_close_smoke`、`reactor_service_typed_close_win_smoke`、`reactor_service_typed_reset_close_smoke`、`reactor_service_typed_error_smoke`、`reactor_service_typed_error_win_smoke`、`reactor_service_typed_request_close_smoke`、`reactor_service_typed_request_close_win_smoke`、`reactor_service_typed_request_reset_close_smoke`、`reactor_service_typed_request_error_smoke`、`reactor_service_typed_request_error_win_smoke` 稳定通过
- `schema_codec_smoke` 能继续充当 typed payload contract 的快速回归面

### M6. 对外 facade 收敛

#### 目标

把“方向正确”的对外体验，继续收敛成“用户真的顺手”的调用面。

#### 任务

- 复核 `TcpClient` / `TcpListener` / `UdpSocket` 的最小动作集合
- 尽量把常见路径压缩到少量高频动词
- 避免把 backend、driver、packet pool、route 等内部概念泄漏给普通用户
- 保持 raw socket 与 typed facade 落在同一套 engine 上

#### 验收

- 普通调用路径能用少量名词和动作表达清楚
- 高级路径仍可落回统一 socket abstraction
- 对外 API 改动保持节制，不急着冻结过多表面

---

## 推荐推进顺序

### 1. 先做 M1：构建与模块边界卫生

原因：

- 这是后面所有工作的地基
- 现在不清，后面每一刀都会重复被打断

### 2. 再做 M2：backend / socket v0 契约锁定

原因：

- reactor、POSIX bridge、typed session 都建立在这里
- 底层 contract 滑动，上层 smoke 再多也会变脆

### 3. 然后做 M3：POSIX fd bridge 收口

原因：

- 这条线决定网络能力能否真正并入 Charm 的通用 I/O 主线
- 也是后续 shell、userland、系统一致性最有复利的一刀

### 4. 再做 M4 + M5：reactor / service 承载面压实

原因：

- 这是“做协议”之前最该稳定的承载面
- 稳住之后，上层协议才能真正复用，而不是重复造轮子

### 5. 最后做 M6：对外 facade 收敛

原因：

- 现在方向已经对了
- 等底座再稳一层，再收 API 体验，改动成本更低，判断也更准

---

## 当前明确暂缓

以下内容当前不进入底座收口优先级：

- 完整 Linux socket 兼容面
- IPv6 / DHCP / DNS / TLS
- netif / route / packet pool 的用户面暴露
- 重量级协议直接上车
- 为了追求“接口优雅”而引入不受控复杂性

---

## 阶段关单清单（网络底座 v0）

这份清单的目的不是增加流程，而是给当前网络阶段一个明确出口：

- 避免“示例还差一点、smoke 还差一点、文档还差一点”长期悬空
- 避免底座阶段和下一阶段的“自研数据面推进”彼此打断
- 让后续讨论“现在该继续收口，还是该开下一章”时有统一口径

### 关单口径

当下面四类条件大体满足时，可以认为“网络底座 v0”告一段落：

#### 1. 对外 façade 收口完成

- `TcpClient / TcpListener / UdpSocket` 的常见路径统一收口到少量高频动作
- `connected / listening / bound` 及其 `*_loopback / *_any` 便捷入口成为推荐写法
- 常见 `send / recv / send_to / recv_from` 已支持更顺手的 `u8` 数组直传
- 普通用户路径不再被迫接触 backend / driver / provider 等内部概念

#### 2. 回归矩阵足够稳定

- host / stub 路径上的基础 smoke 持续通过
- `WinProvider` 路径上的真实语义 smoke 持续通过
- ARM / QEMU 路径继续保持可构建、可解释
- `posix_socket_bridge_smoke` 继续保持通过
- `net_pump_smoke` 继续保持最小 `ARP / IPv4 / UDP` 闭环回归能力

#### 3. 承载面契约已钉牢

- reactor 对 `readable / writable / accepted / closed / error` 的观察语义不再反复变化
- `request / service / typed service / deferred reply` 的 `close / reset / error / timeout` 语义不再漂移
- `schema_codec / service_codec` 能继续充当 typed payload contract 的快速回归面
- 现有网络示例已经能说明“网络是 Charm 主线中的公共 I/O 能力”，而不是零散 demo

#### 4. 文档与边界已同步

- 本任务单中的阶段判断与现实进度一致
- `docs/io/net_stack_dual_surface_design.md` 与实际对外 façade 方向一致
- 当前明确暂缓项仍保持清楚，不把 `IPv6 / DHCP / DNS / TLS / 完整 Linux socket 兼容面` 混进底座关单条件

### 明确“不必等到”的事情

网络底座 v0 的关单，不需要等到下面这些工作完成：

- 完整自研 TCP/IP 协议栈
- 完整 Linux/POSIX socket 兼容面
- IPv6 / DHCP / DNS / TLS
- route / netif / packet pool 的用户面开放
- 重量级上层协议全面铺开

### 关单后的下一阶段

当网络底座 v0 关单后，下一阶段建议明确切到：

> **自研数据面的最小闭环推进。**

建议优先顺序：

- 把 `packet / driver / netif / stack` 的内部边界继续压实
- 继续完善 `ARP` 的缓存、请求、应答与生命周期
- 把 `IPv4` 的最小主路径继续打通
- 在 `UDP` 之上跑一个更贴近真实用途的最小诊断/回显协议
- 只在上述路径站稳后，再讨论更重的协议与更大的用户面

### 当前判断（截至 2026-04-17）

如果按上面的四类关单口径来判断，当前已经可以明确宣布“网络底座 v0 满足关单条件”：

- **对外 façade 收口**：这条已经非常接近满足；`Examples/io/net` 里的主流用户路径已经基本切到 `connected / listening / bound` 及其 `*_loopback / *_any` 便捷入口，旧式 loopback `listen/connect` 与直接花括号构造 `SocketChannelBinding` 的写法已基本退出示例层
- **回归矩阵稳定性**：host / stub / WinProvider 路径当前已经有较强的 contract smoke 支撑，而且首轮“小而硬”的关单回归批次已经实际跑通；如果还要更稳地关单，剩下更像是按同一口径补跑，而不是新的 API 设计工作
- **承载面契约**：`reactor / request / service / typed service / deferred reply` 的 close / reset / error 语义已经被一批真实 smoke 压实；只要后续不再出现新的 contract 漂移，就不需要继续把主要精力花在底座承载面重写上
- **文档与阶段边界**：任务单、阶段复盘与独立关单清单现在已经可以共用同一口径；是否继续推进，不再取决于“底座有没有收口”，而取决于“下一阶段要先打哪条数据面主路径”

### 已完成的关单回归批次（截至 2026-04-17）

这轮回归的目的，不是“把所有网络示例再跑一遍”，而是先固定一组足以说明底座已基本站稳的最小证据面。

#### 1. 用户入口与系统桥接

- `net-api-facade-smoke`
- `net-posix-socket-bridge-smoke`
- `net-pump-smoke`

这组三个目标分别覆盖：

- 对外 façade 的最小用户入口
- socket 并入 POSIX fd 体系后的关键桥接语义
- `ARP / IPv4 / UDP` 数据面的最小推进闭环

#### 2. close / reset / error / request / service / typed 代表面

- `net-reactor-request-close-smoke`
- `net-reactor-close-drain-win-smoke`
- `net-reactor-write-reset-close-smoke`
- `net-reactor-service-close-win-smoke`
- `net-reactor-service-typed-request-error-win-smoke`

这组目标用于证明我们不是只把“快乐路径”跑通，而是已经覆盖：

- request 侧的 `close`
- transport / driver 侧的 `close drain`
- transport 侧的 `reset -> closed`
- service / deferred reply 侧的 `close`
- typed request / client 侧的 `error`

#### 3. 跨目标构建检查

- 已补做一轮 armv7a / QEMU 方向的最小构建检查：root 配置成功，`Charm-runtime` 成功构建

这条证据的价值不在于“ARM 路径已经做了大量运行时网络回归”，而在于：

- 最近一轮网络收口没有把跨目标构建卫生重新弄脏
- `net.*` 与 `posix.*` 当前仍能继续参与非 host 路径回归

#### 4. 回归中顺手清掉的真实阻塞点

这轮回归过程中，还顺手暴露并修掉了一处真实构建阻塞：

- Windows CRT 宏污染导致 `posix.fd_table` / `posix.api` / `posix.file` / `posix.term` 中的 `S_IF*` 与 `SEEK_*` 常量名发生冲突

这说明当前回归批次不仅能证明“东西还能跑”，也确实能继续帮助我们发现底座级卫生问题。

### 关单后的建议工作方式

既然 `网络底座 v0` 已满足关单条件，后续建议按下面的节奏推进：

- 把当前这组已跑通的关单回归批次固定成后续防漂移的标准证据面；如果后面再补跑，也尽量围绕这组最小集合做增量，而不是无限扩表
- 在后续网络改动触及 `net.* / posix.* / reactor.*` 主干时，按需补跑一轮 ARM / QEMU 最小构建检查，防止跨目标卫生回潮
- 明确把下一阶段主线切到“自研数据面的最小闭环推进”，避免再次回到开放式底座收口

### 一句话关单标准

如果当前网络能力已经足够说明下面这句话成立，就可以认为底座阶段可以告一段落：

> **Charm 已经拥有一套稳定、可回归、可承载上层协议、对用户调用足够简单的网络公共 I/O 底座。**

---

## 一句话执行口径

当前阶段的执行口径可以压缩成一句话：

> **先把网络底座做成 Charm 主线里稳定、可回归、可承载上层协议的公共 I/O 能力，再谈协议铺开。**
