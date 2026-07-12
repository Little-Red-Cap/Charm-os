# init.materialize 观察工具链阶段里程碑

> 状态：archived。本文是早期 materialized graph 工具链的阶段复盘；其中能力与完成度声明
> 不代表当前事实，使用前必须回查源码、schema 与当次 smoke。现行 init.graph 规则见
> [`../../system/init_graph_contract.md`](../../system/init_graph_contract.md)。

本文档记录 `materialized_graph` 观察导出线在当前阶段已经形成的能力闭环、方法论意义与下一步演进方向。

它不是 `init.observe` 的 API 手册，
也不是 `Recipe / Plan / Materialize` 的设计草案，
而是回答：

> **我们已经把这条线推进到了什么程度？它真正证明了什么？**

---

## 本阶段主题

这一阶段的主题，不是继续发明新的装配表面，
而是让 `materialized_graph` 从“内部归一化结果”进一步变成：

- 可观察的对象
- 可导出的对象
- 可比较的对象
- 可报告的对象
- 可被 CI 与工具稳定消费的对象

换句话说，这一阶段不是在扩 DSL，
而是在为“显式系统秩序”建立工具后端。

---

## 当前已经形成的能力链

截至当前阶段，围绕 `materialized_graph` 已经形成了下面这条完整能力链：

### 1. 观察与导出

- `init.observe` 提供只读语义视图
- `format_dot(...)` 导出结构观察图
- `format_json_sample(...)` 导出样例 JSON

这一层解决的是：

> `materialize(...)` 的结果已经不再只能停留在内存里被框架自己消费。

### 2. 批量导出

- `scripts/export_materialized_graph.ps1`
- `scripts/materialized_graph.export_case_manifest.v1.json`
- 多 case bundle
- `index.json`

这一层解决的是：

> 观察结果开始可以被组织成批量工件，而且 case 输入事实也开始脱离脚本内硬编码，并在 bundle 顶层保留输入 provenance。

### 3. 批量检视与差异比较

- `scripts/inspect_materialized_graph_bundle.ps1`
- `scripts/diff_materialized_graph_bundle.ps1`
- `materialized_graph.bundle_diff/v1`

这一层解决的是：

> 不同装配结果之间的差异开始可以被机器稳定比较，而不再依赖人肉对照入口代码，同时 compare 面也开始保留输入 provenance。

### 4. 面向人的报告层

- `scripts/report_materialized_graph_bundle.ps1`
- Markdown / HTML 报告
- `materialized_graph.report_manifest/v1`

这一层解决的是：

> diff 结果不仅可以被脚本吃，也可以被人高效审阅。

### 5. 面向自动化的 CI 层

- `scripts/ci_materialized_graph_bundle.ps1`
- `materialized_graph.ci_summary/v1`
- `.github/workflows/materialized-graph-observe.yml`

这一层解决的是：

> 这条能力链已经能进入持续集成，而不是只停留在开发者本地习惯。

### 6. 协议与自动验证

- `schemas/materialized_graph.sample.v2.schema.json`
- `schemas/materialized_graph.export_case_manifest.v1.schema.json`
- `schemas/materialized_graph.export_bundle.v1.schema.json`
- `schemas/materialized_graph.bundle_diff.v1.schema.json`
- `schemas/materialized_graph.report_manifest.v1.schema.json`
- `schemas/materialized_graph.ci_summary.v1.schema.json`
- `scripts/validate_materialized_graph_artifacts.py`

这一层解决的是：

> 这条链上的关键 JSON 协议已经不是“文档口头约定”，而是机器可验证的正式接口。

---

## 这一阶段真正证明了什么

从方法论角度看，
这一阶段最重要的结论不是“我们做了一堆脚本”，
而是下面三件事开始同时成立。

### 1. 系统秩序已经能被抽离成独立对象

当前围绕 `materialized_graph` 建立的观察链，
证明了 capability、依赖边、phase、runlevel、barrier 这类关系，
已经可以作为独立于平台入口代码的对象存在。

这意味着：

> “系统长什么样”开始可以在平台之前被描述，也可以在执行之前被观察。

### 2. 系统同一性开始能被结构化比较

过去谈“同一系统在不同平台或不同装配下是否还是同一个系统”，
更多依赖工程直觉和代码阅读。

现在至少在当前覆盖范围内，
这个问题已经开始可以被改写成：

- `materialized_graph` 是否同构
- 哪些 case 有变化
- 是节点变化、依赖变化、phase 变化还是 runlevel 变化

这使“同一性”第一次开始变成结构问题，而不是感觉问题。

### 3. 工具链已经拥有统一消费后端

现在脚本、报告、CI、工作流都不再直接围绕某个入口文件或某个 `Node[]` 拼装细节打补丁，
而是围绕统一的 `materialized_graph` 结果和它的衍生协议工作。

这意味着：

> `materialized_graph` 已经开始承担“统一中间语”的角色。

这离最终平台化还很远，
但已经不再只是理论上的可能性。

---

## 为什么这一步值钱

这一步值钱，不是因为现在报告更漂亮了，
而是因为它把之前那条方法论判断推进成了工程事实：

> **系统秩序可以被显式化、可以被工具消费、可以被自动化验证。**

这件事一旦成立，后面的很多能力才有真正的落点，例如：

- profile 差异比较
- board package 差异比较
- profile / board 迁移辅助
- IDE 结构视图
- 可视化审阅
- 构建前结构检查

如果没有这一阶段的工作，
这些都仍然只能停留在概念层。

---

## 当前边界

这一阶段虽然已经形成闭环，
但仍然必须清楚它的边界。

### 1. `sample/v2` 仍然不是长期冻结协议

当前已经为它提供了 schema 与消费侧校验，
但这代表的是：

- 它是“当前受支持的样例协议”
- 不是“永久不变的最终对外协议”

### 2. 当前覆盖的真实 case 还不够广

现在导出链已经覆盖：

- `materialize_observe_demo`
- `bringup_block_observe_demo`
- `bringup_minimal_observe_demo`
- `usb_msc_block_demo`

这些案例已经足够证明链路成立，
但还不足以代表全部 profile / board / 真实产品路径。

### 3. 还没有回到“输入语义树”这一层

当前工具消费面刻意挂在 `materialized_graph` 之后，
这是对的，
因为它保证了消费的是归一化结果。

但这也意味着：

- 当前更擅长比较“落成后的秩序结果”
- 还不擅长比较“输入阶段的 Plan 语义结构”

这部分如果以后要做，应该是下一阶段工作，而不是在这一阶段反绑当前协议。

---

## 当前阶段的工程结论

如果把这阶段压成一句工程结论，我会这样写：

> **`materialized_graph` 已经从内部 IR 结果，推进成了可导出、可比较、可审阅、可进入 CI、可被 schema 验证的统一中间后端。**

这不是终局，
但已经足以说明：

> Charm 正在把“系统秩序”从工程习惯推进成工具可消费的结构对象。

---

## 下一阶段建议

当前最值得继续推进的方向有四个。

### 1. 扩大真实 case 覆盖面

优先把更多真实 bringup / profile / board case 接入导出链，
尤其是更接近 `player`、更接近真实产品路径的案例。

### 2. 收敛长期稳定协议边界

当前 schema 已经落地，
下一步要继续回答：

- 哪些字段会成为长期承诺
- 哪些字段仍然只是样例层勘探字段
- `sample` 如何向更稳定的外部协议收敛

### 3. 增加更高层的比较视角

例如：

- 同一系统在不同 profile 下的差异
- 同一 profile 在不同 board package 下的差异
- 同一 bundle 在不同提交之间的演化趋势

### 4. 为上层工具接入做准备

当 schema、diff、report、CI 都稳定后，
下一步就可以更自然地接入：

- 可视化前端
- IDE 面板
- 审阅助手
- 迁移辅助工具

---

## 一句话里程碑结论

如果只保留一句话来描述这一阶段，我会写：

> **Charm 已经初步把 `materialized_graph` 建成“系统秩序的统一观察后端”，并让它第一次同时服务人、脚本、CI 与未来工具链。**

