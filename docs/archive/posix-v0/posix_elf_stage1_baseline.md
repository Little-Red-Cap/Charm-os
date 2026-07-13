# POSIX / ELF 第一阶段验证基线

这份文档冻结 Charm 当前这一轮 `QEMU + 真实 C ELF` 落地验证的官方主链。

它回答的是一个很具体的问题：

> 本文记录 Charm 第一阶段用户态程序执行面的验收基线。

这不是一份新的 POSIX 总览，也不是新的扩张路线图。
它只是把当前已经存在的能力、样本、QEMU 入口和自动化验收口径，收敛成一条明确的官方验证链。

---

## 1. 第一阶段目标

第一阶段只做下面这件事：

- 在 `QEMU + Cortex-M7 + same-address-space POSIX userland` 上，
- 稳定执行一组真实静态 C ELF 小程序，
- 并以自动化 smoke 作为主要胜利标准。

第一阶段**不**做：

- H747 实板执行
- BusyBox 扩面
- ModuleX 双线验证
- `fork`
- 完整 signals
- 动态链接
- 完整 Linux 进程模型

第一阶段验证：

> Charm 已经具备一条真实用户态程序执行主链。

---

## 2. 官方主链

第一阶段的唯一官方主链表述为：

`spawn / spawnp -> resolve image -> load ELF -> start image -> waitpid`

围绕这条主链，当前官方支持的映像输入形态只有两种：

- `elfmem:` 内嵌 ELF 样本
- file-backed ELF

第一阶段不把 `ModuleX` 作为并行成功标准。

---

## 3. 最小用户态能力面

第一阶段官方承诺的最小用户态能力，仅限于当前真实 ELF 样本已经依赖的部分：

- `argc/argv/envp`
- `write/read/open/close`
- `fstat/isatty`
- `exit/_exit`
- `PATH` 搜索
- 最小 `cwd` 解析
- `stdin/stdout/stderr`
- `pipe/dup2`
- `spawn/spawnp/waitpid`

这条线依赖但不重新定义的共享底座包括：

- `posix.api`
- `posix.exec_loader`
- `posix.program_image_elf`
- `posix.user_runtime`
- `fd_table`
- `fs.vfs`
- `pipe`
- `newlib syscall bridge`

后续新增兼容能力，必须继续遵守：

- 先有真实 blocker
- 再补最小契约
- 再补最小 smoke
- 最后同步文档

---

## 4. 官方旗舰样本组

第一阶段的官方旗舰样本组固定为：

- `hello`
  - 最小装载、stdout、exit 主链
- `argv_dump`
  - `argc/argv` 入口 ABI
- `env_dump`
  - `envp` 启动上下文
- `stderr_demo`
  - `stdout/stderr` 分流
- `exit_code`
  - 退出码回收
- `cat_file`
  - 文件读取与 pipe-backed stdin
- `write_file`
  - `open(O_TRUNC)`、写入、重开读回
- `append_file`
  - `open(O_APPEND)`、追加写、结果可见性
- `fd_probe`
  - `term/file/pipe` fd 语义
- `stat_probe`
  - 最小 `fstat` 类型与 size 语义

这组样本用于冻结第一阶段“什么叫成功”。

后续可以新增样本，但不应随意替换这组样本的角色。

---

## 5. 官方自动化验收链

第一阶段的自动化验收链分成两部分：

### A. 主线 QEMU smoke

入口：

- `Examples/kernel/posix/qemu/run_qemu_ci.ps1`

目标：

- `posix-qemu-demo.elf`

必须覆盖：

- POSIX smoke
- real ELF samples
- BusyBox Phase 2 smoke

成功标志：

- `[posix-smoke] end ok`
- `bb2 all ok`

### B. newlib stdio 专项 smoke

入口：

- `Examples/kernel/posix/qemu/run_qemu_stage1_ci.ps1`
  或
- `Examples/kernel/posix/qemu/run_qemu_ci.ps1 -RequireBusyboxPhase2:$false`

目标：

- `posix-qemu-newlib-stdio.elf`

这条链是专项验证，不混成主线定义，但必须持续可运行。

---

## 6. 第一阶段验收口径

第一阶段闭环需同时满足：

- `elfmem:` 样本执行成功
- file-backed ELF 执行成功
- `hello` 输出与退出码正确
- `argv/envp` 传递正确
- `stderr/stdout` 分流正确
- `exit/_exit` 回收正确
- `open/read/close/write/append` 行为正确
- `fstat/isatty` 对 `term/file/pipe` 的最小语义正确
- `spawnp + PATH` 工作正常
- QEMU 主线 smoke 绿色
- newlib stdio 专项 smoke 绿色

验收原则：

- 以自动化结果为准
- 不以单次手工演示为准
- 没有进入自动化回归的能力，不算第一阶段闭环能力

---

## 7. 对第二阶段 H747 迁移的冻结边界

第一阶段虽然不做 H747 执行，但需要提前冻结语义边界，避免第二阶段重写模型。

第二阶段允许替换的东西：

- image source
- load buffer / memory layout
- hostcall/backend surface
- console/file/device backend

第二阶段不应重新定义的东西：

- `ProgramImage` 的 ELF 主链语义
- `user_runtime` 的最小用户态 ABI
- `exec_context` 的退出与错误收束角色
- `fd_table` / `spawn` / `waitpid` 的用户可见行为骨架
- 用户程序入口模型

第二阶段应验证同一条主链迁移到 H747，不新增另一套用户态程序模型。

---

## 8. 推荐入口

如果你要沿这条基线继续工作，推荐阅读顺序：

1. `docs/system/posix_support_overview.md`
2. 本文档
3. `Examples/posix/README.md`
4. `Examples/posix/elf_samples/README.md`
5. `Examples/kernel/posix/qemu/README.md`

维护方式和当前验收入口只以 `docs/system/posix_support_overview.md`、源码与当次 runner 为准。
