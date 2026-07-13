# POSIX 兼容执行面

状态：supporting inventory。

本页只说明当前源码边界和验证入口。POSIX 是可选兼容执行面，不定义 Charm Core，
也不承诺完整 Linux 进程或 ABI 兼容。

## 实现入口

根 `CMakeLists.txt` 的 `CHARM_ENABLE_POSIX` 控制该执行面，source collection 由
`cmake/sources/CharmIoSources.cmake` 维护。实现与公开 C 头文件位于 `Modules/io/posix/`；具体
module/file 清单以源码和 CMake 为准，不在本文复制。

## 当前模型

- 程序通过 `ProgramImage` 与 loader 进入执行路径。
- `ProcService` 管理当前 same-address-space 进程语义。
- `Runtime` 与 `ProcessBinding` 把活动 API、进程和用户态调用绑定起来。
- ELF、ModuleX 和 registered image 可以有不同 loader，但不应各自复制进程生命周期。
- fd、stdio、argv/envp、errno 与退出状态属于兼容执行面的行为，不是平台硬件事实。

具体行为以对应模块和 smoke 为准。本页不把测试名称、文档清单或曾经通过的阶段验收
转换成永久能力声明。

## 明确边界

当前模型是同地址空间的最小用户态执行环境，不等于：

- MMU 进程隔离
- `fork()` 或完整 signal 语义
- 动态链接器
- 完整权限、session、process group 或 `/proc`
- Linux syscall/errno 的完整覆盖
- 产品级兼容认证

POSIX façade 不得反向规定 kernel、runtime 或 Charm Core 的公共语义。新增兼容行为应由
真实程序阻塞点驱动，并由最小测试固定。

## 证据入口

- 模块级与程序级测试：`Examples/posix/tests/`
- 静态 ELF 样本：[`../../Examples/posix/elf_samples/README.md`](../../Examples/posix/elf_samples/README.md)
- Cortex-M7 QEMU 系统入口：
  [`../../Examples/kernel/posix/qemu/README.md`](../../Examples/kernel/posix/qemu/README.md)

QEMU runner、token 和参数由该入口维护；是否通过必须以当次构建与运行结果判断。

## 继续阅读

- 用户态 runtime：[`posix_user_runtime_contract.md`](posix_user_runtime_contract.md)
- ProgramImage：[`posix_program_image_contract.md`](posix_program_image_contract.md)
- spawn：[`posix_spawn_contract.md`](posix_spawn_contract.md)
- fd 与错误语义：[`posix_fd_table_contract.md`](posix_fd_table_contract.md)、
  [`posix_error_semantics.md`](posix_error_semantics.md)

POSIX v0 的阶段基线、早期设计和工具链记录见
[`posix-v0`](../archive/posix-v0/README.md)。
