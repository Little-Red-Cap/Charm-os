# 网络协议栈双层方案（对外极简 / 对内可演进）

本文用于定义 Charm 网络协议栈的起步方向。

核心立场只有一句话：

> **对内允许复杂，对外保持朴素。**

也就是说，网络栈内部可以为了性能、零分配、平台收敛、协议复用而做复杂分层、状态机和技巧化实现；
但最终呈现给使用者的调用方式，应尽量收敛为少数稳定名词与少数高频动作。

---

## 本文定位

本文不是完整 TCP/IP 设计，也不是后端选型决议。

本文只先回答四件事：

- 用户最终应该看到什么
- 内部应该拆成哪些层
- 哪些层必须稳定，哪些层允许激进演进
- 第一阶段先做什么，暂时不做什么

当前阶段优先级仍然遵守既有判断：

> **不急着搬完整协议栈，先把 `socket / packet / endpoint` 抽象钉住。**

---

## 设计目标

网络栈的第一目标不是“功能多”，而是“结构对”。

我们希望它同时满足：

- 对用户简单：常见场景只需要 `连接 / 监听 / 收 / 发 / 关`
- 对实现可控：保持非阻塞、零分配默认、固定容量、可在 MCU 上收敛
- 对平台解耦：底层可替换 `winsock / lwip / 自研 netstack / future backend`
- 对系统统一：后续能并入 `io.reactor`、设备模型、POSIX fd 体系
- 对协议友好：HTTP/MQTT/私有协议等后续都走同一 socket/packet 抽象

---

## 非目标

以下内容不在 v0 起步目标内：

- 一次性做完整 Linux socket 兼容
- 立即实现完整 IPv6 / DHCP / DNS / TLS
- 立即把网络驱动、链路层、传输层全部自研到底
- 在用户侧暴露 netif、route、ARP、buffer pool 等内部细节
- 因为追求“接口优雅”而引入动态分配、阻塞等待、不可控后台线程

---

## 对外：用户应该看到什么

### 1. 稳定的少数名词

无论内部怎么演进，用户侧优先固定以下几个概念：

- `Stack`：一个网络栈实例或网络执行面
- `Endpoint`：地址 + 端口
- `TcpClient`：主动连接
- `TcpListener`：被动监听
- `UdpSocket`：无连接收发
- `NetEvent`：可读 / 可写 / 关闭 / 错误

用户不应被迫直接理解：

- netif
- route table
- packet pool
- driver adapter
- backend op table
- socket provider

这些都属于框架内部概念。

### 2. 稳定的高频动作

用户侧最常见动作应收敛为：

- `connect()`
- `listen()`
- `accept()`
- `send()`
- `recv()`
- `close()`
- `poll()` 或事件回调绑定

如果一个普通用户要做“TCP 客户端连上服务器发一包数据再收结果”，
不应该接触 driver、packet、adapter、stack phase 这些内部术语。

### 3. 推荐的用户入口形状

建议把对外入口分成两层：

- **第一层：Typed Facade**
  - `TcpClient`
  - `TcpListener`
  - `UdpSocket`
- **第二层：Raw Socket**
  - 给高级用户和协议层作者使用
  - 暴露较完整的 `open/bind/connect/listen/send/recv/...` 能力

这样做的好处：

- 普通用户先用 typed façade，心智负担最低
- 高级用法仍可落到统一 socket 抽象，不会出现两套系统
- 内部实现只需要维护一套核心 socket engine

### 4. 用户侧示例（目标体验）

下面的风格是目标，不代表最终命名必须完全一致：

```cpp
import charm.net;

net::TcpClient client{};

auto st = client.connect(stack, net::Endpoint::ipv4(192, 168, 1, 10, 1883));
if (!st) {
    return;
}

client.send(as_bytes("ping"));
client.recv(buffer);
client.close();
```

UDP 也应足够直接：

```cpp
import charm.net;

net::UdpSocket udp{};
udp.bind(stack, net::Endpoint::ipv4_any(5000));
udp.recv_from(buffer, peer);
udp.send_to(peer, payload);
```

重点不是 API 名字，而是用户侧体验：

- 常见路径直接可见
- 无需先理解整个系统分层
- 默认路径简单
- 高级路径存在但不打扰普通用户

---

## 对内：允许复杂，但要分区清楚

内部建议采用“外稳内活”的双层结构。

### 1. 稳定区：对外契约层

这部分一旦对用户开放，就应尽量保持稳定：

- `net.common`
- `net.socket`
- `net.api` 或聚合入口 `charm.net`
- `Endpoint / SocketType / NetEvent / SocketResult` 等核心类型

这里的变更必须谨慎，因为它们决定用户如何理解 Charm 的网络能力。

### 2. 可演进区：内部实现层

这部分允许为了工程目标持续重构：

- `net.packet`
- `net.driver`
- `net.netif`
- `net.stack`
- `net.backend.win`
- `net.backend.lwip`
- `net.backend.self`
- `net.posix_bridge`
- `net.protocol.*`

这里允许：

- 改模块拆分
- 改状态机布局
- 改 buffer 组织方式
- 改后端接法
- 改内部调度与 pump 细节

只要不破坏对外稳定区即可。

---

## 建议的内部分层

建议在 `Modules/io/net/` 下逐步形成如下结构：

```text
Modules/io/net/
  common/        # 地址、端口、协议常量、事件、结果码
  packet/        # netbuf/packet view/固定容量池
  driver/        # MAC/PHY/host link driver 抽象
  netif/         # 网络接口生命周期、input/output 接面
  stack/         # 栈实例、接口绑定、配置、路由/解析入口
  socket/        # socket 统一抽象与 provider 接口
  api/           # TcpClient/UdpSocket/TcpListener 等用户 façade
  protocol/      # HTTP/MQTT/自定义协议等上层协议
  posix/         # 与 fd_table / socket ABI 的桥接
  tests/         # 最小契约与 host/backend 验证
```

### `net.common`

负责放置最稳定的跨层类型：

- `IpAddress`
- `Endpoint`
- `SocketKind`
- `SocketState`
- `NetEvent`
- `ShutdownMode`
- `SocketError`

要求：

- 纯值语义
- 无平台依赖
- 无动态分配
- 尽量可 `constexpr`

### `net.packet`

负责网络数据包抽象，但不暴露给普通用户作为主要入口。

建议最小对象：

- `PacketView`
- `MutPacketView`
- `PacketBuffer`
- `PacketPool`

它服务于：

- 驱动收发
- netif input/output
- backend 与 socket 引擎之间的数据搬运

普通应用不应被迫理解 packet 生命周期。

### `net.driver`

负责屏蔽链路层/平台差异。

可包括：

- MCU MAC/PHY 驱动适配
- Windows host backend 适配
- wpcap / tap / future host injection stub

要求：

- 不把平台偶然性泄漏到 socket API
- 只承担链路接入与最小能力表达

### `net.netif`

负责把“链路接入”提升为“网络接口”。

最小职责：

- up/down
- input packet
- output packet
- mtu/mac/address capability 描述
- 与 stack 的绑定关系

netif 是内部骨架概念，不应成为普通用户主入口。

### `net.stack`

负责“一个网络栈实例”的运行期收口。

建议职责：

- 维护已挂接 netif
- 管理 socket provider/backend
- 提供默认配置、接口启停、地址配置入口
- 为上层 `TcpClient/UdpSocket` 提供统一宿主

这里可以复杂，因为它是内部调度中心；
但用户不应手动组装一堆内部组件才能发第一包数据。

### `net.socket`

这是整个网络栈最关键的稳定内核。

它需要统一：

- `open`
- `bind`
- `connect`
- `listen`
- `accept`
- `send`
- `recv`
- `shutdown`
- `close`

同时允许底层 provider 替换：

- `winsock`
- `lwip`
- 自研栈

这层是“外部简洁 API”与“内部多后端实现”之间的总隔离墙。

### `net.api`

这层只做一件事：

> **把 `net.socket` 包成普通用户最容易上手的样子。**

它应提供：

- `TcpClient`
- `TcpListener`
- `UdpSocket`

可以再往后扩：

- `HttpClient`
- `DnsClient`
- `MqttClient`

但这些上层 façade 不应反向污染 `net.socket` 的核心语义。

### `net.protocol`

这层放真正的协议实现。

规则很重要：

- 协议层依赖 `net.socket` 或 `net.api`
- 协议层不直接依赖 `net.driver`
- 协议层不直接 import 平台 HAL

这样 HTTP/MQTT/私有协议都能跨后端复用。

### `net.posix`

这是中后期桥接层，不是 v0 第一优先级。

它的职责是：

- 把 `socket` 行为挂接进 POSIX fd 模型
- 与 `fd_table`、`errno`、`spawn/user_runtime` 保持一致语义

原则上不另起一套“网络专用 runtime”。

---

## 与现有 Charm 体系的关系

### 1. 与 `io.channel / io.reactor / io.registry` 的关系

网络栈不应绕开现有 IO 原则。

应该继承的基本约束：

- 非阻塞优先
- 等待由 Reactor/Kernel 负责，而不是协议层 busy-loop
- 固定容量、零分配默认
- 能力通过注册/装配进入系统，而不是隐藏全局单例

可接受的关系是：

- 某些底层链路接入通过 `io.channel` 做 host stub 或桥接
- socket 事件通过 `io.reactor` 进入系统调度
- stack/netif 通过 bringup 或设备模型被装配进系统

但不建议把“网络栈本体”简单降格成一个普通字节通道。
`channel` 适合表达字节流入口，`socket/packet/netif` 仍需保留自身语义层。

### 2. 与设备模型的关系

网络驱动与网络接口最终应能进入设备模型，而不是漂浮为独立孤岛。

建议关系：

- `driver`：设备/驱动层对象
- `netif`：设备能力在网络域的投影
- `stack`：消费 netif，向上提供 socket 能力

这样未来 USB 网卡、板载以太网、Host backend 都能用统一方式接入。

### 3. 与 POSIX 的关系

后续如果推进 POSIX `socket()`，目标不是另做一套实现，
而是把 POSIX socket 语义投影到同一套 `net.socket` / `net.stack` 能力上。

也就是说：

- 用户态 POSIX 是“投影面”
- `net.socket` 是“统一内核语义面”

---

## 网络栈的硬约束

以下约束建议从 v0 起就固定。

### 1. 默认非阻塞

无论对外 façade 还是内部 socket/provider，默认都应遵守非阻塞语义。

- 不 sleep
- 不 busy-spin
- 不做隐藏等待线程
- 等待由 Reactor/EDA/Kernel 驱动

### 2. 默认零动态分配

网络常常是最容易失控的一层，因此更要收口：

- buffer pool 固定容量
- socket table 固定容量
- netif table 固定容量
- route/arp/cache 若存在，也优先固定容量

需要动态分配时，必须是可选策略，而不是默认前提。

### 3. 用户对象轻量

`TcpClient/UdpSocket/TcpListener` 应尽量轻量。

用户对象负责：

- 表达意图
- 持有句柄/状态

大块资源应尽量归属于：

- `Stack`
- `PacketPool`
- backend/provider

这样用户就不会因为“只是想发个 TCP 包”而被迫管理复杂资源图。

### 4. 后端可替换

`winsock / lwip / 自研栈` 不能影响用户侧核心 API 形状。

如果某个后端要求额外概念，
优先把复杂性封在 backend/provider 内部，
而不是把它抛给所有用户。

### 5. 协议层不得越层

HTTP/MQTT/私有协议等不得：

- 直接依赖平台 HAL
- 直接依赖网卡驱动
- 自己实现等待循环

协议层应只站在 socket/facade 之上。

### 6. 事件接入优先走 `io.reactor`

网络等待语义不应各处自长一套循环。

v0 推荐做法是把 socket readiness 投影到 `io.reactor`：

- `SocketPoller` 负责轮询 socket provider 的 `poll()`，只做事件采样与 `reactor.notify()` 入队
- `SocketChannelBinding` 把已连接 socket 投影成 `io::Channel`，供上层协议直接走 `read/write`
- `SocketEventChannelBinding` 提供纯事件通道，适合 `TcpListener` 这类只关心 accept-ready 的对象
- 如果同一次采样里同时观察到 `readable + closed`，driver 应先把剩余可读 payload 交给 session，再上报 transport closed，避免把“最后一帧数据”误伤成 error
- `RequestSession / ServiceSession / TypedServiceSession` 在 transport close 时应清空本地 pending / deferred 状态，并统一向 error handler 上抛 `errc::closed`，不要把断链拖成 timeout
- 对 `ServiceSession / TypedServiceSession` 而言，transport close 之后旧的 deferred reply token 也应立即失效，后续 `send_deferred_response()` 应返回 `noent`，避免业务层把断链后的迟到回复误判成还能发送
- 协议驱动层可继续复用 `set_sender / feed / notify_writable` 这类 session 契约，把复杂状态机压在协议层内部，而不是散落在业务代码里
- 文本协议可先落 `LineSession`；二进制协议优先落固定长度前缀的 `FrameSession`，先把最常见的 request/response 主路径钉稳
- 在 `FrameSession` 之上，可继续收敛出 `RequestSession`：统一 `request_id / opcode / timeout / pending table`，让请求关联逻辑也留在框架内而不是散落到业务层
- 在 `RequestSession` 之上，还可继续收敛出 `ServiceSession`：统一 `opcode -> handler` 路由、`status code` 语义与同步回复缓冲，让业务层先写简单 handler，再逐步演进到更复杂协议
- 当协议开始涉及设备等待、跨任务协作或慢操作时，`ServiceSession` 还应支持延迟回复 token 与显式 cancel：业务层可以先 defer，再在稍后的调度点统一 `send_deferred_response()`，客户端也可以主动 cancel 本地 pending，避免把这些样板逻辑撒回业务代码
- 如果还想把 `payload` 编解码一并封进框架，可在 `ServiceSession` 之上继续收敛出 `TypedServiceSession`：用 `Op trait + RequestCodec/ResponseCodec` 绑定 `opcode / request / response / handler signature`，把业务代码进一步收敛成 typed request/response 的形式
- 如果 typed request/response 里大部分只是定长标量与定长字节段，还可再补一层 `net.schema_codec`：用 `WireField / SchemaCodec / WireSchemaCodec` 这类固定布局 helper，把“手写 payload codec”继续压回框架内部；业务层更多只是在声明字段顺序与字节序，而不是反复手搓 encode/decode
- 如果连 `Op trait` 这层样板也想继续压缩，还可在 `TypedServiceSession` 周边补 `ServiceOp / TrivialServiceOp / WireServiceOp` 这类定义糖：把 `opcode / request / response / codec` 的机械拼装封进模板，业务层只保留“这是哪个操作、请求结构是什么、按什么线序编码”这些真正有语义的信息
- `NetEvent::accepted` 在 reactor 投影面统一映射为 `io::Event::readable`，监听者据此调用 `accept()`
- `writable` 默认更适合按需单次 arm，避免因“长期可写”而把系统推成忙轮询

这样可以保持：

- `notify()` 仍然只负责入队，不直接跑协议回调
- 真正协议处理继续留在 `reactor.drain()` 的 task-context 中执行
- 网络协议层与现有串口/AT/XMODEM 一样，共享同一套 Reactor 驱动模型

---

## v0 最小落地建议

第一阶段不追求“能上网很多功能”，而追求“把骨架钉对”。

建议先做以下最小集合：

### 第 1 步：定义公共类型

先落：

- `net.common`
- `Endpoint`
- `IpAddress`
- `SocketKind`
- `NetEvent`
- `SocketError`

目标：统一术语，不让后续代码各自发明一套名字。

### 第 2 步：定义 socket 统一抽象

先落：

- `net.socket`
- provider/op table
- `open/bind/connect/listen/accept/send/recv/close`

目标：先确定统一语义面，再决定底层挂什么 backend。

### 第 3 步：定义对外 façade

先落：

- `TcpClient`
- `TcpListener`
- `UdpSocket`

目标：尽早验证“用户视角是否足够简单”。

### 第 4 步：先接 host backend 验证

建议最早接一个 host backend，用来快速验证：

- API 是否顺手
- 事件模型是否顺手
- socket 语义是否够统一
- demo/tests 是否容易写

host backend 可以是：

- `winsock` provider
- 或更轻的 stub backend

### 第 5 步：再推进 netif/driver/packet 细化

等用户面和 socket 语义站稳后，再继续推进：

- packet pool
- netif lifecycle
- driver adapter
- board bringup

这样可以避免一开始就陷进链路层细节而丢失整体方向。

---

## 建议的模块开放策略

建议分三类管理网络模块：

### A. 公开稳定模块

- `charm.net`（聚合入口，统一导出 `net.api / net.reactor / net.*session / typed codec helper` 等常用网络面）
- `net.api`
- `net.common`
- `net.socket`

这些模块面向使用者，应慎改。

### B. 半公开模块

- `net.stack`

这类模块主要服务 bringup、系统集成、测试与高级用户。

### C. 内部易变模块

- `net.packet`
- `net.driver`
- `net.netif`
- `net.backend.*`
- `net.posix`

这些模块默认不应成为普通用户的直接依赖点。

---

## 当前推荐路线

如果按“先把复杂度封进框架，再给用户简单入口”的原则推进，
当前最合适的路线是：

1. 先把 `Endpoint / Socket / Typed Facade` 三层关系钉住
2. 用 host backend 跑最小 TCP/UDP demo
3. 把 socket readiness 接到 `io.reactor`，先跑通最小事件驱动 loopback
4. 在 reactor 之上跑一个最小协议样例，验证协议层不必自己写等待循环
5. 再把 netif/packet/driver 细化到适合 MCU 的固定容量模型
6. 最后再推进 POSIX socket/fd 投影面

这条路线的价值在于：

- 用户入口早收敛
- 内部实现保留高自由度
- 不被某个具体 backend 绑死
- 后续无论接 `winsock`、`lwip` 还是自研，都不会推翻前面的外部体验

---

## 一句话结论

Charm 网络栈的正确起点，不是先做一个“很全的协议栈”，而是：

> **先定义一个对用户足够简单、对内部足够自由、对后端足够可替换的统一网络语义面。**
