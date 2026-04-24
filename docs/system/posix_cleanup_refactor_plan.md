# POSIX 大整理实施清单

## 目标
在保持 QEMU 主线烟测全绿的前提下，做一次面向结构的收敛重构，减少 `posix.proc`、错误语义和 programs smoke 的继续堆叠，让后续 Linux 用户态样本可以沿更干净的主干继续推进。

## 当前基线
- QEMU 主线通过：`posix smoke + busybox phase2 smoke`
- ELF 常规执行路径已成立：`spawn -> load_image -> start_image`
- `_exit(code)` ABI v0 已成立：`ExecContext + setjmp/longjmp`
- `fd/errno` 已有一批稳定契约，详见 `docs/system/posix_stage_summary.md`
- `stat_probe` 仍是单独隔离项，不阻塞主线
- Phase 1 已落地：programs smoke 已拆成 `test_harness` / `exec` / `fdpath` / `shell`
- Phase 2 已开刀：`ExecContext` 已抽到 `posix.exec_context`，`setjmp` 仍保留在 `start_image()` 调用栈上
- Phase 2 继续推进：ELF hostcall 已抽到 `posix.elf_hostcall`，镜像注册/来源解析已抽到 `posix.program_catalog` / `posix.exec_source`

## 本次整理的四个主轴

### A. `posix.proc` 结构拆分
目标：把“进程生命周期”“执行上下文”“ELF hostcall”三个责任拆开，避免继续在一个模块里缠绕增长。

计划拆分：
- `posix.proc`
  - 负责：pid、进程表、`spawn/waitpid`、fd 绑定、镜像加载入口编排
  - 不再直接承载 hostcall 细节和 `_exit` 控制流机制
- `posix.exec_context`
  - 负责：`ExecContext`、`setjmp/longjmp` 退出跳转点、`explicit exit > return` 收束
- `posix.elf_hostcall`
  - 负责：hostcall 分发、errno 通道、fd/file bridge、`_exit` 入口

验收：
- `posix.proc.cppm` 体积与职责明显下降
- `_exit` smoke 继续全绿
- `waitpid()` 行为不变

### B. 错误语义统一收口
目标：把已经成立的 v0 错误语义从“分散特判”收成一张明确的实现约束表。

本次收口范围：
- `util::Errc`
- `posix.errno`
- `fs_ramfs` / `fs_vfs`
- `posix.file` / `posix.api`

优先保证的 v0 契约：
- `ENOENT`
- `EISDIR`
- `ENOTDIR`
- 当前 fd invalid 的 v0 约定
- `EIO` / `ENOTSUP` 等现有 fallback

明确暂缓：
- 完整 `stat()` 类型矩阵
- 完整路径错误矩阵

验收：
- `posix.errno.tests` 覆盖新增 typed path errors
- programs smoke 中路径类型错误保持稳定
- 新增契约能回写到阶段总结文档

### C. tests / harness 拆分
目标：把 `Examples/posix/tests/posix.programs.tests.cppm` 从“大收纳箱”拆成更短、更可观测的测试块，降低函数体/布局级异常的触发概率。

建议拆分：
- `Examples/posix/tests/posix.test_harness.cppm`
  - `Harness`
  - `RamFsMount`
  - `check_*`
  - 通用 read/write helper
- `Examples/posix/tests/posix.programs.exec.tests.cppm`
  - hello / argv / exit / explicit-exit / ELF load path
- `Examples/posix/tests/posix.programs.fd.tests.cppm`
  - fd_probe / read-write-error contracts / pipe-ish userland cases
- `Examples/posix/tests/posix.programs.path.tests.cppm`
  - path-type errors / file-backed ELF path behavior / 后续 `stat_probe`
- `Examples/posix/tests/posix.programs.shell.tests.cppm`
  - echo/cat/sh redirection and pipe phases

验收：
- 主 smoke 入口仍然一键跑全量
- 单个测试块可以独立开关
- `stat_probe` 后续可单独启停，不再拖住整包

### D. 实验态与待修项显式标记
目标：把“当前只是 v0 过渡态”的点写清楚，避免后续被误当成最终结构。

明确标记：
- `stat_probe`：布局级异常，隔离待修
- ELF 模型：same-address-space v0，不等同完整用户态/内核态隔离
- typed path errors：当前只收 `open()` 第一批，不代表完整路径语义已完成

验收：
- `docs/system/posix_stage_summary.md` 持续同步
- 待修项有单独条目，不再隐含在测试里

## 建议推进顺序

### Phase 1: 先拆 tests / harness（已完成第一轮）
原因：
- 风险最低
- `stat_probe` 已证明测试宿主形态本身会反咬
- 拆完以后再动 `posix.proc`，验证面更干净

交付：
- 抽出 `posix.test_harness.cppm`
- programs smoke 至少拆成 `exec` 与 `fd/path` 两块
- 保持 QEMU 主线全绿

### Phase 2: 再拆 `posix.proc`（已开始）
原因：
- `_exit` / hostcall / wait 已经够复杂，继续堆在一个文件里收益下降
- 有了更干净的测试块后，更容易验证拆分没有回归

交付：
- `posix.exec_context`
- `posix.elf_hostcall`
- `posix.proc` 只保留 orchestrator 职责

### Phase 3: 错误语义统一收口
原因：
- 拆完结构后，再统一错误表，修改面更可控
- 可以顺手把路径/fd 契约回写文档和 smoke

交付：
- 一份明确的 v0 错误语义表
- 代码层消减散落特判

## 明确不做的事
- 不在这次整理里追完整 POSIX / Linux 进程模型
- 不引入 MMU/地址空间级用户态隔离
- 不一次性补全 `stat` / `fcntl` / `signal` / `procfs`
- 不让 `stat_probe` 阻塞主线

## 整理完成的判断标准
- 主线仍是：`[ok] posix smoke + busybox phase2 smoke`
- `posix.proc` 职责边界更清楚
- tests 不再集中在一个巨型 programs 文件里
- v0 契约与待修项有明确文档锚点
- 后续新增真实 ELF 样本时，不需要再靠大量临时日志才能定位

## Progress Notes
- Phase 2 now includes posix.proc_types for shared spawn/process types.
- posix.exec_loader now owns ELF buffer/file/candidate loading helpers so posix.proc can keep shrinking toward an orchestrator.

- posix.image_resolver now owns load_image source selection and fallback wiring, further shrinking posix.proc toward an orchestrator.
- posix.spawn_fds now owns child fd-table setup and file-actions application, so posix.proc keeps less spawn-specific wiring.
- ELF hostcall invalid-fd errno has now converged to `EBADF`, including `read/write/fstat/close` on bad fds.
