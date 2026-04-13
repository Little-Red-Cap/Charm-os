# POSIX User Runtime 最小设计

## 目的

`posix.user_runtime`、`posix.user_context`、`posix.user_crt` 与 `posix.user_crt_c` 一起承接同地址空间 POSIX 用户程序看到的最小“用户态运行时”接口。

它解决的问题不是重新实现一套 `posix.api`，而是把“当前活跃的宿主 API / 进程上下文”从测试私有的临时注入方式中抽出来，收敛成正式模块。

这一步的目标是：
- 让注册式用户程序不再依赖测试私有的 `ProgramEnv`。
- 让 `spawn -> start_image -> main(argc, argv, envp)` 这条链路里，用户程序看到的是稳定的运行时入口。
- 为后续最小 user CRT / 用户程序启动包装保留统一挂接点。

## 位置

- 模块：`Modules/io/posix/posix.user_runtime.cppm`
- 模块：`Modules/io/posix/posix.user_context.cppm`
- 模块：`Modules/io/posix/posix.user_crt.cppm`
- 模块：`Modules/io/posix/posix.user_crt_c.cppm`
- 头文件：`Modules/io/posix/charm_posix_user_crt.h`

## 当前职责

- 维护当前活跃的 runtime 绑定。
- 维护当前活跃的 `argc/argv/envp` 启动上下文。
- 提供嵌套绑定栈，支撑父程序内再 `spawn` 子程序的场景。
- 提供 `ProcessBinding<Api>`，把 `push_process/pop_process` 与 runtime 绑定一起接到 `ProcService::bind_process_runtime_hooks(...)`。
- 对用户程序暴露最小 facade：
  - `read/write/open/close/pipe`
  - `fstat/isatty/lseek`
  - `spawn/spawnp/waitpid/kill/list_processes/getpid`
  - `sleep`
  - `mkdir/unlink/rmdir/rename/opendir/readdir/closedir`
- 对用户程序暴露最小 startup context facade：
  - `argc()/argv()/envp()`
  - `argv0()/arg(index)`
  - `env_entry(index)/getenv(key)`
- 对用户程序暴露最小 CRT-like facade：
  - `exit(code)/_exit(code)/abort()`
  - `environ()/getenv_cstr(key)`
  - `errno_location()`
- 对后续 C / newlib 桥接暴露最小 C ABI 壳：
  - `charm_posix_argc/argv/envp/environ/getenv`
  - `charm_posix_errno_location`
  - `charm_posix_read/write/getpid/sleep/exit/abort`

当前这组 C ABI 已有可直接包含的头文件，纯 C 样例可通过：
- `#include "charm_posix_user_crt.h"`
- 依赖 `Charm-os` 暴露的 `Modules/io/posix` 公开包含目录

## 边界

- 它不拥有 fd、pipe、file、proc 的真实语义；真实实现仍在 `posix.api` 及其下层服务里。
- 它不拥有程序装载与参数构造语义；`argv/envp` 的真实构造仍在 `posix.proc`。
- 它不试图提供完整 CRT，也不承担参数布局、堆栈布局、ELF 装载策略。
- 它当前仍服务于 same-address-space 的最小执行模型，不引入新的隔离承诺。
- 在 ARM/Thumb 裸机测试目标上，它避免使用 TLS 依赖，运行时绑定存储退回普通静态存储；这与当前执行模型是一致的。

## 当前接线方式

- 宿主侧通过 `make_runtime(api)` 生成 runtime facade。
- `ProcessBinding<Api>` 负责在进程进入时：
  - 绑定当前 `pid`
  - 绑定当前 runtime
- `ProcService::start_image(...)` 在真正调用 `main(...)` 前绑定当前 `argc/argv/envp`，并在返回后恢复上一层上下文。
- `posix.user_crt` 通过 `ExecContext` 把 `exit/_exit` 收束回当前进程退出路径，而不是依赖调用者手工返回。
- `posix.user_runtime` 在转发用户态调用时会同步当前执行上下文的 errno 视图，使注册式用户程序与 ELF hostcall 都能通过 `errno_location()` 观察一致的错误状态。
- `posix.user_crt_c` 只做薄转发，不重新定义语义；它的职责是为后续独立用户程序、C 代码或 newlib 桥接预留稳定符号名。
- 在进程退出时：
  - 解绑 runtime
  - 恢复上一层 `pid`

## 当前收益

- 样例用户程序现在直接依赖 `posix::user::*`，不再依赖测试私有全局对象。
- 用户程序现在可以通过 `posix::user::argc()/argv()/envp()/getenv(...)` 读取当前启动上下文，而不必把这套访问逻辑散落在测试 harness 或临时全局里。
- 用户程序现在也可以通过 `posix::user::exit/_exit` 显式结束当前进程，并通过 `getenv_cstr/environ/errno_location` 使用更接近 CRT 的访问形态。
- process hook 与用户态 facade 的关系被显式化，后续可以继续向最小 user CRT 演进。
- 已通过 QEMU smoke 验证自动绑定路径；用户程序不需要手动注入测试环境也能跑通最小 hello 场景。

## 下一步

- 继续把 `main` 入口包装、环境绑定与后续 libc/newlib 桥接收敛到这层。
- 视需要补充更稳定的宿主调用表定义，逐步减少样例程序对测试 harness 的隐式假设。

## newlib/syscall bridge v0

当前阶段把 `newlib/syscall bridge` 明确收敛为 QEMU/POSIX 目标上的一个窄桥面：
- bridge 必须长在 `posix.user_runtime` facade 之上，不直接打洞到 `posix.api` 内部对象结构。
- bridge 先解决最基础、最常见的 `nosys` 缺口，而不是一口气铺完整 libc 适配面。
- bridge 不扩大 `charm_posix_user_crt.h` 的职责范围；优先补实现，保持公开 C ABI 头小而稳定。
- bridge 负责处理 libc/newlib 与 Charm 内部 errno 常量数值不完全一致的问题；用户态 C `errno` 观察到的是目标 libc 约定，而不是内部实现细节。

### v0 契约

- `_read`：EOF 返回 `0`；失败返回 `-1` 并设置 `errno`。
- `_write`：成功返回写入字节数；失败返回 `-1` 并设置 `errno`。
- `_isatty`：TTY 返回 `1`；有效的 non-tty 返回 `0` 且保持 `errno` 不变；无效 fd 返回 `0` 并设置错误。
- `_fstat`：v0 至少保证类型位与 `size` 稳定；`term -> S_IFCHR`、`pipe -> S_IFIFO`、regular file -> `S_IFREG`。
- `_stat`：当前最小桥面也保证目录路径可稳定返回 `S_IFDIR`；目录 `size` 先统一收敛为 `0`。
- `_access`：当前最小桥面基于 `stat.mode` 的权限位做路径检查；`F_OK` 检查存在性，`R_OK/W_OK/X_OK` 分别按 `0444/0222/0111` 掩码收敛，不引入 uid/gid/ACL 语义。
- `_rmdir`：当前最小桥面只删除空目录；对文件路径返回 `ENOTDIR`，对非空目录返回 `ENOTEMPTY`，并明确把“删除目录”从 `unlink` 路径中分离出来。
- `_kill`：v0 仅承诺最小信号集 `SIGTERM/SIGKILL/SIGINT`，范围仅限当前最小进程表可见 pid。
- `_lseek`：v0 仅保证 regular file 上的 `SEEK_SET/SEEK_CUR/SEEK_END`；pipe/tty/dev 返回 `ESPIPE`；非法 `whence` 返回 `EINVAL`。

### 落地顺序

- 第一批：`_write`、`_read`、`_isatty`、`_fstat`
- 第二批：`_getpid`、`_kill`
- 第三批：`_lseek`

这里的顺序不是为了把实现永久拆碎，而是为了约束语义扩张顺序：先把最有价值的 I/O 桥打通，再补最小进程控制，最后再触碰 offset 语义。

### 当前已落地的最小桥面

- I/O / 进程 / offset：`_read`、`_write`、`_close`、`_open`、`_fstat`、`_stat`、`_isatty`、`_lseek`、`_getpid`、`_kill`
- 路径基础：`_unlink`、`_mkdir`、`_rmdir`、`_rename`、`_access`
- `FILE*` 级验证目前保持为独立 QEMU 烟测目标，避免把全局 stdio/newlib 状态耦合进主 POSIX 回归套件。
