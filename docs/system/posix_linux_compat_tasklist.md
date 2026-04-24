# Linux 生态兼容任务清单（维护态）

这份清单不再用于驱动一条“持续铺开 POSIX 能力面”的主线工程。

从当前仓库状态看，`POSIX v0` 已经收口，当前更合适的工作方式是：

- 把已有主干稳定下来；
- 只在真实程序或真实阻塞点出现时做增量补齐；
- 每次增量都绑定最小契约、最小 smoke 与同步文档更新。

关联文档：

- 收口判定：`docs/system/posix_v0_closure_checklist.md`
- 阶段总结：`docs/system/posix_stage_summary.md`
- 总览：`docs/system/posix_support_overview.md`
- 维护期协作：`docs/system/posix_maintenance_mode_collaboration.md`
- BusyBox 阶段验收：`docs/system/posix_busybox_phase_checklist.md`

---

## 当前状态

当前可视为已经成立的基线：

- BusyBox Phase 1 / 2 主干链路已稳定；
- Phase 3 minimum 已闭环；
- 公共 C surface 与 newlib bridge 已进入可维护状态；
- QEMU 主线 smoke 已恢复；
- 专项 newlib stdio smoke 已恢复。

因此，后续 POSIX 工作不再按“Phase 4 / Phase 5”线性展开，而是进入维护态 backlog。

---

## 已收口的主干能力

以下能力已经不再是默认主线任务，而是已收口基线：

- `spawn` / `spawnp` / `waitpid`
- 最小 `kill` / `sleep` / `getpid` / `ps`
- `fd_table` / `dup` / `dup2` / `fcntl` 最小闭环
- `pipe` 最小可用语义
- `open/read/write/close/stat/fstat/lseek`
- `opendir/readdir/closedir`
- `stdin/stdout/stderr`、`isatty`、`/dev/null`、`/dev/tty`、`/dev/console`
- PATH / argv / envp / errno 最小契约
- real-ELF 装载与 `_exit(code)` 收束
- BusyBox / shell / newlib / public C header 的最小一致性

这些能力的后续工作原则是：

- 除非出现真实阻塞点，否则不主动扩面；
- 只修契约漂移、真实回归、或真实用户态样例卡点；
- 不为了“看起来更像 Linux”而主动扩大复杂度。

---

## 维护态 backlog 分组

下面这些 backlog 不是默认连续推进项，而是“按触发条件取用”的增量池。

### A. 契约加固类

触发条件：

- 现有 smoke 暴露回归；
- 公共头文件、runtime bridge、QEMU 行为之间出现漂移；
- 同一语义在 API / shell / BusyBox / newlib 表面不一致。

典型项：

- 补齐已承诺接口的边界行为；
- 修正 `errno`、返回值、EOF/EPIPE/EBADF/EINVAL 等最小契约；
- 收紧 `fd` / path / cwd / dirent / redirect 的成功侧与失败侧一致性；
- 修正文档与实现不一致的地方。

验收要求：

- 至少一条最小回归；
- 必要时补一条 QEMU smoke；
- 更新对应文档。

### B. 真实程序阻塞类

触发条件：

- BusyBox 某个当前关注 applet 被真实卡住；
- real-ELF 样例暴露新的运行时缺口；
- 真实 C/newlib 用户态程序无法运行，且问题落在 POSIX 责任面。

典型项：

- 新增一个最小样例来复现阻塞；
- 只补齐该样例所需的最小运行时语义；
- 在样例打通后，把该能力沉淀为最小契约，而不是继续外溢扩张。

验收要求：

- 一条程序级用例；
- 一条最小 smoke；
- 一段同步文档更新。

### C. 环境扩展类

触发条件：

- 某类真实程序明确依赖一个环境能力，且不能被现有最小模型替代；
- 该能力对多个样例都有明显复用价值。

候选项：

- `/proc` 最小只读视图
- 更多 `devfs` 节点
- 更宽的 `stat` 字段矩阵
- 更细的路径/权限只读语义
- `termios` / `socket` / `select/poll` 等更高层表面

注意：

- 这些能力默认都不属于 v0 收口后的必做项；
- 只有真实样例或明确上层需求驱动时才进入开发。

### D. 工具链/构建协同类

触发条件：

- 当前模块化构建、`-fmodules-ts`、交叉编译链、QEMU 路径变化影响 POSIX 开发效率；
- 问题虽不在 POSIX 语义本身，但直接阻塞 POSIX 验证。

典型项：

- 记录工具链踩坑；
- 调整 smoke 构建入口；
- 维护 QEMU 验证脚本；
- 适配上游 CMake 组织调整。

注意：

- 这类工作通常是“协同修复”，不是 POSIX 能力扩张；
- 应记录边界，避免把非 POSIX 问题误记到 POSIX backlog。

---

## 当前建议关注的 backlog

如果需要从维护态中挑下一刀，优先级建议如下：

### P1. 文档与任务面一致性

- 持续保持 tasklist / roadmap / stage summary / closure checklist 口径一致；
- 避免旧文档继续呈现“POSIX 仍在主线铺功能”的错觉。

### P2. 真实样例驱动的小缺口

- 仅当 BusyBox、real-ELF、新的 C 用户态样例出现明确阻塞时介入；
- 先做最小复现，再做最小修复；
- 不跳过“样例 -> 契约 -> smoke -> 文档”这条链。

### P3. 上游协同适配

- 在构建系统、模块组织、工具链约束发生变化时，保证 POSIX 验证路径继续可用；
- 保持 `cmake-build-*` 目录约定与 QEMU 验证脚本可持续工作。

---

## 明确不作为默认推进项的内容

以下内容继续保留在“非默认推进”区：

- 完整 Linux syscall 兼容
- `fork`
- 完整 signal model
- process group / session / job control
- 动态链接
- 完整 `/proc`
- 完整权限模型 / 用户模型
- “BusyBox 所有 applet 都能跑”
- 仅为了“接口看起来更全”而补齐冷门 flag / 冷门 errno / 冷门路径矩阵

如果未来要做这些内容，必须先回答：

- 是哪个真实程序在阻塞？
- 现有最小模型为什么不够？
- 这次补齐的最小契约是什么？
- 对应最小验证路径是什么？

---

## 增量工作模板

后续每个 POSIX 增量项，建议统一按下面的模板记录：

### 条目模板

- **阻塞样例**：哪个程序 / applet / smoke 被卡住
- **最小缺口**：缺的具体语义是什么
- **改动边界**：落在哪个模块，不改哪些边界
- **契约**：对外可见行为怎么定义
- **验证**：最小 smoke / QEMU / 样例用例
- **文档**：需要同步哪些文档

只要一个候选改动无法填清这 6 项，就不应直接进入实现。

---

## 当前验证入口

- QEMU 主线：`Examples/kernel/posix/qemu/run_qemu_ci.ps1`
- ELF 样例：`Examples/posix/elf_samples/README.md`
- 程序级 smoke：`Examples/posix/tests/posix.programs.tests.cppm`
- 阶段总结：`docs/system/posix_stage_summary.md`
- BusyBox 阶段清单：`docs/system/posix_busybox_phase_checklist.md`

---

## 一句话结论

这份清单现在的职责，不再是“列出还没做完的 POSIX 宇宙”，而是：

> 帮助我们在 POSIX v0 已收口的前提下，只对真实问题做小而稳的增量推进。
