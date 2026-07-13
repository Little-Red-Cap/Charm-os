# POSIX / ELF 第一阶段验证基线

> `status`: `archived`

本文记录早期 `QEMU + Cortex-M7 + same-address-space POSIX userland` 的静态 C ELF 验收边界，
不代表当前 runner、样本或能力状态。现行入口见
[`posix_support_overview.md`](../../system/posix_support_overview.md)。

## 历史主链

```text
spawn / spawnp -> resolve image -> load ELF -> start image -> waitpid
```

当时验证了 memory-backed `elfmem:` 与 file-backed ELF 两种输入，并以真实小程序覆盖：

- `argc/argv/envp`、stdout/stderr 与 exit status；
- open/read/write/append、fstat/isatty 与 stdin/stdout/stderr fd；
- pipe/dup2、PATH/cwd 解析、spawn/spawnp/waitpid；
- newlib stdio 的专项执行路径。

样本名称、token 和命令属于当时 runner 快照，不在本文冻结；需要追溯时使用 Git 历史和对应提交。
阶段结果不能证明当前 QEMU 仍通过，也不能外推到 H747、真实板或完整 Linux 兼容。

## 未覆盖

该基线没有证明地址空间隔离、`fork()`、完整 signal、动态链接、BusyBox 扩面、ModuleX 等价执行、
真实板 backend 或产品级 POSIX ABI。

## 保留的不变量

迁移到其它 target 时可以替换 image source、load buffer、memory layout、console/file/device backend，
但不应为每个平台重建一套程序生命周期。不同 image loader 应继续汇入同一组 resolve、load、start、
wait、exit 与资源回收语义。

兼容行为必须由真实程序阻塞点驱动，并由最小测试固定；API、样本或历史通过记录的存在不构成当前能力。
