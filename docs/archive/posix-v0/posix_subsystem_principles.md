# POSIX 子系统分层与演进原则（借鉴 VSF，避免宏地狱）

这份文档不是新的任务清单，而是 Charm POSIX 子系统的上位设计约束。

它回答 4 个问题：

- Charm 为什么要做 POSIX 兼容执行面
- 可以从 VSF 借鉴什么，不能复制什么
- POSIX 子系统与平台层、运行时、用户程序层如何分层
- 后续评审 POSIX 改动时，哪些红线不能被突破

## 1. 先给结论

Charm 应该继续沿着当前路线推进：

- 先固化一个干净、可验证、same-address-space 的最小用户态骨架
- 再按真实程序样本需求扩展 POSIX surface
- 兼容层只做 facade，不反向塑造内核与 runtime 原语
- 平台层继续托管真实内存布局、启动、镜像装配与链接知识

VSF 证明了“在嵌入式上做伪 Linux 用户态”是可行的，
但 Charm 不应复制 VSF 那种以头文件宏重映射和庞大配置矩阵为中心的实现风格。

## 2. VSF 给出的有效启发

VSF 的价值，不在于它的具体实现必须被复刻，
而在于它证明了下面几件事：

- POSIX 不能只做几个 syscall，而要作为一个完整子系统来设计
- 文件描述符、文件系统、socket、console、进程/线程对象必须共享同一套运行时模型
- 用户程序可以被当作独立运行单元，而不必和系统核心混编为一个逻辑程序
- 真实第三方程序样本，比“接口看起来像 POSIX”更能驱动兼容层收敛

对 Charm 来说，VSF 更像“方向证明”，不是“实现模板”。

## 3. Charm 不复制 VSF 的地方

Charm 必须明确避免以下路径：

### 3.1 不走全局宏重映射路线

不应该把大量标准 API 的兼容建立在：

- `#define open xxx`
- `#define readdir xxx`
- `#define socket xxx`

这类全局头文件重映射之上。

原因很简单：

- 它会把 ABI、实现、配置开关、外部接口耦合在一起
- 它会显著放大 include 顺序、命名污染、调试可读性问题
- 它会让兼容层逐渐从 facade 变成系统真实入口

Charm 应优先使用：

- 清晰的内部模块边界
- 显式 wrapper
- 稳定的用户程序 ABI
- 必要时再提供薄头文件适配层

### 3.2 不让 POSIX 兼容层反向塑造内核语义

Charm 的 kernel/runtime 原语应该由系统自身需求定义，
不是由 Linux 头文件的形状直接决定。

例如：

- `fd_table` 的容量、错误模型、继承策略，应由 Charm 的确定性约束决定
- `spawn` 语义应该围绕 `posix_spawn` 风格最小闭环设计，而不是为了未来 `fork` 先污染核心模型
- `pipe`、`term`、`file`、`proc` 的边界应先在内部固化，再由 `posix.api` 统一翻译成 POSIX 行为

兼容层可以翻译语义，但不能要求底层为了“长得像 Linux”而丢掉可验证性与可裁剪性。

### 3.3 不用配置矩阵替代架构分层

功能开关是必要的，但不能把“越来越多的 feature 宏”当成系统扩展的主要手段。

Charm 更适合的做法是：

- 用模块边界表达职责
- 用模板参数表达容量与静态约束
- 用文档与回归样本表达阶段目标
- 用少量、可审计的构建开关表达平台差异

如果一个能力只能通过层层宏判断才能理解是否存在，
那通常说明分层已经开始变形。

## 4. Charm 的三层模型

POSIX 子系统必须建立在清晰的三层模型之上。

### 4.1 System Image 层

这一层负责真实板级世界的事实：

- 启动文件
- 向量表
- 链接脚本
- Flash / SRAM / TCM / 外部存储布局
- bin / hex / uf2 / 厂商镜像产物
- QEMU / 板级 bringup 的入口组织

这层必须知道真实内存布局。
应用层不应直接知道这些信息。

### 4.2 Runtime 层

这一层负责把裸机平台包装成“看起来像 hosted 环境”的执行面：

- `fd_table`
- `file`
- `pipe`
- `term`
- `proc`
- `errno`
- `env`
- `exec_context` / `exec_loader` / `program_image`

它的职责不是模拟完整 Linux 内核，
而是提供一个最小、稳定、可验证、能承接真实用户程序的执行骨架。

### 4.3 User App 层

这一层应该尽量只暴露用户程序语义：

- `main(argc, argv, envp)` 或等价入口
- `stdin/stdout/stderr`
- `open/read/write/pipe/spawn/waitpid`
- PATH / argv / envp / errno 等基础行为

用户程序可以被编译为独立映像、独立 ELF、或其他受控 Program Image，
再由 Runtime 层加载与启动。

用户程序层不应直接接触：

- 板级链接脚本
- 启动文件
- 物理内存布局
- 向量表与 Reset Handler

## 5. POSIX 子系统的内部边界

Charm 当前 POSIX 目录的分层方向是正确的，后续应该继续强化：

### 5.1 `posix.fd_table`

负责统一 fd 抽象，是整个执行面的脊柱。

它定义：

- `FdKind`
- `FdFlags`
- `FdOps`
- `FdEntry`
- 固定容量 `FdTable`

任何文件、管道、终端、设备、后续 socket，都应优先落到这条统一 fd 链路上。

### 5.2 `posix.file`

负责把 VFS/file 语义接到 fd 模型上，
把路径打开、读写、`stat`、flags 语义接入执行面。

### 5.3 `posix.pipe`

负责匿名管道、EOF/EPIPE 等最小语义。

它应该建立在 `fd_table` 之上，
而不是与 fd 体系平行长出第二套句柄模型。

### 5.4 `posix.term`

负责 stdio 绑定与 tty 判定。

它的职责是把 console/channel 等系统能力投影到 POSIX 语义上，
而不是把终端语义散落到 `api` 或 `proc` 中。

### 5.5 `posix.proc`

负责 same-address-space 模型下的最小进程执行闭环：

- `spawn`
- `waitpid`
- `kill`
- `getpid`
- 子 fd 表继承与清理
- 程序映像装载与启动收束

这个模块应该继续坚持：

- 不为了未来假想需求先做 `fork`
- 不把地址空间隔离幻觉写进当前语义承诺
- 用最小、可证明的执行模型承接真实样本

### 5.6 `posix.errno`

负责集中维护 Charm 错误模型到 POSIX errno 的翻译规则。

错误翻译必须单点收敛，避免各模块各自散落“半兼容”行为。

### 5.7 `posix.env`

负责 PATH / envp / `spawnp` 相关基础规则。

它不应混入 `proc` 的核心生命周期逻辑，
而应作为明确的执行参数解析层存在。

### 5.8 `posix.api`

`posix.api` 的定位必须保持克制：

- 组合已有服务
- 做 errno 设置
- 做当前进程绑定
- 暴露最小 POSIX 风格入口

它不是所有语义的真实实现中心，
也不应该变成一个无边界的“大而全 wrapper”。

## 6. 演进原则：先 ABI spine，后 surface 面积

Charm 的节奏应该始终是：

1. 先让 ABI spine 成立
2. 再按真实程序样本补 surface

这里的 ABI spine，至少包括：

- `main/argv/envp`
- `stdin/stdout/stderr`
- `open/read/write/close`
- `pipe/dup2/isatty`
- `spawn/spawnp/waitpid/kill/getpid`
- `_exit(code)` 收束语义
- PATH 与最小 errno 契约

只有这条主干稳定后，
再去扩展：

- `select/poll`
- `socket`
- `devfs`
- `termios`
- 更宽的 `stat`/路径错误矩阵

这样做的好处是：

- 每次新增能力都能挂到既有 spine 上
- BusyBox/QEMU smoke 可以持续当作主回归链
- 不会因为“先铺头文件宇宙”而失去主线可验证性

## 7. 用户程序的构建与装载原则

Charm 追求的不是“让裸机应用也自己写链接脚本”，
而是“让应用开发者不直接面对链接脚本”。

因此，后续应坚持：

- 最终板级镜像由平台层统一装配
- 用户程序优先作为独立程序映像构建
- Runtime 负责加载、启动、退出收束与进程级资源隔离

这条路线同时带来两个收益：

- 工程收益：把裸机内存布局与启动复杂度压回平台层
- 架构收益：把用户程序与系统核心之间的边界做实

对于 GPL 等许可证问题，
这类边界通常也更有利于把第三方程序保持为独立构建、独立运行单元；
但这只是架构收益，不应被表述成自动成立的法律结论。

## 8. 评审 POSIX 改动时的硬规则

以后所有 POSIX 相关改动，默认按下面几条检查：

### 规则 1：优先挂接既有模块，不新增旁路

如果一个功能可以挂到 `fd_table/file/pipe/proc/term/env/errno` 之一，
就不应该另起一套隐藏模型。

### 规则 2：兼容行为必须通过样本验证

优先使用：

- `Examples/posix/tests/*`
- `Examples/kernel/posix/qemu/*`
- BusyBox / 最小真实程序样本

来验证行为，而不是只看头文件是否“像 Linux”。

### 规则 3：不要为了未来能力提前污染当前模型

如果当前阶段明确不支持：

- `fork`
- 完整 signals
- 动态链接
- 完整进程隔离

那就不应该为了这些未来能力，先把现有实现做成抽象泄漏的半成品。

### 规则 4：wrapper 层允许变宽，核心层必须克制

头文件适配层、POSIX facade、程序样本可以逐步增加，
但核心模块的职责边界必须始终收敛。

### 规则 5：QEMU 主线优先于接口表面完备

只要主线目标仍是 BusyBox 和最小 Linux 用户态样本可跑，
那就应优先保障：

- QEMU smoke 持续可回归
- ABI 契约持续稳定
- 新能力不破坏既有样本

## 9. 收口后的聚焦点

基于当前仓库状态，`POSIX v0` 已经收口，因此后续更合适的节奏是：

1. 维护 `fd_table/file/pipe/proc/term/errno/env/api` 这条已收口主骨架
2. 只在 BusyBox、real-ELF、newlib 或真实用户态样例出现阻塞时补路径/errno/fd 细节
3. 继续把每次增量约束为“真实阻塞点 -> 最小契约 -> 最小 smoke -> 文档同步”
4. 把 `select/poll`、`socket/devfs/termios` 等更宽接口面保留为需求驱动项，而不是默认推进项

这意味着 Charm 的目标不是“比 VSF 更快铺满 POSIX 头文件”，
而是“在已收口的最小用户态骨架上，按真实需求稳定成长为可维护的执行面”。

## 10. 这份文档不负责什么

这份文档不负责替代：

- 三层执行模型：`docs/system/posix_three_layer_contract.md`
- 具体接口草案：`docs/system/posix_spawn_minimal_design.md`
- fd 表设计：`docs/system/posix_fd_table_minimal_design.md`
- 错误语义与 errno 映射：`docs/system/posix_error_semantics.md`、`docs/system/posix_errno_mapping.md`

阶段 roadmap 与任务清单已经归档到 `docs/archive/posix-v0/`。

它只负责提供一个统一判断标准：

Charm 要做的是一个干净、可验证、逐步扩展的 POSIX 子系统；
不是一个靠宏森林和配置矩阵堆出来的 Linux 幻觉层。
