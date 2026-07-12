# POSIX User Runtime 契约

状态：supporting contract。

User Runtime 是同地址空间 POSIX 程序到活动 `posix::Api`/process context 的转发表。它不拥有
fd、filesystem、process 或 loader，也不是完整 libc/CRT。

源码入口：

- `Modules/io/posix/posix.user_runtime.cppm`
- `Modules/io/posix/posix.user_context.cppm`
- `Modules/io/posix/posix.user_crt.cppm`
- `Modules/io/posix/posix.user_crt_c.cppm`
- `Modules/io/posix/charm_posix_user_crt.h`
- `Modules/io/posix/charm_posix_user_fs.h`

## Runtime 转发表

`posix::user::Runtime` 保存不拥有的 ctx 与一组 C-style function pointers。当前表面覆盖：

- fd/file/dir：read、write、open、close、stat、dup、fcntl、pipe、cwd 等
- process：spawn、spawnp、waitpid、kill、list、getpid、sleep
- path mutation：mkdir、unlink、rmdir、rename

`make_runtime(api)` 从具体 `Api` 构造转发表。wrapper 调用前读取 `active_runtime()`；多数缺失
runtime/operation 的路径返回失败 sentinel 并设置 `ENOSYS`。具体 sentinel 由每个 wrapper 定义，
不能假设所有函数都返回 `-1`。

wrapper 在调用前后同步活动 `ExecContext::errno_value`，使同一执行上下文中的 C/C++ façade
观察一致 errno。映射边界见 [`posix_error_semantics.md`](posix_error_semantics.md)。

## Process 绑定

`ProcessBinding<Api>` 持有不拥有的 `Api*` 与对应 Runtime。通过
`bind_process_runtime()` 接入 `ProcService` hooks 后：

- process enter：`api.push_process(pid)`，再绑定 Runtime
- process exit：解绑 Runtime，再 `api.pop_process()`

Runtime binding 与 startup context 都支持嵌套恢复。当前保存栈固定为 32 层；超过容量不会
返回错误，因此调用方必须保证嵌套深度不溢出。

Host 存储使用 `thread_local`；ARM/Thumb 使用模块静态存储。后者不提供 task-local 或跨核隔离，
只适合当前串行 same-address-space 执行模型。

## Startup 与 CRT

`StartupContext` 暴露当前 argc、argv、envp，并提供 argv/env 查询。它由
`ProcService::start_image()` 在调用程序入口前绑定，退出时恢复。

`posix.user_crt` 提供 environ、getenv、errno location 和 exit 入口。`exit/_exit/abort` 依赖活动
`ExecContext` 完成非局部退出；没有活动 context 时会停在无限循环，因此不得作为任意系统上下文
中的通用终止 API。

`posix.user_crt_c` 与两个公开头文件提供薄 C ABI。C ABI 只转发 Runtime/CRT 行为，不建立第二套
fd、path 或 process 语义。

## 生命周期

- Runtime 不拥有 ctx，绑定期间 `Api` 必须存活。
- ProcessBinding 不拥有 ProcService，hooks 与 binding 的生命周期必须由装配方协调。
- argv/envp 指针只在当前程序入口的 startup context 内有效。
- active runtime/startup/exec context 必须按栈顺序解绑，不支持跨线程迁移。

## 不提供的能力

- 完整 CRT、sysroot 或动态链接
- 独立地址空间、TLS runtime 或 task-local errno 的通用保证
- 自动线程/跨核上下文传播
- fd、filesystem、process 或 loader 的真实实现
- 无界嵌套与可恢复的 binding overflow

验证分布在 `Examples/posix/tests/` 以及 `Examples/kernel/posix/qemu/` 的 C/newlib smoke。
早期设计和 bridge 阶段记录见 [`../archive/posix-v0/`](../archive/posix-v0/README.md)。
