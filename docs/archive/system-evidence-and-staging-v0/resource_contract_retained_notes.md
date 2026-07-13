# Resource Contract 早期取舍保留笔记

> `status`: `archived`

当前 artifact report 只投影声明、provided facts 和审计结果，见
[`artifact_report_v0.md`](../../system/artifact_report_v0.md)。具体资源约束由所属 module、target 与
execution context 定义。

## 候选审计维度

- `may_block`：绑定具体操作和 context，并说明等待对象、timeout 与 cancellation。
- `needs_heap`：说明 allocator owner、最大容量、分配阶段、耗尽和释放；arena/pool 同样有容量失败。
- `needs_reactor`：表达 deferred readiness/drain 依赖，并说明 reactor、pump budget、registration
  lifetime 和 unavailable 行为。
- `needs_monotonic_clock`：说明 resolution、wrap、pause、跨核可见性和 timeout comparison。
- `irq_safe`：绑定具体操作，并说明 lock/critical section、memory ordering、nested IRQ、容量满和
  deferred handoff。

这些标签是审计问题，不是脱离操作和上下文的全仓布尔属性。

## 声明、事实与 Verdict

资源解释必须区分 consumer declaration、target/environment provided fact、审计规则和
`satisfied/violated/unknown` verdict。Fact 保留来源，不能从 board/profile 名称猜测；`unknown` 表示
证据不足，不能当作满足。`satisfied` 也只对当前输入、规则和环境成立。

Init graph、scheduler/SSU 和 capability binding 各自约束局部行为，不拥有全仓资源法律；artifact report
只读解释，不执行 runtime admission 或隔离。

## 强化条件

标签进入门禁前需要真实 consumer、正反例、失败行为和至少两个 target/environment 的证据。静态检查
必须声明覆盖范围，无法证明的部分保持 `unknown`。RAM/stack map、heap peak、ISR latency、reactor budget
和 clock accuracy 是独立证据，不能合并为单个 `resource_ok`。
