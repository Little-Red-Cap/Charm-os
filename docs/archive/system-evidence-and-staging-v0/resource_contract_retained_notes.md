# Resource Contract 早期取舍保留笔记

> status: `archived`
>
> scope: 资源与执行约束的候选维度和审计边界，不定义统一 runtime contract

当前 artifact report 只投影声明、provided facts 和审计结果，见
[`artifact_report_v0.md`](../../system/artifact_report_v0.md)。具体 blocking、allocation、reactor、clock
和 IRQ 约束由所属模块、target 与执行上下文定义。

## 候选维度

早期讨论提出五个标签，它们只能作为审计问题，不能脱离操作和上下文成为全仓布尔真理。

### `may_block`

需要说明哪个操作、在哪个 execution context、允许等待什么以及 timeout/cancel 行为。task 可阻塞不代表
ISR、reactor callback 或 scheduler ingress 可阻塞。

### `needs_heap`

“使用 heap”不足以形成契约；还需 allocator ownership、最大容量、分配阶段、耗尽行为和释放时机。
静态 arena/pool 也有容量失败，不能仅以无 `new` 宣称资源安全。

### `needs_reactor`

应表达语义上依赖 deferred readiness/drain，而不是代码是否 import reactor。具体 reactor、pump budget、
registration lifetime 和 unavailable 行为属于局部 contract。

### `needs_monotonic_clock`

需要说明分辨率、wrap、暂停、跨核可见性和 timeout 比较规则。存在名为 `system.clock` 的 capability
不自动证明这些性质。

### `irq_safe`

IRQ safety 必须绑定到具体操作，并说明 lock-free/critical section、memory ordering、nested IRQ、容量满
和 deferred handoff。一个对象不应被整体标成 irq-safe 后推断所有方法都可在 IRQ 调用。

## 声明、事实与审计

资源解释至少区分：

- declaration：消费者声明的要求；
- provided fact：当前 target/environment 明确提供的条件；
- audit：把两者按已定义规则比较；
- verdict：`satisfied`、`violated` 或 `unknown`。

`unknown` 表示元数据或证据不足，不能当作合法。`satisfied` 只对当前输入、规则和环境成立；fact 必须
保留来源，不能从 board/profile 名称猜测。

## 与其它机制的边界

- init graph 可以限制自己的初始化路径，但不拥有全仓资源法律；
- scheduler/SSU 可以定义执行与阻塞语义，但不推导所有模块的 heap/IRQ 条件；
- capability binding 说明谁满足行为依赖，不自动满足时间、内存或上下文约束；
- artifact report 是只读解释工具，不执行 runtime admission 或资源隔离。

## 强化前的证据

候选标签升级为门禁前，需要真实消费者、正反例、失败行为和至少两个 target/environment 的证据。
能够静态检查的规则应说明覆盖范围；不能证明的部分保持 `unknown`，不要要求模块填充无依据元数据。

RAM/stack map、heap peak、ISR latency、reactor budget 和 clock accuracy 是不同证据，不能压成一个
`resource_ok` verdict。
