# POSIX 兼容执行面总览

## 战线关系声明

- `track_kind`: `maintenance`
- `track_status`: `maintained`
- 这条线的角色：
  - 它是已收口的维护线，继续承接 blocker、回归和验证链维护。
- 它当前驱动的共享收敛面：
  - fd / pipe / spawn / ELF / errno / C surface 的维护稳定性
  - QEMU smoke 与 newlib stdio 验证链
- 它不能反向重定义的仓库公共规则：
  - 不能再被当成默认扩张前线。
  - 不能为了“更像 Linux”而重新打开无边界兼容面扩张。
  - 不能反向主导 Charm 核心语义面和共享底座设计。

## 这是什么

Charm 当前已经形成了一条可工作的 POSIX / Linux 用户态兼容执行面。

它的目标不是一步做成完整 Linux 内核，而是先让一批真实用户态程序在 Charm 上跑起来，再由真实程序的阻塞点反向暴露缺口与优先级。

---

## 当前状态

当前阶段可以明确表述为：

- `POSIX v0` 已收口；
- POSIX 子系统已进入维护模式；
- 后续工作改为按真实阻塞点增量推进，而不是继续无边界扩张。

这里的“已收口”并不表示 POSIX 已经做完，而是表示：

- 当前阶段目标已经闭环；
- BusyBox Phase 1 / 2 和 Phase 3 minimum 已形成稳定主干；
- 公共 C surface 与 newlib bridge 已经足够承托最小真实用户态样例；
- 主线 QEMU smoke 已恢复并重新成为可信基线。

---

## 当前已经具备什么

- `posix.api`：提供面向 POSIX 风格调用的最小入口，包含 `spawn` / `spawnp` 等执行路径。
- `posix.user_runtime` / `posix.user_context` / `posix.user_crt` / `posix.user_crt_c`：提供注册式用户程序运行时 façade，把活动运行时、进程绑定、`argc/argv/envp`、`getenv`、`exit/_exit` 等最小 ABI 骨架稳定下来。
- 统一 `fd_table` 链路：标准输入输出、文件、管道、终端判定已经开始复用同一条 fd 语义主链。
- `spawn / waitpid`：已经形成最小进程执行闭环，支持 stdio 绑定、file actions、cwd 继承/覆盖，以及子进程 fd 视图管理。
- `PATH` 执行语义：`spawnp` 与 shell smoke 都已经走真实的 `PATH` 搜索链路。
- BusyBox applet 入口形态：已经覆盖 `argv[0]` 与 `busybox sh -c ...` 等典型入口。
- `pipe / dup2 / redirect`：最小 shell 重定向与管道链路可用。
- ELF 装载执行：支持 registered image、`elfmem:` 与文件路径 ELF，主链为 `spawn -> load_image -> start_image`。
- 显式退出 ABI：`_exit(code)` 已通过 `ExecContext + setjmp/longjmp` 接入主链。
- errno / fd 契约：关键路径已有 smoke 固化。
- newlib bridge：最小 syscall bridge 已足以支撑当前 v0 范围内的真实 C 用户态样例。

---

## 当前验证基线

当前已经恢复并依赖下面两条验证基线：

- QEMU 主线 smoke：`posix smoke + busybox phase2 smoke`
- 专项 newlib stdio smoke：`posix-qemu-newlib-stdio.elf`

同时，仓库中已经具备一组真实样例用于持续回归，例如：

- `hello`
- `argv_dump`
- `stderr_demo`
- `exit_code`
- `cat_file`
- `write_file`
- `append_file`
- `fd_probe`
- `stat_probe`

---

## 架构位置与边界

POSIX 兼容执行面主要位于 `Modules/io/posix/*`，但职责横跨运行时的多个层次：

- 对上：向样例程序、shell、BusyBox 风格用户态提供最小兼容执行面。
- 对内：复用 `fs.vfs`、`fd_table`、`pipe`、网络抽象、QEMU bringup 等已有系统能力。
- 对外：通过 `Examples/posix/tests/*` 和 `Examples/kernel/posix/qemu/*` 持续做回归验证。

当前边界必须始终明确：

- 这是 same-address-space 的最小用户态执行模型，不是完整 Linux 进程模型。
- 当前不包含 `fork`、完整 signals、动态链接、完整权限模型、完整 `/proc`、完整 Linux 用户态语义矩阵。
- POSIX façade 只负责兼容表面，不应反向主导 Charm 核心设计。

---

## 收口后的推进原则

收口之后，POSIX 子系统按下面的方式继续演进：

- 没有真实阻塞点，就不主动扩接口；
- 没有契约变化，就不重复补等价 smoke；
- 每一项新增兼容能力，都必须绑定：
  - 一个真实程序或真实用户态阻塞点；
  - 一条明确契约；
  - 一条最小 smoke；
  - 一次同步文档更新。

这意味着后续 POSIX 工作会继续做，但方式从“主线铺面”切换为“稳定底座上的按需增量”。

---

## 推荐阅读

- 分层与演进原则：`docs/system/posix_subsystem_principles.md`
- 三层执行模型：`docs/system/posix_three_layer_contract.md`
- 用户态运行时：`docs/system/posix_user_runtime_minimal_design.md`
- 路线图：`docs/system/posix_compat_roadmap.md`
- 维护期协作：`docs/system/posix_maintenance_mode_collaboration.md`
- 阶段进度：`docs/system/posix_stage_summary.md`
- 收口判定：`docs/system/posix_v0_closure_checklist.md`
- 任务清单：`docs/system/posix_linux_compat_tasklist.md`
- 清理重构计划：`docs/system/posix_cleanup_refactor_plan.md`
- spawn 设计：`docs/system/posix_spawn_minimal_design.md`
- fd 表设计：`docs/system/posix_fd_table_minimal_design.md`
- errno 映射：`docs/system/posix_errno_mapping.md`
