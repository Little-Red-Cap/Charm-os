# POSIX 示例入口

## 文档状态

- `status`: `supporting`
- `scope`: POSIX ProgramImage fixture 与模块测试路由
- `authority`: [`posix_support_overview.md`](../../docs/system/posix_support_overview.md) 与本目录源码

本目录保存 POSIX 兼容执行面的 fixture 和模块测试，不定义完整 Linux/POSIX 兼容性。

## 内容

| 路径 | 用途 |
|---|---|
| [`elf_samples/`](elf_samples/README.md) | Cortex-M freestanding ELF fixture |
| `tests/` | fd、errno、pipe、proc 与 ProgramImage smoke |

执行模型见 [`ProgramImage contract`](../../docs/system/posix_program_image_contract.md)，系统回归见
[`kernel/posix/qemu`](../kernel/posix/qemu/README.md)。能力状态以当前源码和当次 runner 为准。
