# RK3506 异常向量骨架

这一步把 `targets/rk3506` 的异常入口从“除 reset 外全部原地挂起”推进成了“正式进入 ARMv7-A 异常 handler 骨架，再停机诊断”。

## 当前已经具备的能力

- `vectors.S` 现在会为 `undefined / svc / prefetch abort / data abort / reserved / irq / fiq` 建立明确的向量入口。
- 入口会按 ARMv7-A 的异常模式保存一个最小异常帧：
  - `SPSR`
  - `vector_id`
  - `r0-r3`
  - `r12`
  - `lr`
- `targets/rk3506/rk3506_exceptions.cpp` 负责把这些帧转成早期串口日志。
- fatal 路径会再次尝试 early UART 初始化，这样即使异常发生在 `rk3506_boot_main()` 之前，也尽量还能看到板端日志。

## 这一步刻意没有做的事

- 还没有打开真实 GIC IRQ 分发。
- 还没有在 IRQ/FIQ handler 里做 acknowledge / complete。
- 还没有把 generic timer 接到中断路径。
- 还没有引入“多阶段 Bootloader”公开模型。

## 为什么先做这一步

如果直接上 real timer IRQ，而板级异常入口还只是死循环，那么一旦 GIC 路由、优先级、ack 流程或向量返回地址有问题，现场只会变成“板子没反应”。先把异常骨架立住，后面的每一刀都会更容易定位。

## 下一步建议

1. 在 `targets/rk3506` 内部引入最小 GIC acknowledge/complete 路径。
2. 把 generic timer 从只读 smoke 升级成一次性的 timer IRQ 触发。
3. 在 IRQ handler 中先做“只证明中断到达”的最小记录，再决定是否进入更复杂的调度或 tick 语义。
