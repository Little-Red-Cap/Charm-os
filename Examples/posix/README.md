# POSIX 示例入口

本目录保存 ProgramImage/POSIX 的真实样本和模块测试，不替代完整 QEMU 运行入口。

## 先读

- 执行面与边界：[`posix_support_overview.md`](../../docs/system/posix_support_overview.md)
- ProgramImage：[`posix_program_image_contract.md`](../../docs/system/posix_program_image_contract.md)
- QEMU 回归：[`kernel/posix/qemu/README.md`](../kernel/posix/qemu/README.md)

## 内容

| 路径 | 用途 |
|---|---|
| [`elf_samples/`](elf_samples/README.md) | 静态 ELF 样本及 `spawn -> load -> start -> waitpid` 主链 |
| `tests/` | fd、errno、pipe、proc 与程序执行的模块 smoke |

能力是否可用以当前源码和当次 smoke 为准，不从样本数量推导完整 POSIX 兼容性。
