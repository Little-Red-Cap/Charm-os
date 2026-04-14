# ELF 样本套件

本目录用于维护 Charm POSIX/ProgramImage 路线的最小真实 ELF 样本套件。
目标不是覆盖完整 Linux 用户态，而是提供一组稳定、可重复、可在 QEMU 上回归的静态程序样本，持续验证 `spawn -> load_image -> start_image -> waitpid` 主链。

## 当前样本

| Sample | Entry command | Expected output / status | 主要覆盖 | 当前输入方式 | 后续目标 |
|---|---|---|---|---|---|
| `hello` | `elfmem:hello` / `elf:/hello.elf` | stdout: `hello\n`, exit=0 | 最小 ELF 执行链、stdout 写路径 | `.elf.inc` 内嵌 + `register_elf_mem`；RAMFS 文件输入 | 增加真实 VFS/包输入 |
| `argv_dump` | `elfmem:argv_dump a b` / `elf:/argv_dump.elf a b` | stdout: `argv[0]=...` | `argc/argv` 入口 ABI、stdout | `.elf.inc` 内嵌 + `register_elf_mem`；RAMFS 文件输入 | 保持 argv 纯断言 |
| `env_dump` | `elfmem:env_dump` / `elf:/env_dump.elf` | stdout: `env[0]=...` | `envp` 入口 ABI、stdout | `.elf.inc` 内嵌 + `register_elf_mem`；RAMFS 文件输入 | 后续补空 env / 边界用例 |
| `stderr_demo` | `elfmem:stderr_demo` / `elf:/stderr_demo.elf` | stdout: `out\n`, stderr: `err\n`, exit=0 | `0/1/2` 分流、stderr 重定向 | `.elf.inc` 内嵌 + `register_elf_mem`；RAMFS 文件输入 | 增加 `2>&1`/文件重定向 |
| `exit_code` | `elfmem:exit_code 7` / `elf:/exit_code.elf 7` | wait status code=7 | 入口参数、退出码回收 | `.elf.inc` 内嵌 + `register_elf_mem`；RAMFS 文件输入 | 增加 shell/管道状态传递 |
| `cat_file` | `elf:/cat_file.elf /cat.txt` / `elf:/cat_file.elf -` | stdout: 输入内容原样透传，exit=0 | `open/read/close/fstat/isatty` 最小文件链；以及 pipe-backed stdin 读取 | RAMFS 文件输入 + pipe stdin | 推进到真实 VFS/包输入 |
| `fd_probe` | `elf:/fd_probe.elf /cat.txt` | stdout: `stdin=chr tty=1` / `stdout=fifo tty=0` / `file=reg tty=0`, exit=0 | fd 语义探针（`term/pipe/file` 的 `isatty/fstat` + 错误路径） | RAMFS 文件输入 | 扩展到更多 fd 类型对照 |
| `stat_probe` | `elf:/stat_probe.elf /stat.txt` | stdout: `file=reg,size=10` + `stdout=fifo,size=0` + `stderr=chr,size=0`, exit=0 | `fstat` 的 `mode/类型/size` 语义探针 | RAMFS 文件输入 | 继续覆盖更完整字段矩阵 |

## 生成方式

- 构建脚本：`Examples/posix/elf_samples/build_elf_samples.ps1`
- 链接脚本：`Examples/posix/elf_samples/elf_samples.ld`
- 生成产物：
  - `Examples/posix/elf_samples/*.elf.inc`
  - 临时 ELF 文件输出到 `Examples/posix/elf_samples/out/`（已在 `.gitignore` 中忽略）

当前样本使用 ARM Cortex-M Thumb 静态 freestanding 构建，链接到测试期固定加载基址，以便通过 QEMU 回归真实 ELF 输入路径。

## 最小 shim 依赖面

当前前 4 个基础样本共同依赖的最小运行面：

- 入口 ABI：`int entry(int argc, char** argv, char** envp)`
- 程序加载：ELF `PT_LOAD` 段复制、`memsz > filesz` 零填充、entry 落点校验
- 进程执行：`spawn`、`waitpid`
- fd/stdio：`0/1/2` 已绑定到 `fd_table`
- 输出/退出 hostcall：
  - `write(fd, buf, len)`
  - `exit(code)`

当前新增 `cat_file` 后，样本套件已经开始要求：

- `open/close/read/fstat`
- `isatty(term)=1`、`isatty(file)=0` 与 `isatty(pipe)=0`

当前样本仍未要求：

- 堆分配/`brk`/`mmap`
- 信号、会话、动态链接

这意味着它们适合作为 Charm 从“可加载 ELF”走向“可常态化执行真实程序”的 P0 样本，而不是最终 Linux 兼容结论。

## 验收入口

程序级 smoke 位于：`Examples/posix/tests/posix.programs.tests.cppm`

当前相关验收段：

- `test_elf_real_samples()`：4 个真实 ELF 样本
- `test_elf_file_spawn()`：文件型 ELF 输入 smoke
- `test_elf_header_stub()`：ELF loader 边界与负例

QEMU 回归命令：

```powershell
cmake --build Examples/kernel/posix/qemu/cmake-build-debug -j 8
Examples/kernel/posix/qemu/run_qemu_ci.ps1 -ElfPath ./Examples/kernel/posix/qemu/cmake-build-debug/posix-qemu-demo.elf -TimeoutSec 60
```

## 当前限制与下一步

当前仍属于“受控真实样本”阶段，主要限制：

- 测试构建下使用固定 `.elf_load` 区域
- 主要输入方式仍是 `.elf.inc` + `elfmem:` 注册
- hostcall 仍是最小测试接口，不是完整 libc/syscall 层

下一步优先级建议：

1. 增加非 `.elf.inc` 的 ELF 输入路径，复用同一 `load_image/start_image` 主链
2. 将基础样本的依赖逐步扩到 `read/open/close/fstat/isatty`
3. 在保持样本稳定的前提下，缩减对固定加载布局的隐含依赖

## 第一批 shim 推进建议

建议按真实样本驱动，而不是一次性扩整张 syscall 表：

- `read`：先做“从文件读取并写到 stdout”的极小样本，覆盖 file fd 与 stdin 两条路径
- `open` / `close`：先做“打开文件 -> 读取/写入 -> 关闭 -> 再验证可见性”的样本
- `fstat`：优先验证 `term` / `file` / `pipe` 的区分，不急着追完整 stat 字段
- `isatty`：先让 ELF 样本显式断言 `isatty(1)=1`、`isatty(file)=0`、`isatty(pipe)=0`

这批 shim 的目标不是“POSIX 名字更全”，而是让后续 `cat`、重定向和 shell 行为具备更接近 Linux 用户态程序的真实依赖面。
