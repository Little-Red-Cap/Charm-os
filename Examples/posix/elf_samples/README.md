# POSIX ELF 样本

该目录保存 Cortex-M Thumb freestanding ELF，用于验证
`spawn -> load_image -> start_image -> waitpid`。样本不是 Linux 用户态兼容声明。

## 样本

| sample | 主要覆盖 |
|---|---|
| `hello` | 最小 ELF load、stdout、exit 0 |
| `argv_dump` | `argc/argv` entry ABI |
| `env_dump` | `envp` entry ABI |
| `stderr_demo` | fd 1/2 分流与 stderr redirect |
| `exit_code` | 参数解析与 wait status |
| `cat_file` | open/read/close/fstat/isatty、file 或 pipe stdin |
| `write_file` | create/truncate/write/reopen/EOF 与 cwd relative path |
| `append_file` | append 保留前缀并可重开读取 |
| `fd_probe` | term/pipe/file 的 isatty/fstat/error |
| `stat_probe` | fd type、mode 与 size |

样本可通过 `elfmem:<name>` 的内嵌 bytes 或 `elf:/path.elf` 的文件路径进入同一 ProgramImage
loader。具体 argv、fixture 文件和 expected output 由
[`posix.programs.tests.cppm`](../tests/posix.programs.tests.cppm) 维护。

## 生成

- [`build_elf_samples.ps1`](build_elf_samples.ps1)：构建 ELF 并更新 `*.elf.inc`；
- [`elf_samples.ld`](elf_samples.ld)：测试链接布局；
- `out/`：临时 ELF，已忽略；
- `*.elf.inc`：供 memory registry 测试使用的生成输入。

固定 load base 与 Cortex-M ABI 是该 fixture 的输入条件，不是通用 ELF ABI。

## 运行面

样本共同依赖：

- ELF `PT_LOAD` copy、BSS zero-fill、entry range validation；
- `spawn/waitpid`、argv/envp 与 exit status；
- fd 0/1/2、read/write/open/close/fstat/isatty；
- RAMFS file input 与 pipe-backed stdin。

它们不要求 heap、`brk/mmap`、dynamic linker、signal、session 或完整 libc/syscall surface。

## 验证入口

- Program tests：[`Examples/posix/tests`](../tests/)
- QEMU runner：[`Examples/kernel/posix/qemu`](../../kernel/posix/qemu/README.md)
- ProgramImage contract：
  [`posix_program_image_contract.md`](../../../docs/system/posix_program_image_contract.md)

是否通过以当次生成、构建和 QEMU runner 结果为准，不从已提交的 `*.elf.inc` 推导当前绿色状态。
