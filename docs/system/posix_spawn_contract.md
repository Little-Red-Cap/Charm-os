# POSIX Spawn 契约

状态：supporting contract。

本文件描述 `SpawnConfig`、`ProcService::spawn()` 与子 fd 视图的当前行为，不承诺 `fork()`、
并行进程调度或完整 `posix_spawn` 兼容。

源码入口：

- `Modules/io/posix/posix.proc_types.cppm`
- `Modules/io/posix/posix.proc.cppm`
- `Modules/io/posix/posix.spawn_fds.cppm`
- `Modules/io/posix/posix.image_resolver.cppm`

## 输入

`SpawnConfig` 当前包含：

- `path`、`argv`、`envp` 与 `cwd`
- 固定上限为 16 的 `FileActions`
- `stdio_in`、`stdio_out`、`stdio_err`
- `PathMode::exact` 或 `PathMode::search_path`

`FileAction` 只支持 `open`、`close` 和 `dup2`。配置既没有 path 又没有 argv 时，spawn 返回
`invalid_arg`。

## 子 fd 视图

`build_spawn_fd_table()` 当前顺序是：

1. 从 parent table 克隆全部 descriptor。
2. 应用三个 stdio override。
3. 按数组顺序应用 file actions。
4. 关闭最终标记为 non-inheritable 的 descriptor。

因此 file action 可以覆盖 stdio；`open` 的相对路径按 `cfg.cwd` 解析，未给 cwd 时使用 `/`。
任何步骤失败都会关闭已建立的 child table，并把 `util::Errc` 返回给调用方。

## 程序执行

`ProcService::spawn()` 当前执行：

```text
build child fd table
-> resolve ProgramImage
-> allocate process slot
-> copy cwd/name
-> build argv/envp
-> bind ExecContext and StartupContext
-> invoke program entry
-> close child fd table
-> retain exited/signaled result for waitpid
```

当前 `start_image()` 在 spawn 调用链内同步执行入口。`SpawnResult` 返回 pid 不代表后台 task
已并行启动；程序结果保存在 process slot 中，随后由 `waitpid()` 读取并释放 slot。

`waitpid()` 找不到 pid 时返回 `noent`；目标尚未结束时返回 `would_block`。wait options 当前没有形成
完整 POSIX 行为。

## 容量与生命周期

- process、fd、argv/envp、name 和 path 都受 `ProcService` 模板容量约束。
- path、argv/envp 与 file-action path 是调用期视图；spawn 在进入程序前复制所需字符串。
- child fd table 在启动失败或程序入口结束后立即回收；`waitpid()` 只读取结果并释放 process slot。
- ProgramImage 与 entry ABI 由
  [`posix_program_image_contract.md`](posix_program_image_contract.md) 约束。

## 不提供的语义

- `fork()` / `vfork()` / `execve()` 替换当前进程
- 独立地址空间或抢占调度
- 完整 wait option、process group、session 或 signal model
- shell command-string 解析
- 无上限 argv/envp、fd、file action 或 process 数量

验证主要位于 `Examples/posix/tests/posix.proc.tests.cppm` 与
`Examples/posix/tests/posix.programs.exec.tests.cppm`。早期设计稿见
[`../archive/posix-v0/`](../archive/posix-v0/README.md)。
