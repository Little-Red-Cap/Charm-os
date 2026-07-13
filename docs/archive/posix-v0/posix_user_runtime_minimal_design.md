# POSIX User Runtime 早期设计摘要

状态：archive。

当前边界见
[`../../system/posix_user_runtime_contract.md`](../../system/posix_user_runtime_contract.md)。本文只保留
user runtime 与 newlib bridge v0 的设计取舍，不描述现行能力。

## 保留的取舍

- `posix.user_runtime` 从测试私有 `ProgramEnv` 中提取活动 `posix::Api` 转发面。
- `posix.user_context` 保存 `argc/argv/envp`；`ProcService::start_image()` 负责入口前绑定和返回后恢复。
- `ProcessBinding<Api>` 将 process enter/exit 与 runtime binding 配对，支持同地址空间内的嵌套调用。
- `posix.user_crt` 提供最小 exit、environment 和 errno 入口，但不拥有 fd、process、loader 或完整 CRT 语义。
- `posix.user_crt_c` 只提供 C ABI 薄壳；newlib bridge 必须建立在 user runtime 上，不直接依赖
  `posix.api` 内部对象布局。
- ARM/Thumb 版本使用模块静态绑定存储，未提供 task-local、跨核或地址空间隔离。
- libc/newlib errno 与 Charm 内部错误值的转换属于 bridge 边界，不能把内部常量直接暴露给 C 程序。

## v0 行为选择

- `_read/_write/_lseek/_fstat/_isatty` 使用 libc 约定的返回值和 errno；有效 non-tty 的
  `_isatty` 返回 `0`，不把它当作错误。
- pipe 采用 eager nonblocking 行为：live writer 下空读返回 `EAGAIN`，live reader 下满写返回
  `EAGAIN`，writer 全部关闭后读返回 EOF。
- `O_NONBLOCK` 与 `O_APPEND` 作为 open-file-description 状态由 dup 别名共享；这不承诺阻塞调度。
- `_fstat/_stat` 只固定最小 file-type 与 size 外观；目录 size 取 `0`。
- `_access` 只按 mode 权限位判断，不引入 uid、gid 或 ACL。
- `_rmdir` 只删除空目录；`_kill` 只覆盖最小信号集；`_lseek` 只支持 regular file。

这些选择后来由 POSIX/newlib smoke 扩展并修正。当前返回值、errno 和支持面必须重新检查源码与
当次测试，不能从本文推断。
