# POSIX 错误语义约定（v1）

本文件用于固定 v1 阶段的错误语义与映射边界，避免 fd/pipe/spawn
在不同模块里发明不同错误口径。

## 设计原则

- core API 只返回 `util::Errc`
- POSIX wrapper 负责设置 `errno` 并返回 POSIX 形式
- `from_errno()` 是有损回转，仅用于兼容入口

## 语义与映射表

| 场景 | core 返回 | POSIX errno（wrapper 侧） |
| --- | --- | --- |
| fd 非法/不存在/已关闭 | `util::Errc::noent` | `EBADF` |
| fd 表已满（进程） | `util::Errc::buffer_overflow` | `EMFILE` |
| fd 资源已满（系统） | `util::Errc::buffer_overflow` | `ENFILE` |
| pipe 写端无读者 | `util::Errc::closed` | `EPIPE` |
| pipe 读端且无写者 | `util::Errc::end_of_stream` | 0 字节读/EOF |
| 只读 fd 上写 | `util::Errc::perm` 或 `util::Errc::inval` | `EBADF` 或 `EACCES` |
| 目录当作文件打开 | `util::Errc::inval` | `EISDIR` |
| 组件期望目录但遇到文件 | `util::Errc::inval` | `ENOTDIR` |
| 资源池耗尽（pipe/proc 等） | `util::Errc::buffer_overflow` | `ENOSPC` |

## 说明

- `EBADF/EMFILE/ENFILE/EPIPE/EISDIR/ENOTDIR` 由 wrapper 侧映射，
  core 层不强制加入更细的 `Errc`。
- `buffer_overflow` 是“资源耗尽”的近似表达，wrapper 需按场景映射。
- 未来若引入更细的 `Errc`，本表再做对应收敛。
