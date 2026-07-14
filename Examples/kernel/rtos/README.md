# RTOS 示例入口

> `status`: `supporting`
>
> `scope`: `charm.system.rtos` 的局部 Host/QEMU fixture 路由

当前 scheduler、task/ISR context、wait、lifecycle 与未证明边界见
[`RTOS Runtime Contract`](../../../docs/system/rtos_runtime_contract.md)。Cortex-M7 QEMU 构建、运行和证据范围
见 [`qemu/README.md`](qemu/README.md)。

示例只证明各自 runner 覆盖的语义，不定义跨 target RTOS ABI、调度策略或实时上限。
