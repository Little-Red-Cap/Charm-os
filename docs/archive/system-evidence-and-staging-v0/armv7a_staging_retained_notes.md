# ARMv7-A Minimal-kernel Staging 保留笔记

> status: `archived`
>
> scope: 从 bare-metal 证据走向最小内核的历史演进纪律，不作为当前 roadmap

现行机器所有权与 handoff 要求见
[`armv7a_platform_contract.md`](../../system/armv7a_platform_contract.md)。具体 target、runner 和已实现
能力必须核对 source、CMake 与当次 Host/QEMU/真实板证据。

## 不建立第二套内核

ARMv7-A leaf 的任务是提供异常、中断、timer、context 与 MMU/cache 的机器入口，不复制 scheduler、
thread、timer、sync 或 syscall policy。平台无关策略留在 kernel modules；ARMv7-A common 只保存 ISA
级语义；SoC/QEMU/board 常量留在 leaf。

旧草案中的 `ArchExceptionPort`、`ArchInterruptPort`、`ArchTimerPort`、`ArchContextPort` 和
`RuntimeLoopPort` 只是候选切分，不是冻结 API。接口形状必须由当前 kernel consumer 和至少两个 leaf
实现共同证明。

## 历史演进顺序

### 1. Machine bring-up

先证明 reset、mode/stack、vector、abort、IRQ/timer、GIC、MMU/cache/TLB 与 handoff state。串口 token
只能辅助定位，不能替代寄存器、异常帧或状态检查。

### 2. Arch ingress

把 exception/IRQ/tick/context 转换成 kernel 可消费的最小入口，明确 frame ownership、ack/eoi、返回值
writeback 和 barrier。此阶段不需要先承诺线程或用户态 ABI。

### 3. 单核线程

在入口稳定后复用既有 scheduler/thread/timer，先验证固定栈、初始 frame、yield/sleep、tick 与
context switch。cooperative/preemptive 是调度策略选择，不应混进 leaf 常量。

### 4. 内存与映射

线程和 fault 入口可观察后，再引入 kernel region、stack guard、page table helper、device/normal memory
属性和最小 allocator。不要直接跳到完整用户地址空间、fork/COW 或复杂 page fault policy。

### 5. Trap/syscall

SVC ingress、trap frame、number/argument decode、dispatch、writeback 和错误证据应形成一条链。POSIX
facade 或 App ABI 可以消费这条边界，但不能反向定义 arch frame 或 kernel policy。

### 6. 更高系统能力

用户态隔离、image loader、VFS、shell、多核和更宽 POSIX 只有在前述入口与证据稳定后再进入各自专题。
该顺序是风险隔离方法，不表示当前仓库处于某个固定阶段。

## 证据纪律

- Host verifier 证明纯语义、frame mapping 或状态转换，不证明机器入口；
- QEMU 证明对应虚拟机器中的指令、异常、timer 和 MMU/cache 路径，不证明真实 SoC；
- 真实板证明 clock、interconnect、memory timing、controller 与 boot chain；
- CI 证明同一检查可重复，不提升其证据域。

每次提升 leaf 语义到 common/kernel 前，应保留正例、失败例、入口/出口 machine state 与 artifact。
build-only、schema-only 或一次成功日志都不足以证明运行边界。
