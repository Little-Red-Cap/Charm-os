# RTE Capability Composition v0 摘要

> **文档状态：`exploration`（停线冻结）**

完整提案保留在
[`../archive/architecture-exploration-v0/rte_capability_composition_contract_v0.md`](../archive/architecture-exploration-v0/rte_capability_composition_contract_v0.md)。
`RTE` 当前裁决为 `Rejected / Deferred`，不进入 Core 词汇。

## 提案内容

原提案把 component requirements、provider provisions 和 profile bindings 组织成多个投影：

- init projection；
- runtime/context projection；
- host projection；
- evidence/explain projection。

这组投影可作为未来工具设计的讨论材料，但不能被称为已经存在的 RTE runtime 或统一对象模型。

## 当前源码事实

仓库中实际存在的是 `init.recipe`、`init.plan` 和 `init.materialize` 等初始化装配模块；
未发现独立 `RTE` 实现、跨环境 RTE ABI 或由该提案驱动的统一 generator。已有能力、绑定和接口仍须按各自现行契约解释。

## 使用规则

不得新增 `RTE` manager、service locator、公共基类或 manifest 作为本提案的默认实现。若重新推进，先拆出一个可独立验证的最小关系，并证明它不能由现有 Requirement/Provision/Binding 表达。
