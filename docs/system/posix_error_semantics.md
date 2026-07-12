# POSIX 错误映射契约

状态：supporting contract。

源码入口：`Modules/io/posix/posix.errno.cppm`。

## 基本规则

- 内部 API 返回 `util::Errc`。
- POSIX façade 在失败时选择对应 mapper、设置 errno，并返回 POSIX 形状。
- `from_errno()` 是有损回转，不能恢复原始错误上下文。
- 成功路径是否保留 errno 由具体 façade 契约决定，不能从 `to_errno(ok)` 反推。

## 通用映射

`to_errno()` 当前直接覆盖常见错误，包括：

| `util::Errc` | errno |
|---|---|
| `perm` | `EPERM` |
| `noent` | `ENOENT` |
| `again` | `EAGAIN` |
| `nomem` | `ENOMEM` |
| `busy` | `EBUSY` |
| `exist` | `EEXIST` |
| `notdir` | `ENOTDIR` |
| `isdir` | `EISDIR` |
| `inval` | `EINVAL` |
| `rofs` | `EROFS` |
| `nametoolong` | `ENAMETOOLONG` |
| `nosys` | `ENOSYS` |
| `notsup` | `ENOTSUP` |
| `timeout` | `ETIMEDOUT` |

`closed` 与 `buffer_overflow` 在通用 mapper 中都退化为 `EIO`。需要更具体含义的调用点必须
使用上下文 mapper，不能先调用通用 mapper 再猜测。

## 上下文映射

- `to_fd_errno(noent|notsup)` -> `EBADF`
- `to_fd_attach_errno(buffer_overflow)` -> `EMFILE`
- `to_pipe_errno(buffer_overflow)` -> `ENOSPC`
- `to_pipe_errno(closed)` -> `EPIPE`

当前没有通用 `ENFILE` mapper。文档或 wrapper 不得仅凭“系统资源满”声称会返回 `ENFILE`；
必须由具体实现显式建立该映射。

## errno 存储

- ARM/Thumb 默认 errno storage 是模块内静态值。
- Host 默认使用 `thread_local` storage。
- 活动 `ExecContext` 存在时，用户 runtime 在调用前后同步其独立 `errno_value`。

因此 errno 不是跨 task/跨核自动隔离的通用设施。需要新的执行域时，必须明确绑定和保存策略。

验证入口包括 `Examples/posix/tests/posix.errno.tests.cppm` 及各 façade 的错误 smoke。旧映射表
与阶段性错误约定保留在 [`../archive/posix-v0/`](../archive/posix-v0/README.md)。
