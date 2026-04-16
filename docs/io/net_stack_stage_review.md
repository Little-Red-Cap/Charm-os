# 网络协议栈阶段复盘（底座已立，先收口再上协议）

## 这份复盘回答什么

这份文档用于回答三件事：

- 当前这阶段到底做成了什么
- 为什么这阶段的推进是健康的
- 下一阶段为什么应该先收口底座，而不是立刻扩协议面

它是 `docs/io/net_stack_dual_surface_design.md` 的阶段性落地回看，不替代设计文档本身。

---

## 当前阶段已经落地的东西

### 1. 对外方向已经钉住

网络栈当前不是在追求“功能看起来很多”，而是在先把对外稳定名词和高频动作钉住：

- `Stack`
- `Endpoint`
- `TcpClient`
- `TcpListener`
- `UdpSocket`
- `NetEvent`

也就是说，网络栈已经明确采用“对外极简、对内可演进”的双表面方向，而不是一开始就把 backend、driver、packet pool 等内部概念暴露给用户。

### 2. 统一导出入口已经形成

`Modules/io/charm.net.cppm` 已经把当前网络相关能力收拢到统一入口，至少说明这套东西已经不是零散实验代码，而是准备被系统性消费的模块集合。

当前已进入统一导出面的能力包括：

- `net.common`
- `net.socket`
- `net.stack`
- `net.api`
- `net.reactor`
- `net.reactor_driver`
- `net.line_session`
- `net.frame_session`
- `net.request_session`
- `net.service_session`
- `net.schema_codec`
- `net.service_codec`

### 3. 原始 socket 面和 typed session 面都已经站住

当前网络层不是只有一个“能发包的 socket 壳”，而是已经同时形成了两层能力：

- 原始 socket / endpoint / stack 抽象
- 更贴近协议和服务编排的 typed session / codec 抽象

这很重要，因为它意味着后续无论挂 HTTP、MQTT，还是私有 request/reply 协议，都不需要重新发明一套新的承载骨架。

### 4. POSIX fd 世界已经开始接入并收口网络能力

`Modules/io/net/net.posix.cppm` 已经开始把 socket 能力投影到 POSIX fd 体系，`Modules/io/posix/posix.api.cppm` 也已经接入 `SocketService`。

这是本阶段最值钱的落点之一：  
我们没有把网络能力做成“系统里另一套平行 I/O”，而是主动并回了 `fd_table` 这条主脊柱。

只要这条路继续走通，后续：

- `dup/close/fstat`
- socket 与 pipe/file/term 的统一 fd 行为
- `spawn` 场景里的继承与清理语义

都会有机会落在同一套系统模型里，而不是各自长歪。

而且这条线现在已经不只是“接上了”，而是开始有了明确 contract：

- socket duplicate fd 共享同一底层句柄
- 最后一个 fd 关闭前不会提前释放底层 socket
- `fstat(socket_fd) -> S_IFSOCK`
- TCP 对端关闭后，`read()/recv()` 走 EOF
- `spawn / stdio / file_actions` 现在也能正确处理 cloexec socket fd 作为复制源的场景

### 5. smoke 已经不是单点，而是形成了矩阵

本阶段的验证已经不止一个 demo，而是形成了多条回归路径：

- `Examples/io/net/win_loopback_smoke`
- `Examples/io/net/posix_socket_bridge_smoke`
- `Examples/io/net/reactor_loopback_smoke`
- `Examples/io/net/reactor_line_echo_smoke`
- `Examples/io/net/reactor_frame_echo_smoke`
- `Examples/io/net/reactor_request_echo_smoke`
- `Examples/io/net/reactor_service_echo_smoke`
- `Examples/io/net/reactor_service_deferred_smoke`
- `Examples/io/net/reactor_service_typed_smoke`
- `Examples/io/net/reactor_listener_close_smoke`
- `Examples/io/net/schema_codec_smoke`
- `Examples/io/net/net_udp_egress_smoke`
- `Examples/io/net/net_pump_smoke`
- `Examples/io/net/net_api_facade_smoke`

这说明我们不是只写了一套“设计上看起来优雅”的层次，而是已经让这些层次有了可执行、可观察、可回归的验证面。

### 6. façade 收敛已经从“方向对”进入“示例层默认写法”

如果说前一阶段更多是在证明 `TcpClient / TcpListener / UdpSocket` 这条 façade 路线“值得做”，
那么到当前这个检查点（2026-04-17），我们已经更接近在证明它“正在成为默认写法”：

- `Examples/io/net` 里的主流用户路径已经大面积切到 `connected / listening / bound` 及其 `*_loopback / *_any` 便捷入口
- 一批 `WinProvider` 侧的 listener / request / service / typed service 示例也已经跟着收敛，不再只是 stub 路径先变干净
- 旧式 `listener.listen(...loopback...)`、`client.connect(...loopback...)`、`SocketChannelBinding{client.raw()}` 这类更偏“底层味道”的写法，已经基本退出示例层主视野

这件事的意义不只是“代码更顺眼”，而是说明网络对外表面已经开始从设计意图，变成真实的用户调用默认面。

---

## 为什么这阶段是健康的

### 1. 先做结构正确，而不是先追协议数量

这阶段最对的地方，是没有一上来就追“做出完整 TCP/IP 功能表”，而是先把：

- 稳定名词
- 分层边界
- smoke 入口
- POSIX / reactor 接缝

这些真正决定长期演进成本的地方钉住。

这符合 Charm 一贯的工程取向：  
先把骨架做对，再让功能自然长出来。

### 2. 对外简洁、对内复杂的原则已经落到了代码形态

你前面讲的原则——“内部可以复杂，外部尽量简单”——这次不是停留在口头上，而是已经进入了设计与实现：

- 对外先固定少量稳定对象与动作
- 对内允许 backend、reactor、session、codec、service 分层并行演进

这比“先把所有东西揉成一个能用 API，后面再重构”要健康得多。

### 3. 没有让网络能力脱离现有系统主线

本阶段没有把网络栈做成孤岛，而是主动对齐了两个已有主线：

- `io.reactor`
- POSIX / fd table

这意味着网络栈不是未来某个“特殊子系统”，而是正在成为 Charm 通用 I/O 世界的一部分。

这一步做得越早，后面越不容易积累系统级技术债。

---

## 这阶段最值钱的几个判断

### 1. 先立双表面，再做具体协议

先确定 `typed facade + raw socket` 的双表面，是正确的。

这样做有几个直接收益：

- 普通用户不会被内部概念淹没
- 高级用法仍然能落回统一 socket engine
- 上层协议作者不会被迫绕开框架自己造轮子

### 2. 先立 typed service/session 骨架，再谈上层协议

当前已经有 `request_session` / `service_session` / `schema_codec` / `service_codec`，这意味着上层协议未来可以长在统一承载面上。

这比直接写一个“HTTP v0 试试看”更有长期价值，因为它沉淀的是公共骨架，而不是单个协议的偶然实现。

### 3. 先把 socket 并进 POSIX，而不是后面再补桥

如果网络能力最开始就绕开 POSIX / fd 体系，后面再补桥通常会非常痛苦。

这次选择在早期就把桥接点立住，是非常好的判断。

---

## 当前暴露出的风险

### 1. ARM / QEMU 路径的模块边界问题已经收住，但仍要持续警惕

当前 host 路径和 ARM/QEMU 路径都已经能稳定构建，前面围绕 `std::span` / module 暴露边界的阻塞也已经在 `net.common`、`net.posix`、`net.stack` 这一层做过收口。

这说明问题不是不可控的；但它仍然提醒我们一件事：  
网络模块继续扩面时，仍要持续注意标准库视图类型、re-export 边界和跨目标 toolchain 差异，避免同类构建卫生问题回潮。

### 2. `posix.api` 正在成为集成热点

这次 merge 冲突落在 `Modules/io/posix/posix.api.cppm` 不是偶然，它提示我们：

- POSIX 对外门面正在承接越来越多系统汇流
- 如果后面继续把更多跨域逻辑直接压进这里，协作冲突和维护成本会继续上涨

换句话说，`posix.api` 现在已经开始提醒我们“应该继续做组织层面的减压”。

### 3. 对外 API 体验方向已明确，但仍应克制冻结表面

目前方向已经清楚，而且示例层已经大面积收敛；但这还不等于：

- 用户调用面已经最终定型
- facade 命名已经完全冻结
- raw socket 与 typed facade 的边界已经被完整 smoke 锁住

所以现在更适合把它当成“关单前的收尾管理问题”，而不是重新发散 API 设计；同时也不适合太早把外部承诺做重。

### 4. 当前强项是 host 验证，弱项是跨目标一致性

当前优势在于：

- host backend 快速回归
- POSIX bridge smoke 可观察
- reactor / service 相关例子已经成形
- ARM/QEMU 路径已经能重新参与构建回归

当前短板在于：

- ARM/QEMU 目前更多还是“可构建、可解释”，不是“网络路径已大规模运行回归”
- 跨 backend 一致性仍然要继续用 contract + smoke 去钉，而不能只靠感觉认为已经对齐

---

## 下一阶段为什么先收口底座

下一阶段最合理的目标，不是立刻扩成“大协议栈”，而是把当前底座收口到以下状态：

- host 路径稳定可回归
- ARM 路径可构建、可解释
- POSIX fd bridge 语义继续锁定
- reactor / channel / service 承载面更清晰
- 对外 facade 开始从“方向正确”收敛到“调用简单”

从 2026-04-17 这个检查点看，最后这一条已经基本进入尾声：  
当前更需要的是把关单证据整理好，而不是继续反复摇摆 façade 形状。

原因很简单：

- 现在继续加协议，收益会被底层不稳定稀释
- 现在先收口底座，后面每长一个协议都能复用同一套骨架

也就是说，下一阶段应该优先沉淀“可复用结构资产”，而不是堆“协议数量”。

---

## 现阶段明确暂不做什么

以下内容当前都不应进入优先级前列：

- 完整 Linux socket 兼容面
- 完整 IPv6 / DHCP / DNS / TLS
- 把 netif / route / packet pool 直接暴露给普通用户
- 为了 API 看起来漂亮而引入额外动态分配、阻塞等待或失控后台线程
- 在底座尚未收口前就推进重量级上层协议

---

## 一句话结论

这阶段最核心的成果，不是“网络协议栈已经做完了”，而是：

> **Charm 的网络底座已经从想法变成了一条真实可推进、可验证、可并入系统主线的骨架。**

下一步最值得做的，不是着急铺更多协议，而是把这条骨架收口、压实、锁契约。
