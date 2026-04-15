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
- socket / stack / endpoint / event 等基础抽象已存在
- reactor / session / codec / service 分层已落地
- `net.posix` 已开始把 socket 投影到 POSIX fd 体系
- `posix_socket_bridge_smoke` 已可作为一条直接回归路径
- host 侧 smoke 已经形成矩阵
- `net.pump` 已开始把 `ARP / IPv4 / UDP ingress/egress` 收口到统一推进面，`net_pump_smoke` 可覆盖最小闭环
- `reactor_listener_close_smoke` 已锁住 watched listener 的本地关闭会向 reactor 收口为 `closed`
- ARM / QEMU 路径当前已恢复稳定构建，之前围绕 `std::span` / module 边界的阻塞已在 `net.common`、`net.posix`、`net.stack` 这一层收住

---

## 当前进度判断

- `M1`：已完成最小收口；host 与 ARM/QEMU 路径都已能稳定构建，跨模块标准库视图边界已做过一轮压实
- `M2`：已完成 v0 契约锁定；`stub / win` backend、`Socket` 状态机与 contract smoke 已对齐
- `M3`：已完成第一阶段收口；socket fd 的 `dup / close / fstat / EOF / spawn` 最小语义已经落地并有 smoke 支撑
- `M4 ~ M6`：仍是下一阶段主任务；reactor 承载面、typed service 骨架与对外 facade 体验还需要继续收敛

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

- `reactor_request_echo_smoke` 稳定通过
- `reactor_service_echo_smoke`、`reactor_service_deferred_smoke`、`reactor_service_typed_smoke` 稳定通过
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

## 一句话执行口径

当前阶段的执行口径可以压缩成一句话：

> **先把网络底座做成 Charm 主线里稳定、可回归、可承载上层协议的公共 I/O 能力，再谈协议铺开。**
