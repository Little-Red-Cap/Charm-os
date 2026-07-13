# POSIX ELF Fixture

## 文档状态

- `status`: `supporting`
- `scope`: POSIX ProgramImage 的 Cortex-M ELF fixture
- `authority`: [`build_elf_samples.ps1`](build_elf_samples.ps1)、[`elf_samples.ld`](elf_samples.ld) 与
  [`../tests/`](../tests/)

本目录生成 Cortex-M7 Thumb freestanding ELF，用于 POSIX
`spawn -> load_image -> start_image -> waitpid` 测试。它不声明 Linux 用户态兼容，也不使用 resident
`AppImage/CharmAppApi` 入口。

## Fixture 边界

- linker 入口是 `entry(argc, argv, envp)`，默认 load base 为 `0x20080000`；
- `.hostcall` 表由测试 runtime 注入，具体调用面以 [`elf_hostcall.h`](elf_hostcall.h) 为准；
- image 可由 `elfmem:<name>` 或 `elf:<path>` 进入同一 ProgramImage loader；
- ELF materialize、入口 ABI 和生命周期由
  [`posix_program_image_contract.md`](../../../docs/system/posix_program_image_contract.md) 定义。

这些地址、hostcall 和 ABI 是测试 fixture 条件，不是通用 ELF ABI。

## 生成与验证

[`build_elf_samples.ps1`](build_elf_samples.ps1) 维护样本集合、编译参数和生成路径：临时 ELF 写入已忽略的
`out/`，内嵌 registry 输入写入 `*.elf.inc`。不要在本页复制样本 inventory。

程序行为和 expected output 由 [`../tests/`](../tests/) 维护；系统级入口是
[`kernel/posix/qemu`](../../kernel/posix/qemu/README.md)。已提交的 `*.elf.inc` 只是生成输入，不能证明
当前 runner 通过。
