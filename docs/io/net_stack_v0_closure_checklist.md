# 网络底座 v0 关单清单

这份清单用于回答一个很实际的问题：

> Charm 当前这一轮网络底座，什么时候可以正式告一段落？

这里的“告一段落”不是“网络已经做完”，而是：

- 当前阶段目标已经闭环；
- 继续无边界扩展底座的收益开始低于维护成本；
- 后续工作应切换到“由真实数据面阻塞点驱动的增量模式”。

---

## 当前结论

结合当前仓库状态，可以把这一轮工作明确表述为：

- `网络底座 v0` 已满足关单条件；
- 网络子系统不再以“继续开放式收口 façade / bridge / reactor 骨架”为主线；
- 下一阶段应明确切到“自研数据面的最小闭环推进”，同时保留一组小而硬的回归面做底座防漂移。

这一定义与当前已经完成的 façade 收敛、POSIX bridge 语义钉牢、reactor / request / service / typed service contract smoke，以及首轮关单回归批次和 armv7a 构建检查一致。

---

## 1. 当前阶段到底在收什么

当前阶段的目标不是 full TCP/IP，也不是完整 Linux socket 兼容。

更准确的名字是：

- `网络底座 v0`
- `typed façade + raw socket` 双表面已站稳
- `socket / POSIX / reactor / request / service` 最小公共 I/O 骨架已成形

它要解决的是：

- 普通用户已经可以通过 `TcpClient / TcpListener / UdpSocket` 使用网络能力；
- 高级能力仍可落回统一 socket abstraction，而不是另起一套系统；
- 网络已经并入 POSIX fd 与 `io.reactor` 主线，而不是成为旁路系统；
- request / service / typed service 已有可复用承载骨架；
- host / WinProvider / armv7a 构建路径已经足够支撑后续演进。

---

## 2. 关单判定标准

只有当下面这些条件同时成立，才应宣布 `网络底座 v0` 关单。

### 2.1 对外 façade 已进入稳定可用态

至少满足：

- `TcpClient / TcpListener / UdpSocket` 的常见路径已经收敛到少量高频动作；
- `connected / listening / bound` 及其 `*_loopback / *_any` 便捷入口已经成为推荐写法；
- `Examples/io/net` 的主流用户路径已经大面积切到新风格；
- 普通调用路径不再被迫接触 backend / provider / driver / packet pool 等内部概念。

### 2.2 回归证据已经形成最小闭环

至少当前已经稳定验证：

- `net-api-facade-smoke`
- `net-posix-socket-bridge-smoke`
- `net-pump-smoke`
- `net-reactor-request-close-smoke`
- `net-reactor-close-drain-win-smoke`
- `net-reactor-write-reset-close-smoke`
- `net-reactor-service-close-win-smoke`
- `net-reactor-service-typed-request-error-win-smoke`

重点不在于“回归列表越大越好”，而在于：

- 既覆盖用户入口，也覆盖系统桥接；
- 既覆盖快乐路径，也覆盖 `close / reset / error / request / service / typed` 代表面；
- 回归批次足够小，后续可以持续重复执行。

### 2.3 承载面契约不再明显漂移

至少当前已经稳定：

- socket 与 POSIX fd bridge 的最小 contract；
- reactor 对 `readable / writable / accepted / closed / error` 的观察语义；
- request / service / typed service 的 `close / reset / error` 收口语义；
- `schema_codec / service_codec` 作为 typed payload contract 的快速回归面。

### 2.4 跨目标构建卫生已重新站稳

这里不要求当前就拥有大规模 ARM 运行时网络回归。

但要宣布 `网络底座 v0` 关单，至少应满足：

- host 路径的主回归不再被网络底座级构建问题反复打断；
- armv7a / QEMU 方向至少能稳定参与网络相关构建检查；
- 最近一轮网络 / POSIX 收口没有重新污染跨目标构建卫生。

当前这一条已经满足：首轮关单回归期间已补做 armv7a 最小构建检查，root 配置成功且 `Charm-runtime` 成功构建。

---

## 3. 哪些东西不属于 v0 关单条件

下面这些内容不应成为 `网络底座 v0` 的卡点：

- 完整自研 TCP/IP 协议栈；
- 完整 Linux/POSIX socket 兼容面；
- IPv6 / DHCP / DNS / TLS；
- netif / route / packet pool 的用户面开放；
- 重量级上层协议全面铺开；
- ARM 路径上的大规模运行时网络矩阵。

这些都应视为：

- 后续阶段能力；
- 或真实需求驱动的增量；
- 而不是当前阶段的出关条件。

---

## 4. 关单后的工作方式

`网络底座 v0` 关单后，建议采用下面的节奏：

- 没有真实阻塞点，就不继续开放式扩 façade；
- 没有 contract 漂移，就不无限扩回归表；
- 后续网络工作默认绑定：
  - 一个真实数据面阻塞点；
  - 一个最小缺口契约；
  - 一条最小 smoke；
  - 一段同步文档更新。

也就是说，后续网络不再作为“继续打磨底座表面”的主线工程，而是作为稳定底座进入维护式回归，并把主精力转向自研数据面推进。

---

## 5. 当前判断依据

当前可以宣布关单，依据是：

- `Examples/io/net` 中主流用户路径已经大面积完成 façade 收敛；
- socket / POSIX / reactor / request / service / typed service 的关键 contract 已由 smoke 矩阵压实；
- 首轮“小而硬”的关单回归批次已经跑通；
- armv7a / QEMU 方向最小构建检查已经恢复并通过；
- 关单回归还顺手暴露并修掉了 Windows CRT 宏污染 `S_IF*` / `SEEK_*` 常量名的真实阻塞点，说明当前回归面不仅能证明稳定，也能继续发现底座级卫生问题；
- 当前继续投入更多 façade / bridge 扩面，边际收益已经明显低于转向数据面最小闭环的收益。

因此，仓库内对当前阶段的推荐表述应为：

- `网络底座 v0 closed`
- `网络底座进入维护式回归`
- `后续网络工作以自研数据面最小闭环推进为主`
