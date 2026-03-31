# Linux 生态兼容任务清单（MCU 目标）

本清单以“单片机可运行 Linux 用户态软件”为目标，按依赖与收益排序。
原则：先跑 BusyBox Phase 2/3，再扩展到更完整的 POSIX 语义。

## 目标分级
- 档 1：源码级移植（可改代码）
- 档 2：接口级兼容（不改逻辑，POSIX 层可编译）
- 档 3：用户态环境接近 Linux（BusyBox/sh + 常用工具链可跑）

## 里程碑与验收

### 里程碑 A：最小闭环（Phase 2 基础）
- 进程：`spawn` + `waitpid`（无 fork）
- FD：`fd_table`、`dup/dup2/close`
- 管道：`pipe` EOF/EPIPE
- 文件：`open/read/write/stat/fstat`
- 终端：`stdin/stdout/stderr` 绑定、`isatty`
- 错误：`errno` 映射与 thread_local

验收：
- BusyBox Phase 2 核心命令集
- QEMU smoke：stdout 可观测、失败可定位

### 里程碑 B：用户态可用性提升（Phase 3）
- `kill` 最小子集（SIGTERM/SIGKILL/SIGINT）
- `sleep/usleep/nanosleep`
- `getpid`、最小 `ps`
- `PATH` 搜索、`envp` 传递

验收：
- BusyBox Phase 3 命令集
- 关键 applet 通过

### 里程碑 C：生态扩展
- `/dev/null`、`/dev/console`、`/tmp`
- `/proc` 最小子集（`/proc/self`、`/proc/<pid>`）
- `stat/lstat` 字段补齐、权限/只读语义
- 设备与伪终端（后续）

验收：
- toybox 小集
- 选定 Linux 程序样本可跑

## 模块清单（按依赖顺序）

| 模块 | 关键能力 | 依赖 |
| --- | --- | --- |
| posix.fd_table | fd 生命周期与继承 | 无 |
| posix.pipe | 管道语义 | fd_table |
| posix.proc | spawn/waitpid | fd_table |
| posix.term | stdio/isatty | fd_table |
| posix.file | open/read/write/stat | VFS |
| posix.env | PATH/envp | 无 |
| posix.errno | errno 映射 | util::Errc |
| posix.api | POSIX wrapper | 上述全部 |

## 详细执行清单（模块/接口/测试）

### A1. posix.fd_table
- 目标能力：attach/get/close/dup2/clone
- 关键语义：继承、dup2 覆盖、EMFILE/ENFILE 区分
- 产出：`FdEntry`/`FdOps`/`FdTable`
- 验收：单测 + QEMU smoke

### A2. posix.pipe
- 目标能力：pipe_create/read/write
- 关键语义：EOF/EPIPE
- 产出：`PipeService` + pipe 端点 FD
- 验收：单测 + QEMU smoke

### A3. posix.proc
- 目标能力：spawn/waitpid + 子 fd 表隔离
- 关键语义：父表不变、file_actions 生效顺序
- 产出：`SpawnConfig`/`WaitStatus`
- 验收：单测 + QEMU smoke

### A4. posix.term
- 目标能力：stdio 绑定、isatty
- 关键语义：term 与 file/pipe 区分
- 产出：`posix.term` + stdio init
- 验收：QEMU smoke

### A5. posix.file
- 目标能力：open/read/write/stat
- 关键语义：/dev/null、基础 flags
- 产出：`FileService` + VFS 对接
- 验收：QEMU smoke + BusyBox Phase 2

### A6. posix.errno
- 目标能力：to_errno 为主、from_errno 有限回转
- 关键语义：线程局部 errno
- 产出：errno 映射表
- 验收：单测

### A7. posix.env
- 目标能力：PATH 搜索、envp 解析
- 关键语义：统一入口，避免 shell/posix 双逻辑
- 产出：`envp_get/for_each_path_candidate`
- 验收：spawn PATH 用例

### A8. posix.api
- 目标能力：POSIX wrapper（open/close/read/write/dup2/pipe/spawn/waitpid）
- 关键语义：errno 统一设置；可绑定当前进程 fd 表
- 产出：`posix::Api`
- 验收：QEMU smoke + BusyBox Phase 2

## 执行表（默认优先级顺序）

| 优先级 | 模块 | 任务 | 负责人 | 状态 | 验收 | 备注 |
| --- | --- | --- | --- | --- | --- | --- |
| P0 | posix.fd_table | attach/get/close/dup2/clone 语义收敛 | TBD | TODO | QEMU smoke | EMFILE/ENFILE 语义明确 |
| P0 | posix.pipe | EOF/EPIPE 语义完善 | TBD | TODO | QEMU smoke | 阻塞/非阻塞先不做 |
| P0 | posix.proc | spawn/waitpid + 子 fd 表隔离 | TBD | TODO | QEMU smoke | file_actions 顺序固定 |
| P0 | posix.term | stdio 绑定 + isatty | TBD | TODO | QEMU smoke | term vs file/pipe |
| P0 | posix.file | open/read/write/stat 基础 | TBD | TODO | BusyBox Phase 2 | /dev/null 已有 |
| P0 | posix.errno | to_errno 主路径 | TBD | TODO | 单测 | from_errno 有限回转 |
| P1 | posix.env | PATH 搜索/统一入口 | TBD | TODO | spawn PATH 用例 | 先不做 shell 双逻辑 |
| P1 | posix.api | wrapper + errno 设置 | TBD | TODO | QEMU smoke | 支持 bind child fd table |
| P2 | phase2 smoke | BusyBox 最小集固化 | TBD | TODO | run_qemu_ci | 可观测输出 |

当前建议：先补 P0 全部，P1 先做 posix.env。

## P0 细化拆分（建议落地顺序）

### P0.1 posix.fd_table
- 接口：attach/get/close/dup2/clone_to
- 语义：EMFILE/ENFILE 区分、dup2 覆盖、继承标志
- 验收：`posix.fd_table.tests` + QEMU smoke

### P0.2 posix.pipe
- 接口：pipe_create/read/write
- 语义：读端 EOF、写端 EPIPE
- 验收：`posix.pipe.tests` + QEMU smoke

### P0.3 posix.proc
- 接口：spawn/waitpid + 子 fd 表隔离
- 语义：file_actions 顺序、父表不变
- 验收：`posix.proc.tests` + QEMU smoke

### P0.4 posix.term
- 接口：stdio 绑定、isatty
- 语义：term/file/pipe 区分
- 验收：`posix.api.tests` + QEMU smoke

### P0.5 posix.file
- 接口：open/read/write/stat
- 语义：/dev/null、基础 flags
- 验收：`posix.api.tests` + BusyBox Phase 2

### P0.6 posix.errno
- 接口：to_errno/from_errno（有限回转）
- 语义：thread_local errno
- 验收：`posix.errno.tests`
## P0 落地 TODO（缺口 -> 动作）

### P0.1 posix.fd_table
- 缺口：EMFILE/ENFILE 语义未完全约束
- 动作：补齐错误码返回规则；在 `posix.fd_table.tests` 加回归项
- 验收：QEMU smoke 通过

### P0.2 posix.pipe
- 缺口：高压/非阻塞暂不处理（保持最小）
- 动作：只保 EOF/EPIPE 行为固定；补充 pipe 关闭传播用例
- 验收：QEMU smoke 通过

### P0.3 posix.proc
- 缺口：PATH 搜索用例暂时跳过
- 动作：补齐 `resolve_name`/PATH 语义并恢复用例
- 验收：`posix.proc.tests` 全绿

### P0.4 posix.term
- 缺口：term/file/pipe 判断边界需固定
- 动作：新增 `isatty` 对 pipe/file 的 negative case
- 验收：`posix.api.tests` 全绿

### P0.5 posix.file
- 缺口：路径 normalize 与 VFS 规则需对齐
- 动作：补 `stat/fstat` 基础字段一致性；固化 /dev/null 行为
- 验收：BusyBox Phase 2 通过

### P0.6 posix.errno
- 缺口：from_errno 有限回转规则需文档化
- 动作：补测试覆盖（未知 errno/非对称回转）
- 验收：`posix.errno.tests` 全绿
## 验收脚本
- QEMU 一键验收：`Examples/kernel/posix/qemu/run_qemu_ci.ps1`
  - `./run_qemu_ci.ps1 -ElfPath ./cmake-build-debug/posix-qemu-demo.elf -TimeoutSec 8`
  - `./run_qemu_ci.ps1 -ElfPath ./cmake-build-debug/posix-qemu-demo.elf -ReportPath ./qemu-smoke.report`
  - `./run_qemu_ci.ps1 -ElfPath ./cmake-build-debug/posix-qemu-demo.elf -KeepLogs $true`

## BusyBox 验收最小集（Phase 2）
- `echo hello`
- `echo hello > out.txt`
- `cat < out.txt`
- `echo hello | cat`
- `echo hello | cat | cat`
- `sh -c 'echo hi > a.txt'`
- `sh -c 'echo hi 1> a.txt 2> b.txt'`
- `sh -c 'echo hi | cat'`

## 兼容性验证工具链
- BusyBox / toybox 交叉编译
- Linux 上 `strace` 对照 syscall
- POSIX/LTP 子集（补语义）

## 风险与约束（默认策略）
- 暂不实现 `fork`，以 `spawn` 为主
- 默认 fd 可继承，靠 `FileActions::add_close` 裁剪
- `from_errno` 保持“有限回转”，避免伪精确

## 相关文档
- `docs/system/posix_compat_roadmap.md`
- `docs/system/posix_busybox_phase_checklist.md`
- `docs/system/posix_spawn_minimal_design.md`
- `docs/system/posix_fd_table_minimal_design.md`
- `docs/system/posix_error_semantics.md`




