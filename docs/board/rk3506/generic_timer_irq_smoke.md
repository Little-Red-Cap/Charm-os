# RK3506 Generic Timer IRQ Smoke

这一步把 `RK3506` 从“只读型 generic timer / GIC 探测”推进到了“最小可返回的真实 timer IRQ smoke”。

它的目标不是立刻做系统 tick，也不是把调度器和时钟子系统一起带进来，而是先证明下面这条链路在真板上确实打通了：

`CNTP -> GIC pending -> IRQ entry -> acknowledge -> EOIR -> return`

## 当前这一步做了什么

- 在 [`targets/rk3506/rk3506_platform.cpp`](../../../targets/rk3506/rk3506_platform.cpp) 里补了最小 GIC 路径：
  - 准备 generic timer 相关 PPI
  - 打开 distributor / CPU interface
  - 在 IRQ handler 里做 `IAR` / `EOIR`
- 在 [`targets/rk3506/rk3506_armv7a_state.hpp`](../../../targets/rk3506/rk3506_armv7a_state.hpp) 里补了最小 CP15 helper：
  - `CNTP_CTL`
  - `CNTP_TVAL`
  - `cpsie i / cpsid i`
- 在 [`targets/rk3506/rk3506_exceptions.cpp`](../../../targets/rk3506/rk3506_exceptions.cpp) 里让 IRQ 路径具备“smoke 窗口内可返回”的能力：
  - 如果当前正在执行 timer IRQ smoke，就记录现场并返回主流程
  - 如果不在 smoke 窗口内，IRQ 仍按 fatal 异常处理

## 默认契约

- 当前公开 handoff 模型是 `post-DDR normal-world PL1 payload`
- 在这个契约下，默认期望 generic timer 走的是 `non-secure physical timer`
- 因此 [`targets/rk3506/CharmTargetConfig.cmake`](../../../targets/rk3506/CharmTargetConfig.cmake) 现在把 `CHARM_RK3506_GENERIC_TIMER_EXPECTED_INTID` 默认设为 `30`

也就是说，当前日志里的“expected intid”表达的是平台契约，而不是“唯一允许的第一块板子行为”。

## 为什么仍然保留 29/30 双线容错

虽然默认契约已经显式写成 `intid 30`，但当前 smoke 仍然把下面两条线都识别为“timer source recognized”：

- `29`: secure physical timer PPI
- `30`: non-secure physical timer PPI

这样做是刻意保守：

- bring-up 初期先优先证明 IRQ 链路是活的
- 不因为前级 handoff、安全态差异或板级现状，把第一轮真板实验过早变成“硬失败”
- 等真板日志稳定后，再把“观察结果”和“默认契约”对齐

因此现在的日志会同时给出两类信息：

- `timer source recognized`
- `matches expected intid`

前者回答“是不是 timer 过来的”，后者回答“是不是符合当前默认 handoff 契约”。

## 当前 smoke 流程

1. 准备 timer 相关 PPI 与 GIC 接口
2. 配置 generic timer one-shot
3. 在 IRQ 关闭状态下轮询 `CNTP_CTL.ISTATUS`
4. 打开 IRQ，等待真实进入 IRQ handler
5. 在 handler 中记录：
   - raw acknowledge
   - observed intid
   - handler `CPSR` / `SPSR`
   - return PC
   - GIC controller snapshot
   - observed line snapshot
6. `EOIR` 后返回 [`rk3506_boot_main()`](../../../targets/rk3506/rk3506_bootstrap.cpp)
7. 由启动日志统一输出 smoke 结果

## 当前还没有做的事

- 周期性 timer tick
- FIQ 路径
- 多核 timer route
- 更高层的时钟/调度抽象
- MMU/cache 打开之后的中断一致性收口

## 下一步建议

1. 在真板日志上确认实际打到的是 `29` 还是 `30`
2. 如果真板稳定命中 `30`，就继续沿默认契约推进
3. 如果真板稳定命中 `29`，先判断是 handoff 现实、前级安全态，还是平台代码需要调整
4. 在 timer 路由事实明确后，再推进 MMU/cache/TLB 归一化会更稳
