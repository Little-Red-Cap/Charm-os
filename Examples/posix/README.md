# POSIX 示例入口

本目录收纳 Charm POSIX / ProgramImage 路线下的真实样本、程序级 smoke 和相关测试素材。

如果你想先看现行执行面与边界，再看示例，建议先读：

- [`../../docs/system/posix_support_overview.md`](../../docs/system/posix_support_overview.md)
- [`../../docs/system/posix_subsystem_principles.md`](../../docs/system/posix_subsystem_principles.md)
- [`../../docs/system/posix_three_layer_contract.md`](../../docs/system/posix_three_layer_contract.md)

## 当前内容

### `elf_samples`

入口：

- [`elf_samples/README.md`](elf_samples/README.md)

这里维护真实 ELF 样本套件，适合回答：

- `spawn -> load_image -> start_image -> waitpid` 主链是否稳定
- 当前最小真实用户态程序都覆盖了哪些依赖面

### `tests`

目录：

- `tests/`

这里主要是程序级 smoke、fd/errno/pipe/proc 等测试模块。
更适合在你已经知道自己要看哪条 POSIX 能力线时直接进入。

## 建议怎么读

- 看真实 ELF 样本：
  先读 `elf_samples/README.md`
- 看 QEMU 回归入口：
  再回到 [`../kernel/posix/qemu/README.md`](../kernel/posix/qemu/README.md)
- 看系统契约：
  回到 `docs/system/posix_*`

## 使用提醒

- 本目录更偏“真实样本 + 测试素材”，不直接替代 `Examples/kernel/posix/qemu` 那条完整运行入口。
- 如果你需要先理解整体执行面，仍然优先从 `docs/system/` 和 `Examples/kernel/posix/qemu/README.md` 进入。
