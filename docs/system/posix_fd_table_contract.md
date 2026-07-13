# POSIX FD Table 契约

状态：supporting contract。

`FdTable<MaxFds>` 是固定容量 descriptor 表。它保存 `FdEntry` 视图并通过 `FdOps` 调用具体
file、pipe、term、device、proc 或 socket backend；它本身不实现这些 backend。

## Entry

Entry 保存 descriptor metadata、non-owning backend context/ops 和 inheritable 状态。Backend 需要
共享 open-file-description 时，必须通过 context 与 dup/close hook 管理引用；缺失 op 的上层错误由
调用该操作的 facade 决定。

## Table 行为

- `attach(entry)` 选择最低空闲 fd；满时返回 `buffer_overflow`。
- `attach(entry, desired)` 要求 desired 有效且空闲；占用时返回 `exist`。
- `get(fd)` 与 `close(fd)` 对无效/已关闭 fd 返回 `noent`。
- `close(fd)` 先调用 backend close；close 失败时保留 slot。
- `dup(from, min_fd)` 选择不小于 min_fd 的最低空闲 slot。
- `dup2(from, to)` 在需要时先关闭 to；`from == to` 成功且不变。
- dup/dup2 创建的新 descriptor 总是 `inheritable=true`。
- backend 提供 dup hook 时，复制 entry 前必须先成功增加 backend 引用。

## Clone 与清理

- `clone_to()` 只克隆 inheritable descriptor。
- `clone_all_to()` 克隆全部 descriptor。
- clone 中途失败会关闭已经复制到目标表的 entries。
- `close_non_inheritable()` 显式关闭标记项。
- `close_all()` 尝试 backend close，忽略 close 错误并清空全部 slots。
- `clear()` 只清空 slots，不调用 backend close；它只适合尚未持有资源的初始化/重置点。
- snapshot 是 entry/used 数组的浅复制，不转移 backend 所有权。

spawn 当前使用 `clone_all_to()`，在 stdio 与 file actions 完成后再执行
`close_non_inheritable()`；具体顺序见 [`posix_spawn_contract.md`](posix_spawn_contract.md)。

## 错误边界

FdTable 返回 `util::Errc`，不直接设置 errno。POSIX façade 必须按操作上下文映射：无效 fd、
attach 容量和 pipe 容量不是同一种 errno。映射规则见
[`posix_error_semantics.md`](posix_error_semantics.md)。

本契约不承诺线程安全、跨核同步、自动 readiness 或 backend 生命周期所有权。验证位于
`Examples/posix/tests/posix.fd_table.tests.cppm`。
