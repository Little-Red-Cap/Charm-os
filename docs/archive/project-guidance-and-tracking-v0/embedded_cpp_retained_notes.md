# 嵌入式 C++ 早期取舍保留笔记

> `status`: `archived`

现行规则见 [`embedded-modern-cpp.md`](../../agent/rules/embedded-modern-cpp.md)。本文只保留依赖 target、
执行上下文和硬件边界的技术取舍，不作为全仓禁止清单。

## Execution Context

选择语言设施前必须明确：

- 代码进入哪些 MCU/Host target；
- 调用发生在 ISR、DMA callback、实时 task、初始化还是 host process；
- 最坏容量、阻塞点、析构时机和失败路径；
- Host 便利是否泄漏到可移植接口。

固定容量、禁用虚调用或禁用动态内存都不是脱离执行上下文的全仓规则。

## Buffer、DMA 与 Cache

- `span` 表达视图，不解决生命周期。异步提交必须说明 owner、借用终点、读写方向、取消和 short IO。
- 裸 pointer+length 可留在 C ABI、vendor SDK、MMIO 与 DMA descriptor adapter，不能扩散到业务接口。
- `volatile` 不提供线程同步或 cache coherency。
- DMA 必须明确 alignment、memory region、clean/invalidate 方向与范围、descriptor/payload lifetime，
  以及 complete/timeout/abort 后的 ownership 归还。

这些约束属于 BSP/HAL 或局部 execution domain，不提升为平台无关 Core 语义。

## Template 与 Callback

Template 适合稳定配置、能力约束和固定维度，不把运行数据搬进类型系统。公共 template 应使用窄 concept；
转发构造默认 `explicit`、排除自身类型，并避免一个万能构造函数用 `if constexpr` 猜测意图。还需检查
实例化数量、Flash 增量、诊断可读性和增量构建成本。

函数指针、trampoline、`function_ref`、固定容量 callable 和虚接口按 ownership、解绑、ABI、代码尺寸与
最坏调用成本选择。实时路径中的 owning callable 必须证明无无界分配；`void* context` 停留在 adapter。

## Error Boundary

外部输入、设备状态、资源不足、short IO 和暂时不可用是运行期错误，应返回专题契约定义的状态。
assert/contract 只处理不可恢复的调用者违约。不使用异常的 target 要显式传播错误并处理释放、解绑与
半初始化状态；Host 测试可以使用异常，但不能迫使 MCU 接口采用同一模型。

## Escape Hatch

硬件、ABI 或第三方约束要求例外时，必须限制在最小 adapter 范围，记录被拒绝方案与手册/map/benchmark
等证据，并明确 ownership、失败行为和退出条件。例外不得污染 Core 接口；登记入口见
[`escape_hatches.md`](../../project/escape_hatches.md)。
