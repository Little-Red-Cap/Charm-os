# Resource Contract v0 摘要

> **文档状态：`exploration`（未形成统一契约）**

完整提案见
[`../archive/system-evidence-and-staging-v0/resource_contract_v0.md`](../archive/system-evidence-and-staging-v0/resource_contract_v0.md)。

## 提案范围

原提案尝试统一描述：

- `may_block`；
- `needs_heap`；
- `needs_reactor`；
- `needs_monotonic_clock`；
- `irq_safe`。

这些问题真实存在，但目前只分散在 SSU、reactor、queue、init 规则和报告脚本中。仓库没有统一 `ResourceContract` C++ API、schema、静态检查器或运行时执法器。

## 当前使用规则

- 不把报告字段当成已经执行的资源法律；
- 不从 capability 装配成功推断 blocking、heap、IRQ 或 clock 要求已满足；
- 具体模块仍由自身契约说明执行上下文和资源限制；
- `resource_contract` 报告只能作为审计 sidecar，不能改变 binding 或运行时行为。

## 重新推进条件

先选择一个真实 consumer 和一条可证伪规则，例如“IRQ 路径不得阻塞”，给出静态或运行时检查及正反 smoke；不要一次冻结五种资源维度或新增全局资源世界模型。
