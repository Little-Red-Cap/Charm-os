# Artifact Report v0

本文不是最终冻结的 JSON Schema，也不是新的导出脚本实现说明。  
它用于定义 Charm 在 `system compiler v0` 阶段的最小统一报告对象：`artifact report`。

当前 schema 草案与最小机器可验样例见：

- `schemas/system_compiler.artifact_report.v0.schema.json`
- `schemas/examples/system_compiler.artifact_report.v0.sample.json`

当前可以直接这样校验样例：

```powershell
python ./scripts/validate_materialized_graph_artifacts.py ./schemas/examples/system_compiler.artifact_report.v0.sample.json
```

它要回答的核心问题不是“有哪些零散导出文件”，而是：

> **当一次系统编译、bringup 举证与资源审计完成后，Charm 应该把哪些核心事实收束成一个可引用对象。**

在当前主线上，`artifact report` 的定位很明确：

- 它不是 explain surface 的替代品
- 它不是底层 bundle / diff / manifest 的替代品
- 它是把这些结果汇总成“系统本次产出了什么结论”的统一工件

## 1. 为什么需要单独定义 `artifact report`

当前仓库已经有很多很强的输出面胚胎：

- `materialized_graph.sample`
- `export_bundle`
- `bundle_diff`
- `ci_summary`
- `report_manifest`
- bringup evidence / resource contract 这两条新文档主线

但它们目前更多回答的是：

- 某个局部工件长什么样
- 某条导出链怎么消费
- 某次 diff 和 CI 摘要如何组织

它们还没有单独回答下面这个更上位的问题：

> **如果把这次系统结果当成一个完整对象，它的最小摘要应该长什么样。**

这正是 `artifact report` 的职责。

没有这一层时，系统结果很容易再次退化成：

- 一堆散文件
- 一堆分散脚本输出
- 一堆只有作者自己知道怎么串起来的观察结果

而有了这一层，Charm 才更像一个真正的 system compiler：

> **它不仅能产工件，还能产“关于这些工件的统一结论对象”。**

## 2. `artifact report` 在输出面里的位置

当前可以把输出面理解为四层：

### 2.1 样例层

- `sample`

用途：

- 字段勘探
- 原型接入
- 快速结构观察

### 2.2 工件组织层

- `bundle`
- `report manifest`

用途：

- 工件归档
- 多文件组织
- 报告文件发现

### 2.3 比较与 CI 层

- `bundle diff`
- `ci summary`

用途：

- 增量变化分析
- CI 摘要
- 自动化状态汇总

### 2.4 汇总结论层

- `artifact report`

用途：

- 把静态结构、bringup 证据、资源审计与支持工件收束成统一摘要对象

因此 `artifact report` 更像：

> **report-of-reports**

它消费已有工件，但不试图取代这些工件。

## 3. v0 明确不做什么

当前版本明确不做：

- 不试图一份报告塞下所有细节
- 不复制所有底层工件全文
- 不承诺当前字段已经是长期冻结协议
- 不要求所有子系统一次性接入
- 不跳过既有 `bundle / diff / manifest` 体系重做一套新管线

当前更重要的是先把这件事做对：

> **定义出一个足够小、足够稳、足够可引用的最小统一报告对象。**

## 4. v0 的最小对象边界

当前建议把 `artifact report v0` 理解为一个只读汇总对象。  
它应满足下面几个特征：

- 以“这次系统结果”为中心，而不是以单个工件为中心
- 只保留最关键的摘要字段
- 引用底层工件，而不是吞掉底层工件
- 同时覆盖静态装配、bringup 证据与资源审计三张面
- 让上层 explain surface 有稳定输入锚点

换句话说，它不应是“大而全数据库”，  
而应是：

> **最小系统结论页。**

## 5. v0 建议字段分组

当前建议把 `artifact report` 分成七组字段。

### 5.1 报告身份

这一组回答：

> “这份报告是谁、什么时候、按什么模式生成的。”

建议至少包含：

- `schema`
- `generated_at_utc`
- `generator`
- `report_kind`
- `mode`

其中：

- `report_kind`
  当前可先固定为 `system_compiler.artifact_report`
- `mode`
  可用于区分：
  - `export_only`
  - `compare`
  - 其它未来模式

### 5.2 系统上下文

这一组回答：

> “这份报告针对的是哪个系统实例。”

建议至少包含：

- `case`
- `profile`
- `board`
- `active_facets`

这里不要求所有字段立即 100% 完备，  
但 v0 应先把这些对象正式列为报告语言的一部分。

### 5.3 静态结构摘要

这一组回答：

> “系统静态装配之后，长成了什么样。”

建议至少包含：

- `capability_count`
- `node_count`
- `edge_count`
- `materialized_order`
- `required_facts`
- `unresolved_bindings`

其中：

- `materialized_order`
  可先只放节点顺序摘要或引用外部工件
- `required_facts`
  应是 system compiler 主线里的第一类核心输入事实
- `unresolved_bindings`
  则是最关键的未完成结构结论之一

### 5.4 bringup 证据摘要

这一组回答：

> “bringup 过程在证据语言里当前是什么结论。”

建议至少包含：

- `declared_count`
- `materialized_count`
- `published_count`
- `observed_count`
- `blocked_count`
- `failed_count`

以及按需包含：

- `published_capabilities`
- `blocked_reasons`
- `failed_reasons`

这里不要求 v0 就把所有节点细节内嵌进报告，  
但应让顶层摘要一眼能看出：

> 当前 bringup 问题到底是“没成立”、还是“成立了但没发布”、还是“已经失败”。 

### 5.5 资源契约摘要

这一组回答：

> “系统在当前资源宇宙里，合法性结论如何。”

建议至少包含：

- `declared_contracts`
- `provided_facts`
- `audited_count`
- `satisfied_count`
- `violated_count`
- `unknown_count`

以及按需包含：

- `violations`
- `unknown_contracts`
- `resource_hotspots`

这里建议特别优先覆盖：

- `may_block`
- `needs_heap`
- `needs_reactor`
- `needs_monotonic_clock`
- `irq_safe`

### 5.6 运行时观察摘要

这一组回答：

> “当前有哪些最小 runtime 事实已经进入稳定观察面。”

建议至少包含：

- `publish_state_summary`
- `export_state_summary`
- `recent_transitions`

这组字段当前不宜做得过大，  
因为 v0 还不是完整 runtime inspector。  
但它至少应该让报告能引用：

- `PublishState`
- `ExportState`
- `ExportTransition`

这些已经存在的观察语言。

### 5.7 支持工件引用

这一组回答：

> “如果我要继续深挖，应该去看哪些底层工件。”

建议至少包含：

- `bundle`
- `dot`
- `sample_json`
- `diff`
- `ci_summary`
- `report_manifest`

其中每个引用建议都以：

- 相对路径
- 或可解析引用键

形式出现，而不是把大块内容直接内嵌进顶层报告。

## 6. v0 的最小样例形状

当前更适合先用一个“语义样例”来固定对象边界，而不是急着冻结正式 schema。

一个收敛后的最小样例可以长这样：

```json
{
  "schema": "system_compiler.artifact_report/v0",
  "generated_at_utc": "2026-04-15T10:30:00Z",
  "generator": "charm.system_compiler",
  "report_kind": "system_compiler.artifact_report",
  "mode": "export_only",
  "subject": {
    "case": "bringup-minimal-observe-demo",
    "profile": "MCU_MIN",
    "board": "stm32_stub",
    "active_facets": ["runtime", "input"]
  },
  "structure": {
    "capability_count": 12,
    "node_count": 9,
    "edge_count": 8,
    "required_facts": ["platform.irq", "system.clock"],
    "unresolved_bindings": []
  },
  "bringup_evidence": {
    "declared_count": 12,
    "materialized_count": 12,
    "published_count": 3,
    "observed_count": 3,
    "blocked_count": 0,
    "failed_count": 0
  },
  "resource_contract": {
    "declared_contracts": 4,
    "provided_facts": ["system.clock", "reactor", "task_context"],
    "audited_count": 4,
    "satisfied_count": 4,
    "violated_count": 0,
    "unknown_count": 0
  },
  "runtime_observe": {
    "publish_state_summary": {"published": 3, "missing": 0},
    "export_state_summary": {"attached": 1, "detached": 2, "missing": 0},
    "recent_transitions": [
      {
        "capability": "io.uart1",
        "action": "attach",
        "before": "detached",
        "after": "attached"
      }
    ]
  },
  "artifacts": {
    "bundle": "out/materialized-graph-bundle/index.json",
    "dot": "out/materialized-graph-bundle/case/materialized_graph.dot",
    "sample_json": "out/materialized-graph-bundle/case/materialized_graph.sample.json",
    "diff": null,
    "ci_summary": null,
    "report_manifest": "out/report/materialized_graph_bundle_diff_report.manifest.json"
  }
}
```

这个样例的重点不是字段名已经最终拍板，  
而是先把对象轮廓固定住：

- 顶层是统一报告对象
- 中层是若干摘要分组
- 底层通过 `artifacts` 继续引用原始工件

## 7. 与其它文档的关系

### 7.1 与 `explain_surface_v0`

`artifact report` 是 explain surface 的重要输入之一，  
但两者不等价。

- `artifact report`
  解决“系统当前产出了什么结论对象”
- `explain surface`
  解决“人和工具如何继续追问这些结论”

### 7.2 与 `bringup_evidence_pipeline_v0`

bringup 文档负责定义：

- 状态语言
- 胚胎映射
- 证据边界

而 `artifact report` 负责把这些状态压成：

- 顶层摘要字段
- 可引用结论对象

### 7.3 与 `resource_contract_v0`

资源契约文档负责定义：

- 法律文本
- 审计语言
- 合法性边界

而 `artifact report` 负责把这些审计结果压成：

- `satisfied / violated / unknown` 摘要
- 可进一步追问的热点入口

### 7.4 与现有 `bundle / diff / report manifest`

`artifact report` 应消费这些工件，  
而不应跳过它们另起一套闭环。

这条边界必须守住，因为它能保证：

- 现有工具链继续有价值
- 报告体系逐步长成，而不是重来
- explain surface 有稳定工件锚点

## 8. v0 的工程边界

当前最健康的推进方式是：

1. 先冻结对象边界
2. 先冻结最小摘要字段
3. 先用文档和样例固定语义
4. 再决定哪些字段值得进入真实 schema
5. 再决定哪些摘要值得接进 CI / 工具链

近期不建议：

- 一上来把它做成大而全总报告
- 一上来要求所有底层模块都直接生成这份报告
- 过早承诺所有字段长期稳定
- 把它做成对现有脚本链不兼容的新世界

v0 更合理的判断标准是：

> **当我们拿到一次系统导出结果时，是否能通过一份统一对象先看懂“这次结果最重要的事实是什么”，然后再顺着引用继续深挖。**

## 9. 当前结论

`Artifact Report v0` 当前应被理解成 system compiler 的最小“结论对象”，而不是新的大平台。

它的近期使命很克制：

- 不替代底层工件
- 不替代 explain surface
- 不替代 bringup 与资源契约文档

它只做一件很关键的事：

> **把分散的结构、证据、合法性与观察结果，先收成一个稳定可引用的系统结论页。**
