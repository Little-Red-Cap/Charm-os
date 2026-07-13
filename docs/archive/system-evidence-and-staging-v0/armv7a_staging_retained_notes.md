# ARMv7-A Minimal-kernel Staging 保留笔记

> `status`: `archived`

现行机器 ownership 与 handoff 见
[`armv7a_platform_contract.md`](../../system/armv7a_platform_contract.md)。本文只保留 bare-metal 到
minimal-kernel 的风险隔离顺序，不表示当前 roadmap 或实现阶段。

## Ownership

ARMv7-A leaf 提供 exception、interrupt、timer、context 与 MMU/cache 的机器入口，不复制 scheduler、
thread、sync 或 syscall policy。平台无关策略属于 kernel modules，ISA 语义属于 ARMv7-A common，
SoC/QEMU/board 地址与时钟属于 leaf。

旧 `Arch*Port` 和 `RuntimeLoopPort` 只是候选切分。接口必须由当前 kernel consumer 与至少两个 leaf
实现共同证明，不能从历史名称恢复 API。

## 风险隔离顺序

1. **Machine bring-up**：reset、mode/stack、vector、abort、IRQ/timer、GIC、MMU/cache/TLB 与 handoff。
2. **Arch ingress**：frame ownership、ack/eoi、writeback 和 barrier，形成 kernel 可消费的最小入口。
3. **Single-core thread**：复用 scheduler/thread/timer，验证 fixed stack、initial frame、yield/sleep、tick
   与 context switch；preemption 是 policy，不是 leaf 常量。
4. **Memory mapping**：在 thread/fault 可观察后加入 region、stack guard、page table、memory attribute 与
   最小 allocator，不直接跳到 fork/COW 或完整用户地址空间。
5. **Trap/syscall**：贯通 SVC ingress、frame、decode、dispatch、writeback 与错误证据；POSIX/App ABI 不
   反向定义 arch frame 或 kernel policy。
6. **Higher layers**：isolation、image loader、VFS、shell、multicore 和更宽 POSIX 分别进入专题。

该顺序只隔离风险，不表示仓库处于固定阶段。串口 token 可定位故障，不能替代寄存器、异常帧或机器
状态检查。

## Evidence Discipline

- Host verifier 证明纯语义、frame mapping 或状态转换，不证明机器入口。
- QEMU 证明虚拟机器中的指令、exception、timer 和 MMU/cache，不证明真实 SoC。
- Real board 证明 clock、interconnect、memory timing、controller 与 boot chain。
- CI 证明检查可重复，不扩大其证据域。

Leaf 语义提升到 common/kernel 前应保留正例、失败例、入口/出口 machine state 与 artifact。
Build-only、schema-only 或单次成功日志不足以证明运行边界。
