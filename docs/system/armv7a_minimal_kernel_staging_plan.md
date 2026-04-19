# ARMv7-A 从裸机到最小内核的分阶段路线

这份文档用于回答一个很具体的问题：

- 我们现在已经在 QEMU 上把 Cortex-A 的裸机 bring-up 跑起来了
- 那么接下来，怎样沿着一条不会把仓库重新拖回宏地狱的路径，逐步长成“最小内核”

先把结论写在前面：

- 这件事完全有可能
- 但第一目标不该是“立刻做成完整操作系统”
- 更健康的目标是先做成一个可观测、可验证、可回归的 ARMv7-A 最小内核底座

这里说的“最小内核”，当前阶段指的是：

- 单核
- 单地址空间优先
- 内核态优先
- 有稳定的异常、IRQ、定时器、线程切换和调度入口
- 还不急着承诺完整进程模型、完整用户态隔离和完整 POSIX

这条路线和下面两份文档是衔接关系：

- 平台 bring-up 边界：`docs/system/armv7a_platform_contract.md`
- RK3506 真实板级分层：`docs/board/rk3506/boot_staging_plan.md`

## 1. 当前站位

截至当前阶段，我们已经基本站在了一个正确的位置上：

- `Examples/kernel/armv7a/qemu/` 负责 QEMU `virt` 上的 ARMv7-A 裸机验证
- `targets/armv7a/common/` 负责 ARMv7-A 公共契约与观测语义
- QEMU 路径已经覆盖了向量、异常、abort、GIC、generic timer、handoff、MMU/cache/TLB 等关键证据
- host 侧 `Examples/boot/bootloader_demo/main.cpp` 已经能承担一部分“契约语义验证器”的角色

这意味着我们不是从“能不能在 Cortex-A 上亮个串口”开始，而是已经有了继续往上长的地基。

更重要的是，当前路线已经证明了一个关键原则：

- 平台差异可以被收进 leaf target
- 公共层可以只保留可复用契约，而不吞板级常量和 SoC 私货

这正是后面走向最小内核时最值钱的秩序。

## 2. 目标与非目标

## 2.1 当前目标

我们这一阶段真正想得到的是：

- 稳定的 ARMv7-A 异常/中断/定时器平台入口
- 能承接线程与调度器的最小 arch hook
- 能支撑单核最小线程模型的运行时
- 能在 QEMU 上形成可重复、可自动化的回归证据
- 将来能把同一套语义迁移到 RK3506 这类真实 Cortex-A7 板级

## 2.2 当前非目标

现阶段不应过早承诺下面这些内容：

- 多核调度
- 复杂用户态隔离
- 完整 ELF 进程装载体系
- 完整 VFS / 驱动模型闭环
- 完整 POSIX / Linux syscall 兼容
- 为某颗芯片的早期 DDR/BootROM 约束，反向改造整仓公共层

换句话说，先做“能稳定站立和行走的最小内核”，不是先做“长得像大操作系统的全家桶”。

## 3. 边界原则

如果这条路线只记住一件事，那就是：

- 平台差异进入 target
- 公共源码退出 `#if`

展开成工程约束，就是下面四条。

### 3.1 Leaf target 持有机器私货

下面这些内容应该继续停留在 leaf target 或 leaf module：

- `startup.S`
- `vectors.S`
- `linker.ld`
- MMIO 基地址
- GIC/UART/timer 的板级连线
- cache/TLB/barrier 的板级顺序细节
- QEMU `virt` 和 RK3506 的寄存器常量

也就是说，`Examples/kernel/armv7a/qemu/` 和未来的 `targets/rk3506/` 应该继续负责“怎么让具体机器进入可运行状态”。

### 3.2 `targets/armv7a/common/` 持有 ARMv7-A 公共语义

下面这些更适合沉淀在 ARMv7-A 公共层：

- 异常帧解释语义
- 向量进入/退出语义
- fault/status 解码
- 中断 lifecycle / timeout / special-ack 语义
- handoff 上下文与切换前提
- 以后可继续加入的 context frame / trap frame / timer route 契约

这些东西描述的是“ARMv7-A 世界里什么叫正确”，而不是“某块板子寄存器是多少”。

### 3.3 `Modules/system/kernel/` 持有平台无关的内核策略

仓库里已经存在的：

- `scheduler.cppm`
- `thread.cppm`
- `timer.cppm`
- `sync*.cppm`
- `task_*.cppm`

它们不应该被 Cortex-A bring-up 绕开，也不应该被复制出第二套“裸机专用小内核”。

更健康的方向是：

- 继续把它们当作长寿命内核核心
- ARMv7-A 工作先提供这些核心真正落到 Cortex-A 裸机所需的 arch ingress seam

也就是说，我们要补的是“入口与落地”，不是再平行造一个内核。

### 3.4 兼容层不反向塑形内核

后续即便继续推进 POSIX，也要坚持已有原则：

- POSIX 是 facade，不是内核本体
- 板级 bring-up 是 leaf，不是公共系统模型
- Boot staging 是板级现实，不是全仓通用世界观

这三条守住了，仓库才不会重新滑回“大而混”的状态。

## 4. 推荐目录草图

下面这棵树不是要求立刻一次性重构完成，而是建议的长期边界形状：

```text
targets/
  armv7a/
    common/
      armv7a_exception_contract.hpp
      armv7a_interrupt_contract.hpp
      armv7a_interrupt_lifecycle_contract.hpp
      armv7a_interrupt_timeout_contract.hpp
      armv7a_special_interrupt_contract.hpp
      armv7a_handoff_contract.hpp
      armv7a_psr_contract.hpp
      armv7a_fault_*_contract.hpp
      armv7a_vector_*_contract.hpp
      armv7a_context_contract.hpp            # future
      armv7a_timer_route_contract.hpp        # future
  rk3506/
    ...

Examples/
  kernel/
    armv7a/
      qemu/
        startup.S
        vectors.S
        linker.ld
        qemu_virt_platform.cpp
        qemu_virt_platform_interrupts.cpp
        early_uart.cpp
        irq_timer.cpp
        ...

Modules/
  system/
    kernel/
      scheduler.cppm
      thread.cppm
      thread_api.cppm
      task_runtime_api.cppm
      task_syscall_api.cppm
      task_syscall_catalog.cppm
      task_syscall_dispatch.cppm
      task_syscall_table.cppm
      task_syscall_frame.cppm
      timer.cppm
      sync*.cppm
      task_*.cppm
      irq/                                 # future
      exception/                           # future
      arch/                                # future arch hook facade
      syscall/                             # future
    mm/                                    # future
      page_allocator/
      mapping/
      address_space/
      heap/
```

这棵树最想表达的不是目录本身，而是边界：

- `.S`、链接脚本、寄存器常量留在 leaf
- ARMv7-A 公共“正确性语义”留在 `targets/armv7a/common/`
- 真正长期存活的调度/线程/同步/内存策略留在 `Modules/system/`

## 5. 最小内核应该怎么长

建议按五段推进，不横跳。

## 5.1 阶段 0：裸机 bring-up 证据层

这是我们已经在做，而且已经拿到不少成果的一层。

目标：

- 验证 reset 到主逻辑的最小路径
- 验证异常、IRQ/FIQ、timer、handoff、MMU/cache/TLB 的关键证据
- 让 host 验证与 QEMU 运行证据共享同一套契约语义

当前产物：

- `Examples/kernel/armv7a/qemu/`
- `targets/armv7a/common/`
- 多个 `run_qemu_*_ci.ps1`
- host 侧契约验证

阶段完成标志不是“功能多”，而是“异常世界不再模糊”。

## 5.2 阶段 1：补齐 arch ingress seam

这是从 bring-up 走向最小内核最关键的一刀。

这一刀要解决的问题是：

- 线程、调度器、timer 以后如何进入 ARMv7-A
- 内核核心如何拿到异常/中断/tick 的统一入口
- 平台叶子如何提供最少但稳定的 hook，而不是把内核绑死在某个 example 里

建议先收敛出四类最小入口：

- `ArchExceptionPort`
- `ArchInterruptPort`
- `ArchTimerPort`
- `ArchContextPort`

在当前这条 `runtime_glue / runtime_bridge` 路线上，还应该同时给下半层留出一个更靠近落地形状的入口：

- `RuntimeLoopPort<Tick>`

它不替代上面四类 arch hook，而是把 lower-half 真正会持有的那组最小动作收成一扇更明确的门：

- `advance_tick(now)`
- `defer_from_isr(task, event)`
- `bootstrap_idle(...)`
- `bootstrap_worker(...)`
- `run_once_or_idle(now)`

它们的责任可以先非常小：

- 安装/切换异常向量
- 允许/屏蔽 IRQ/FIQ
- 设置/确认 tick 来源
- 保存/恢复线程上下文

但它们必须足够明确，能让之后的线程与调度器不用直接理解 QEMU `virt` 或 RK3506 的细节。

这一阶段最重要的结果不是“线程已经跑起来”，而是“线程以后知道该从哪扇门进来”。

## 5.3 阶段 2：单核最小线程模型

这里的目标不是完整 RTOS，而是 Cortex-A 上的第一个可运行线程内核。

建议范围控制为：

- 单核
- 内核态线程
- 固定大小内核栈
- cooperative 优先，preemptive 可后补
- 一个稳定的 tick 驱动源
- ISR 只做最短路径，剩余工作通过统一调度入口延后

这个阶段最好复用仓库已有内核资产，而不是平地重写：

- `scheduler.cppm`
- `thread.cppm`
- `timer.cppm`
- `sync*.cppm`

需要 Cortex-A 侧补的是：

- 上下文保存/恢复
- 初始线程栈布局
- tick 中断接入
- 异常与调度器之间的边界

这一步做稳以后，我们就不只是“能处理中断”，而是开始拥有“系统运行时”。

## 5.4 阶段 3：最小内存与映射层

当线程模型立住之后，再让内存边界进入主线。

建议优先顺序：

- 固定内核堆
- 页表操作 helper
- 设备映射和普通内存映射的明确区分
- 内核栈观测与越界诊断
- 最小页分配器

当前阶段先不急着做：

- 完整用户态地址空间
- fork/exec 风格进程语义
- 复杂缺页与写时复制

这里的重点是先把“内核自己站在哪块地上”说清楚。

## 5.5 阶段 4：SVC/syscall 边界

这一层不是为了马上变成 Linux，而是为了让“异常入口”和“内核服务入口”不再混用。

当前已经有一版对应的上半层草图：

- `docs/system/minimal_kernel_runtime_service_contract.md`
- `docs/system/minimal_kernel_task_runtime_api_contract.md`
- `docs/system/minimal_kernel_task_syscall_api_contract.md`
- `docs/system/minimal_kernel_task_syscall_catalog_contract.md`
- `docs/system/minimal_kernel_task_syscall_dispatch_contract.md`
- `docs/system/minimal_kernel_task_syscall_table_contract.md`
- `docs/system/minimal_kernel_task_syscall_frame_contract.md`（当前存在历史编码损坏，待恢复）
- `docs/system/minimal_kernel_trap_syscall_contract.md`
- `docs/system/minimal_kernel_trap_ingress_contract.md`
- `docs/system/armv7a_runtime_trap_mapping_contract.md`

建议先做最小 syscall 面：

- `yield`
- `sleep`
- `write` 或最小调试输出
- 简单 capability / handle 调用

更关键的是建立语义：

- 什么算 trap frame
- 参数如何进内核
- 最小 syscall 编号如何映射到 trap service / view
- 最小 syscall request 如何稳定地分派到 transport / handler
- 最小 syscall 号如何组织成静态 handler table
- 最小 numbered syscall frame 如何 decode / writeback
- 返回值和错误码怎样表达
- 失败时如何留下可观测证据

只要这层建立起来，未来不管往 POSIX 走、往 runtime 走、还是往用户任务走，都会轻松很多。

## 5.6 阶段 5：从最小内核走向系统

只有前面几层稳定以后，下面这些方向才值得认真进入主线：

- 用户态任务
- 程序镜像装载
- VFS / 驱动 / shell
- 更完整的 POSIX 兼容
- 多核和更复杂的内存隔离

换句话说：

- “操作系统”当然可以是我们的长期方向
- 但它应该长在稳定的最小内核之上，而不是长在一堆临时 bring-up 代码和宏条件分支之上

## 6. 每一阶段都要留下什么证据

我们已经证明过，“能跑一次”不等于“站稳了”。

所以后面每一层都应该坚持三类证据：

- host 语义验证
- QEMU 运行证据
- CI 可回归脚本

当前这条 trap ingress 映射证据，已经可以先落在独立 host verifier：

- `Examples/kernel/runtime_trap_armv7a_host/`
- `scripts/minimal_kernel_runtime_host_smoke.ps1`
- `scripts/minimal_kernel_runtime_armv7a_qemu_smoke.ps1`

它的作用不是替代 QEMU，而是先验证：

- `Armv7aSvcObservation -> TrapFrameView`
- `TrapFrameView -> RuntimeTrapIngress`
- `TrapResult -> host-local writeback`

可以把它理解成统一方法论：

- host 证明“语义定义是自洽的”
- QEMU 证明“真实执行路径符合语义”
- CI 证明“这不是一次性的偶然成功”

其中 `scripts/minimal_kernel_runtime_host_smoke.ps1` 当前会批量回归上半层 `runtime_*_host` verifier，默认不把 lower-half 认领的 `runtime_task_syscall_frame_armv7a_host` 纳入同一批次，避免两条并行线重新耦合。
而 `scripts/minimal_kernel_runtime_armv7a_qemu_smoke.ps1` 则把 QEMU 叶子里的 `runtime-trap / runtime-live / task-syscall` 聚焦 smoke 收成一条共享 `debug` 构建的 lower-half 回归入口。

如果以后线程、调度器、syscall、页表操作都能保持这个节奏，我们会比很多“先把功能糊上去”的内核项目更稳。

## 7. 两人协作时，怎么分工最不容易互相踩

如果后面开始并行推进，我更推荐下面这种分法。

### 7.1 一个人负责平台叶子与板级映射

适合负责的内容：

- QEMU leaf target 收口
- RK3506 leaf target
- `startup.S` / `vectors.S` / `linker.ld`
- UART/GIC/timer/clock 的板级接线
- toolchain / preset / target 组织
- 板级 bring-up 文档与回归脚本

这部分的特点是：

- 和真实机器更近
- 和 CMake / target 边界更近
- 对 kernel core 的直接改动相对少

### 7.2 一个人负责公共契约与内核核心承接

适合负责的内容：

- `targets/armv7a/common/` 契约继续收口
- host 契约验证器
- 异常/中断/timer 到 scheduler/thread 的 arch ingress seam
- 最小线程与调度运行路径
- trap/syscall 语义
- 最小内存与映射契约

这部分的特点是：

- 更偏“把裸机证据长成系统语义”
- 需要频繁做契约命名、host 验证、QEMU 对齐

### 7.3 共同拥有的接口面

下面这些地方最好持续共同评审：

- 任何新的 contract 命名
- 任何新的 arch hook 形状
- 任何从 leaf 往公共层提升的内容
- 任何新的 QEMU CI 判定语义

因为这些接口一旦定型，后面会影响很多年。

## 8. 如果现在就开始分工，我建议先这样配合

如果要尽快进入并行推进，我更推荐下面这个组合：

- 我继续主攻 `targets/armv7a/common/`、QEMU 证据层、arch ingress seam
- 你优先主攻 CMake/target 组织、board leaf 边界、RK3506 映射、以及和现有仓库主线的对齐

这样分的好处是：

- 写入范围天然更分离
- 不容易在同一批文件里反复打架
- 我们仍然通过 contract 和 CI 汇合，而不是通过“谁先改完某个大文件”汇合

如果你后面想再多接一点裸机代码，最适合并行切入的子题目通常是：

- 新增一个 leaf target 的最小串口/中断接线
- 给某个已有 contract 补 host 验证
- 补一条独立的 QEMU 烟测脚本
- 补 `ArchTimerPort` 或 `ArchInterruptPort` 这类接口的叶子实现

这些任务粒度健康，也最容易避免互相覆盖。

## 9. 当前最值得推进的下一刀

从这份路线图往回落地，当前最值得推进的不是“急着做 syscall”，而是：

1. 把异常/中断/定时器的 arch ingress seam 定形
2. 让现有 kernel scheduler/thread/timer 能在 ARMv7-A 裸机路径上拥有明确入口
3. 继续把 QEMU 和 host 的证据层做成同语义回归

只要这一刀做稳，我们就不再只是“在 Cortex-A 世界里裸奔”，而是已经把“最小内核的门框”立起来了。

## 10. 一句话收口

如果把未来的“Charm 操作系统”想成一座楼，那我们现在不是在装修大厅，而是在把地基、承重墙和竖井位置先钉死。

这一步不喧哗，但它决定了以后楼能盖多高，以及会不会在二楼就开始裂。
