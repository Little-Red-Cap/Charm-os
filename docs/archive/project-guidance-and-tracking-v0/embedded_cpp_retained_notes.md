# 嵌入式 C++ 早期取舍保留笔记

> status: `archived`
>
> scope: 早期实践指南中仍值得保留的边界问题，不作为当前编码规范

现行规则见 [`embedded-modern-cpp.md`](../../agent/rules/embedded-modern-cpp.md)。本文只保留不能由
“现代 C++ 更好”或全仓禁止清单替代的技术取舍；具体 target、toolchain 和执行路径必须重新核对。

## 执行上下文先于语言偏好

同一种 C++ 设施在不同上下文中的成本不同。ISR、DMA callback、协议 ingress 和调度路径需要有界
延迟；初始化、离线转换和 host 工具可以接受更宽松的内存与标准库策略。判断应回答：

- 代码进入哪些 target，是否会被 MCU 链接；
- 调用发生在 ISR、实时 callback、普通 task、初始化阶段还是 host process；
- 最坏容量、阻塞点、析构时机和失败路径是否可观察；
- host 便利是否泄漏进可移植接口。

固定容量、禁用虚调用或禁用动态内存都不是脱离执行上下文的全仓真理。

## Buffer 与所有权

连续区间优先用 `span` 表达大小和可写性，但 `span` 只解决视图形状，不解决生命周期。异步提交、
DMA 和延迟 callback 必须另外说明：

- buffer 由谁拥有，借用持续到哪个事件；
- 允许谁写，设备与 CPU 的读写方向是什么；
- short read/write、buffer too small 和取消时如何返回；
- 固定容量的上限来自协议、硬件还是项目配置。

裸指针和长度在 C ABI、vendor SDK、MMIO 与 DMA 描述符边界仍然合理，但应在 adapter 内立即转换，
不能让边界表示扩散成业务接口。

## MMIO、DMA 与 Cache

`volatile` 只表达必须保留的硬件可见访问，不提供线程同步、内存所有权或 cache 一致性。DMA buffer
除了地址和长度，还需要平台侧明确：

- 对齐和可访问 memory region；
- clean/invalidate 的方向、时机和覆盖范围；
- descriptor 与 payload 的生命周期；
- 传输完成、超时、中止后的 ownership 归还。

这些约束通常属于 BSP/HAL 或局部执行域，不应包装成平台无关 Core 语义。

## 模板与构造

模板适合表达稳定配置、能力约束和固定维度，不适合把运行期数据搬进类型系统。公共模板需要足够窄
的 `concept` 或 `requires`，使失败发生在接口边界。

转发引用构造函数尤其容易劫持复制或移动构造。保留的最低约束是：

- 默认 `explicit`；
- 排除自身类型；
- 用 concept 限定输入类别；
- 不用一个万能构造函数加大段 `if constexpr` 猜测调用意图。

是否采用模板还应核对实例化数量、Flash 增量、错误可读性和增量构建成本。

## 回调与类型擦除

函数指针、对象加 trampoline、`function_ref`、固定容量 callable 和虚接口都有适用位置。选择依据是
ownership、解绑时机、代码尺寸、最坏调用成本和 ABI，而不是统一偏好。实时路径若使用拥有型 callable，
必须证明它不进行无界分配；跨 C 或 vendor callback 时，`void* context` 应停留在 adapter 内。

## 错误与断言

外部输入、设备状态、资源不足、short IO 和暂时不可用属于运行期失败，应通过专题契约定义的状态或
Result 返回。assert/contract 只适合调用者违反不可恢复前置条件，不能代替正常错误路径。

不依赖异常的 target 需要显式错误传播；host 测试可以使用异常和动态容器，但不得迫使 MCU 可链接
接口采用相同模型。资源获取失败后必须能证明释放、解绑和半初始化状态均被处理。

## Escape Hatch

硬件、ABI 或第三方库迫使实现偏离默认规则时，例外必须：

1. 限制在最小 adapter 或函数范围；
2. 记录真实约束和被拒绝的替代方案；
3. 给出手册、ABI、map/size、benchmark 或生成代码证据；
4. 明确 ownership、失败行为与退出条件；
5. 不污染公共 Core 接口。

当前登记入口见 [`escape_hatches.md`](../../project/escape_hatches.md)。历史“禁止/受限”列表不再保留，
因为它们会把 target 和执行上下文差异错误地压成一套全仓语言政策。
