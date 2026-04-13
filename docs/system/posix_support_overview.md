# POSIX 兼容执行面总览

## 这是什么
Charm 当前已经形成了一条可工作的 POSIX/Linux 用户态兼容执行面。
它的目标不是一步做成完整 Linux 内核，而是先让一批真实用户态程序在 Charm 上跑起来，再用程序的真实需求反向逼出架构缺口与优先级。

## 当前已经支持什么
- `posix.api`：提供面向 POSIX 风格调用的最小入口，已具备 `spawn` / `spawnp` 两条执行入口。
- `posix.user_runtime` / `posix.user_context` / `posix.user_crt` / `posix.user_crt_c`：提供注册式用户程序可见的最小运行时 facade，把活跃 API / 进程绑定、`argc/argv/envp` 启动上下文、最小 `exit/_exit/getenv` 风格入口，以及后续 C/newlib 桥接可用的稳定 C ABI 符号从测试私有注入中抽离出来。
- `fd_table` 统一链路：标准输入输出、文件、管道、终端判定都开始走同一套 fd 路径。
- `spawn / waitpid`：已经形成最小进程执行闭环，支持 `stdio` 绑定、`file_actions`、子进程 fd 表隔离。
- `PATH` 执行语义：`spawnp` 与 shell smoke 已开始真实依赖 `PATH` 去解析 `/bin/*` 命令，而不是只在 proc smoke 里验证。
- BusyBox 入口形态：program smoke 已覆盖 `argv[0]` applet 形态与 `busybox sh -c ...` 这类 `argv[1]` applet 分派。
- `pipe / dup2 / redirection`：可以支撑 `echo > out.txt`、`cat < out.txt`、`echo hi | cat` 这类基础程序路径。
- ELF 装载执行：支持 registered image、`elfmem:`、文件路径 ELF，执行主链为 `spawn -> load_image -> start_image`。
- 显式退出 ABI v0：`_exit(code)` 已通过 `ExecContext + setjmp/longjmp` 接入主链。
- errno / fd 契约：已经稳定了一批最小 ABI 契约，用于支撑真实样本和 QEMU smoke。
- 用户程序运行时绑定：已支持通过 process hook 自动绑定当前 runtime，并在进入 `main(...)` 前绑定当前 `argc/argv/envp`，用户程序不必再依赖测试私有全局环境。

## 当前架构位置
POSIX 兼容执行面位于 `Modules/io/posix/*`，但它的职责横跨 Runtime 的几个层面：
- 对上：向程序样本、shell、后续 Linux 用户态程序提供最小兼容执行面。
- 对内：复用 `fs.vfs`、`fd_table`、`pipe`、InitGraph/QEMU bringup 等已有系统能力。
- 对外验证：通过 `Examples/posix/tests/*` 与 `Examples/kernel/posix/qemu/*` 持续做 QEMU 回归。

## 当前已验证的主骨架
- QEMU 主线已能稳定通过：`posix smoke + busybox phase2 smoke`。
- 已有真实样本覆盖：`hello`、`argv_dump`、`stderr_demo`、`exit_code`、`cat_file`、`fd_probe`、`stat_probe`。
- 已成立的关键能力包括：
  - 文件路径 ELF 装载
  - `_exit(code)` 统一收束
  - `open/read/write/fstat/isatty` 的最小 hostcall/errno 路径
  - `open("/dir", O_WRONLY) -> EISDIR`
  - `open("/file/child", O_RDONLY) -> ENOTDIR`

## 当前边界与非目标
当前这条执行面仍然是 v0，必须明确它的边界：
- 这是 same-address-space 的最小用户程序执行模型，不是完整 Linux 进程模型。
- 还没有 `fork`、signals、动态链接、用户态/内核态隔离、完整 `stat`/路径错误矩阵。
- `stat_probe` 已回到主线回归，并已开始覆盖最小 `mode/类型` 语义；更完整的字段矩阵仍待继续补齐。
- `close(-1)` 已收敛到 `EBADF`，但更完整的 fd/path 错误矩阵仍未覆盖完。

## 推荐阅读
- 分层与演进原则：`docs/system/posix_subsystem_principles.md`
- 三层执行模型：`docs/system/posix_three_layer_contract.md`
- 用户态运行时：`docs/system/posix_user_runtime_minimal_design.md`
- 路线图：`docs/system/posix_compat_roadmap.md`
- 阶段进度：`docs/system/posix_stage_summary.md`
- 任务清单：`docs/system/posix_linux_compat_tasklist.md`
- 清理重构计划：`docs/system/posix_cleanup_refactor_plan.md`
- spawn 设计：`docs/system/posix_spawn_minimal_design.md`
- fd 表设计：`docs/system/posix_fd_table_minimal_design.md`
- errno 映射：`docs/system/posix_errno_mapping.md`
