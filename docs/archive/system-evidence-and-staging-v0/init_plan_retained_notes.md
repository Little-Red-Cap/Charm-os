# Init Recipe / Plan 早期取舍保留笔记

> status: `archived`
>
> scope: Recipe、Plan、Barrier 与 materialization 的分层和合并语义

当前接入纪律见 [`init_plan_review_rules.md`](../../system/init_plan_review_rules.md)，Graph 硬规则见
[`init_graph_contract.md`](../../system/init_graph_contract.md)。准确行为以 `Modules/init` source 和 smoke 为准。

## 分层

- driver/core 表达设备或组件行为，不拥有装配 phase/capability；
- binding 将具体 HAL、IRQ、DMA、clock 或 runtime object 绑定给实现；
- `recipe_desc` 描述单个装配单元的 intrinsic name/phase/runlevel/requires/provides 与 start/stop；
- bound recipe 将 recipe 与实例 context 关联；
- Plan 组合 recipe 并施加本次装配约束；
- Materializer 验证并生成 Graph 消费的 `Node` IR。

这些是 init 实现层次，不是 Charm Core 对象模型。`Node` 是 materialized IR，不应成为业务接入 API。

## 约束与产出

Plan 可以向子树传递约束，但不能隐式宣称子树产出。

- effective requires 是 intrinsic requirements 与 inherited requirements 的并集；
- provides 只来自 recipe 自身或显式 barrier；
- runlevel/phase 约束按当前 materializer 规则收窄，越界必须失败；
- trace/budget/debug 等历史候选只有 source 实现后才成立，不能从旧草案推断。

将父 Plan 的 provides 扩散到所有叶子会伪造 provider identity，并可能绕过唯一 provider 检查。

## Barrier

子树“全部完成后提供 capability”必须 materialize 为独立 barrier node。`ready_as<Cap>(plan)` 只是这一
结构的语法表面，不允许改写叶子 recipe 的 intrinsic provides。

Barrier 表达完成顺序，不拥有 runtime object，也不证明子树外部行为正确。其 phase/runlevel 与依赖
必须由 materializer 从实际子树推导并验证。

## Capability 边界

Init capability 用于 provider uniqueness、dependency ordering 和 readiness 校验。它不负责 object
discovery、handle lookup、service locator、runtime ownership 或 hotplug。对象通过 binding 或专题接口
显式传递；动态 discovery 不应伪装成静态 Plan。

## Materializer

Materializer 的局部责任是：

1. 展开组合与可选项；
2. 合并 inherited constraints；
3. 检查 capacity、provider 冲突、缺失依赖、phase/runlevel 与 barrier 合法性；
4. 生成 Node IR、有效过滤参数与观察所需 kind metadata。

它不执行 node、不发现硬件、不拥有 runtime topology，也不能用 materialized graph 证明系统已运行。

旧 HQZY/Player 路径、迁移完成度、模块文件清单和 legacy API 淘汰流水账已删除。
