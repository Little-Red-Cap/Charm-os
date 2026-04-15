# POSIX v0 收口清单

这份清单用于回答一个很实际的问题：

> Charm 当前这轮 POSIX 子系统，什么时候可以“告一段落”？

这里的“告一段落”不是“POSIX 已经做完”，而是：

- 当前阶段目标已经闭环；
- 继续扩展的收益开始低于维护成本；
- 后续工作应转入“按真实用户态阻塞点驱动”的增量模式。

---

## 1. 当前阶段的定位

本阶段目标不是 full Linux parity，也不是完整 POSIX 覆盖。

本阶段更准确的名字应当是：

- `POSIX v0`
- `BusyBox Phase 1/2 已稳 + Phase 3 minimum`
- `same-address-space userland minimum`

它要解决的是：

- 应用层可以像 hosted 程序一样，从 `main()` 进入；
- 最小文件/目录/stdio/pipe/process-control 语义已经可用；
- BusyBox 风格用户态可以作为真实验收样本持续回归；
- 公共 C 面、newlib bridge、shell/BusyBox smoke 三条表面不再明显漂移。

---

## 2. 可以宣布“POSIX v0 告一段落”的硬条件

以下条件全部满足时，可以认为这一阶段应当正式收口。

### 2.1 BusyBox Phase 1 / 2 保持稳定

至少满足：

- `docs/system/posix_busybox_phase_checklist.md` 中 Phase 1 当前验收项长期稳定；
- Phase 2 的 shell / redirect / pipe 当前验收项长期稳定；
- `docs/system/posix_stage_summary.md` 中的主线结论仍成立；
- QEMU 主线回归继续保持：
  - `posix smoke + busybox phase2 smoke`

这条线已经基本成立；它是收口的地板，不是天花板。

### 2.2 Phase 3 minimum 闭环

至少满足下面这一组最小可见语义：

- `getpid`
- `sleep`
- `kill(SIGTERM/SIGINT/SIGKILL)`
- `waitpid`
- 最小 `ps(pid/state/name)`
- shell / busybox 的 `kill` / `sleep` / `ps` 路径稳定

重点不是功能多，而是：

- 行为稳定；
- wait status 可解释；
- shell / BusyBox / API / real-ELF 四个表面不互相打架。

如果这一组都稳定，就可以认为 “BusyBox Phase 3 minimum” 已闭环。

### 2.3 公开 C 面已经足够稳定

至少要求：

- `charm_posix_user_crt.h`
- `charm_posix_user_fs.h`

这两层对外表面已经具备：

- 最小 errno 常量集合；
- 路径/目录/fd/stdio 的稳定最小语义；
- 成功路径 errno preservation；
- 关键失败路径有 smoke 固化；
- 不再频繁出现“实现已变、头文件契约未同步”的情况。

换句话说，公共 C 面应当进入“补洞很少、主要维护”的状态。

### 2.4 newlib/syscall bridge 不再是主阻塞

至少要求当前 v0 范围内常见缺口已经收住，例如：

- `_read`
- `_write`
- `_isatty`
- `_fstat`
- `_lseek`
- `_getpid`
- `_kill`

不要求完整 libc 世界都通，但要求：

- 当前最常见的 nosys 噪声已经明显下降；
- 真实 C 用户态的最小样本可以稳定工作；
- bridge 语义与公开 C 面契约基本对齐。

### 2.5 恢复至少一次完整主链链接验收

这是收口前必须补上的一条。

当前阶段虽然对象级 `ninja` 重编可持续推进，但如果要正式宣布 `POSIX v0` 收口，仍应满足至少一条：

- 相关 full link blocker 已修复；
- 或 full link blocker 被正式隔离并明确“不属于 POSIX 子系统责任面”。

也就是说：

- 可以在推进阶段接受 object-level validation；
- 但在“阶段收口”时，不能永远停留在 object-only。

---

## 3. 到了什么状态，就不该继续无限扩张

出现下面的信号时，说明 POSIX 这轮应当收口，而不是继续无边界扩展：

- 新增工作主要是在补非常细碎、低频、没有真实用户态阻塞的语义角落；
- 新增 smoke 大多只是把已经稳定的行为换一种写法再测一遍；
- BusyBox Phase 1/2/3 minimum 已经不再被现实使用卡住；
- 新需求主要来自更高层应用，而不是 POSIX 骨架本身；
- 继续推进开始明显挤压 CMake、toolchain、loader、runtime 等更上游/更横向的工作。

一旦进入这个区间，策略应当切换为：

- POSIX 不再作为“主线扩张工程”；
- 改为“真实程序阻塞 -> 最小补齐 -> 契约落文档/落 smoke”。

---

## 4. 明确不属于本阶段收口前必须完成的内容

以下内容不应成为 `POSIX v0` 的卡点：

- 完整 Linux syscall 兼容
- `fork`
- 完整 signal model
- process group / session / job control
- 动态链接
- 完整 `/proc`
- 完整权限模型 / 用户模型
- 完整 `truncate` / `lstat` / 稀有 flag / 冷门 applet 行为矩阵
- 追求“BusyBox 所有 applet 都能跑”

这些都应被视为：

- 后续阶段；
- 或需求驱动型增量；
- 而不是当前阶段的出关条件。

---

## 5. 建议的正式收口口径

当下列三句话都能成立时，就可以正式宣布：

### 口径 A

> BusyBox Phase 1 / 2 已稳，Phase 3 minimum 已闭环。

### 口径 B

> 公开 C 面与 newlib bridge 的最小契约已经稳定，并有 smoke 支撑。

### 口径 C

> 完整主链链接/验收已恢复，POSIX 不再依赖“仅对象级验证”维持信心。

满足这三条后，建议把仓库内的表述改为：

- `POSIX v0 closed`
- `POSIX enters maintenance mode`
- `future POSIX work is demand-driven`

---

## 6. 收口后的工作方式

POSIX v0 收口后，建议采用下面的规则：

- 没有真实阻塞，就不扩接口；
- 没有契约漂移，就不补重复 smoke；
- 新增兼容工作必须绑定：
  - 一个真实程序/用户态阻塞点；
  - 一条明确契约；
  - 一条最小 smoke；
  - 一段阶段文档更新。

这样可以保证 POSIX 子系统后续继续成长，但不会重新变成无边界吞噬精力的长期黑洞。

---

## 7. 当前判断

结合当前仓库状态，我的判断是：

- Phase 1 / 2 实际上已经很接近“稳定完成”；
- Phase 3 minimum 已经不是骨架缺失，而更像收尾与验收问题；
- 当前最影响“正式收口”的，不是 POSIX 语义本身，而是：
  - full link blocker
  - 以及 Phase 3 minimum 的最后一段用户态闭环整理

因此，当前最合理的目标不是“继续无限补 POSIX”，而是：

> 把 POSIX 推到 `v0 收口线`，然后主动降为维护态。

