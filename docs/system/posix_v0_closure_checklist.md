# POSIX v0 收口清单

这份清单用于回答一个很实际的问题：

> Charm 当前这一轮 POSIX 子系统，什么时候可以正式告一段落？

这里的“告一段落”不是“POSIX 已经做完”，而是：

- 当前阶段目标已经闭环；
- 继续无边界扩展的收益开始低于维护成本；
- 后续工作应切换到“由真实样例和真实阻塞点驱动的增量模式”。

---

## 当前结论

结合当前仓库状态，可以把这一轮工作明确表述为：

- `POSIX v0` 已满足收口条件；
- POSIX 子系统进入维护模式；
- 后续 POSIX 工作改为按真实阻塞点增量推进，而不是继续主线式铺功能。

这一定义与当前已经恢复的主线 QEMU 回归、newlib bridge 验证、BusyBox Phase 1/2 稳定度，以及 Phase 3 minimum 的落地状态一致。

---

## 1. 当前阶段到底在收什么

当前阶段的目标不是 full Linux parity，也不是完整 POSIX 覆盖。

更准确的名字是：

- `POSIX v0`
- `BusyBox Phase 1/2 已稳 + Phase 3 minimum`
- `same-address-space userland minimum`

它要解决的是：

- 应用层可以像 hosted 程序一样，从 `main()` 进入；
- 最小文件、目录、stdio、pipe、进程控制语义已经可用；
- BusyBox 风格用户态可以作为真实样例持续回归；
- 公共 C surface、newlib bridge、shell/BusyBox smoke 三条表面不再明显漂移。

---

## 2. 收口判定标准

只有当下面这些条件同时成立，才应宣布 `POSIX v0` 收口。

### 2.1 BusyBox Phase 1 / 2 持续稳定

至少满足：

- `docs/system/posix_busybox_phase_checklist.md` 中当前 Phase 1 的验收项稳定成立；
- shell / redirect / pipe 等 Phase 2 最小链路持续稳定；
- `docs/system/posix_stage_summary.md` 中记录的主线判断依然成立；
- QEMU 主线 smoke 维持绿色：`posix smoke + busybox phase2 smoke`。

### 2.2 Phase 3 minimum 已经闭环

至少具备并稳定验证：

- `getpid`
- `sleep`
- `kill(SIGTERM / SIGINT / SIGKILL)`
- `waitpid`
- 最小 `ps(pid/state/name)`
- shell / busybox 的 `kill` / `sleep` / `ps` 路径稳定

重点不在功能多，而在于：

- 行为稳定；
- wait status 可解释；
- API / shell / BusyBox / real-ELF 四个表面不互相打架。

### 2.3 公共 C surface 已进入稳定态

当前至少应包含并稳定：

- `Modules/io/posix/charm_posix_user_crt.h`
- `Modules/io/posix/charm_posix_user_fs.h`

这里要求的不是“接口很多”，而是：

- 最小 errno 常量集合已稳定；
- fd / stdio / path / dir 的最小语义已固定；
- success-side `errno` preservation 有 smoke 约束；
- 关键失败路径也有最小 smoke 固化；
- 不再频繁出现“实现已变、头文件契约未同步”的漂移。

### 2.4 newlib/syscall bridge 不再是主阻塞点

至少当前 v0 范围内的常见桥接缺口已经收住，例如：

- `_read`
- `_write`
- `_isatty`
- `_fstat`
- `_lseek`
- `_getpid`
- `_kill`

这里不要求完整 libc 世界都打通，但要求：

- 常见 `nosys` 噪声明显下降；
- 最小真实 C 用户态样例能够稳定运行；
- bridge 行为与公共 C surface 的契约一致。

### 2.5 完整主链验证已经恢复

在推进阶段，可以暂时使用 object-level rebuild 保持节奏。

但要宣布 `POSIX v0` 收口，仍应满足至少一条完整主链验证恢复：

- 主线 QEMU smoke 已恢复并持续可用；
- 相关 full link blocker 已修复，或已被正式隔离并明确不属于 POSIX 子系统责任面。

当前这一条已经满足：主线 `posix-qemu-demo` 与专项 `posix-qemu-newlib-stdio` 都已重新回到稳定验证路径。

---

## 3. 哪些东西不属于 v0 收口条件

下面这些内容不应成为 `POSIX v0` 的卡点：

- 完整 Linux syscall 兼容
- `fork`
- 完整 signal model
- process group / session / job control
- 动态链接
- 完整 `/proc`
- 完整权限模型 / 用户模型
- 完整 `truncate` / `lstat` / 冷门 flag / 冷门 applet 行为矩阵
- “BusyBox 所有 applet 都能跑”

这些都应视为：

- 后续阶段能力；
- 或需求驱动型增量；
- 而不是当前阶段的出关条件。

---

## 4. 收口后的工作方式

`POSIX v0` 收口后，建议采用下面的节奏：

- 没有真实阻塞点，就不主动扩接口；
- 没有契约漂移，就不重复补等价 smoke；
- 每个新兼容项都必须绑定：
  - 一个真实程序或真实用户态阻塞点；
  - 一条明确契约；
  - 一条最小 smoke；
  - 一段同步文档更新。

也就是说，后续 POSIX 不再作为“无限扩张的主线工程”，而是作为稳定底座进入维护模式。

---

## 5. 当前判断依据

当前可以宣布收口，依据是：

- BusyBox Phase 1 / 2 的主干链路已经稳定；
- Phase 3 minimum 已经从“骨架缺失”转为“最小能力已成形并被回归覆盖”；
- 公共 C surface 与 newlib bridge 已进入可维护状态；
- 主线 QEMU smoke 与专项 newlib stdio smoke 均已恢复；
- 当前继续投入更多 POSIX 扩展，边际收益已经明显低于上游构建、工具链、运行时整理等工作的收益。

因此，仓库内对当前阶段的推荐表述应为：

- `POSIX v0 closed`
- `POSIX enters maintenance mode`
- `future POSIX work is demand-driven`
