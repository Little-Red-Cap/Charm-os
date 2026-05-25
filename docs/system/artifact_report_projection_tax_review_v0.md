# Artifact Report 重复事实体检 v0

本文不是对 GMP 的读书笔记，也不是新的导出实现说明。  
它用于钉住 Charm 当前 `artifact report` 相关链路里，哪些系统事实已经在多个平面被手工镜像，哪些重复仍属合理投影，哪些重复已经开始演变成维护税。

它要回答的核心问题不是“有没有元编程工具可用”，而是：

> **当前 `artifact report` 这一条链里，哪些事实已经值得被提升为单一事实源。**

## 1. 为什么现在要先做体检

`docs/architecture/system_compiler_roadmap.md` 已经明确：

- Charm 当前主任务是把“系统如何成立”编译出来
- `system compiler v0` 优先做解释物，而不是大规模 code generation
- `artifact report` 已经是现行结果物主轴之一

问题在于，`artifact report` 现在已经不只是一个 JSON 文件。  
它同时牵动：

- 正式 schema
- summary schema
- export 脚本组装
- inspect / explain 消费面
- 对应文档术语

一旦这些平面共享同一批高价值字段，风险就不再是“多写几行”，而是：

> **同一个系统事实开始在多个地方被手工同步。**

这正是当前最值得先识别的重复事实税。

## 2. 观察范围

本轮体检只看 `artifact report` 及其直接投影链，不讨论更大范围的全仓反射。

当前重点观察面固定为四类：

- 正式 schema 面  
  例如：`schemas/system_compiler.artifact_report.v0.schema.json`
- summary schema 面  
  例如：`schemas/system_input_summary.v0.schema.json`、`schemas/binding_result_summary.v0.schema.json`、`schemas/bringup_order_summary.v0.schema.json`、`schemas/system_compiler_summary.v0.schema.json`
- export 脚本组装面  
  例如：`scripts/export_system_compiler_artifact_report.ps1`
- inspect / explain 消费面  
  例如：`scripts/inspect_system_compiler_artifact_report.ps1` 与 `docs/system/explain_surface_v0.md`

## 3. 当前高重复字段族

### 3.1 `subject` 族

当前核心字段：

- `case`
- `profile`
- `board`
- `active_facets`

当前重复路径很明显：

- `artifact report` 顶层 `subject`
- `system_input_summary` 中的 case / subject 上下文
- `binding_result_summary` 的 case 视图
- `bringup_order_summary` 的 case 视图
- `system_compiler_summary` 的 case context
- `inspect` 默认摘要与 `-ListCases` 一类消费面
- export 脚本中的 subject 解析、默认值解析、resolved subject 组装

当前最直接的仓库锚点包括：

- `schemas/system_compiler.artifact_report.v0.schema.json`
- `schemas/system_input_summary.v0.schema.json`
- `schemas/binding_result_summary.v0.schema.json`
- `schemas/bringup_order_summary.v0.schema.json`
- `schemas/system_compiler_summary.v0.schema.json`
- `scripts/export_system_compiler_artifact_report.ps1`
- `scripts/inspect_system_compiler_artifact_report.ps1`

这里的重复有两类：

- 合理投影  
  不同 summary 对同一 subject 做不同视角切片，这是正常的
- 手工同步税  
  `profile / board / active_facets` 的形状、空值规则、来源解释和 case context 现在已在多个对象里重复手写

当前判断：

> `subject` 族已经是第一批最适合收敛为单一事实源的字段。

### 3.2 `system_input` 族

当前核心字段：

- `declared_input.subject`
- `declared_facts`
- `declared_contract_entries`
- `resolved_input.profile`
- `resolved_input.board`
- `resolved_input.active_facets`
- `subject_facts`

当前重复路径：

- `artifact report.system_input`
- `system_input_summary`
- `system_compiler_summary` 中的 formation basis / subject context
- export 脚本中的 `New-SystemInputSummary`、comparison side、change summary 组装
- `artifact_report_v0` 与 `system_compiler_vocabulary_v0` 文档中的输入语言说明

当前最直接的仓库锚点包括：

- `schemas/system_compiler.artifact_report.v0.schema.json`
- `schemas/system_input_summary.v0.schema.json`
- `schemas/system_compiler_summary.v0.schema.json`
- `scripts/export_system_compiler_artifact_report.ps1`
- `docs/system/artifact_report_v0.md`
- `docs/architecture/system_compiler_vocabulary_v0.md`

这里的风险比 `subject` 更高，因为它不仅是值，还带有：

- declared / resolved 的分层
- source / provenance
- compare mode 漂移文本

当前判断：

- 这组字段非常值得统一
- 但它比 `subject` 更接近“输入语言层”
- 第一试点可以先借它的 `subject` 子集，不建议马上全量纳入

### 3.3 `binding_result + bringup_order` 计数族

当前核心字段：

- `required_binding_count`
- `resolved_binding_count`
- `unresolved_binding_count`
- `ordered_node_count`
- `blocked_node_count`

以及它们的近邻字段：

- `unresolved_capabilities`
- `blocked_nodes`
- `phase_counts`

当前重复路径：

- `artifact report.binding_result`
- `artifact report.bringup_order`
- `system_formation.binding_summary`
- `system_formation.bringup_summary`
- `binding_result_summary`
- `bringup_order_summary`
- `system_compiler_summary`
- export 脚本中的 summary、comparison、system formation 汇总
- inspect 面中的 headline / listcases / drift 聚合

当前最直接的仓库锚点包括：

- `schemas/system_compiler.artifact_report.v0.schema.json`
- `schemas/binding_result_summary.v0.schema.json`
- `schemas/bringup_order_summary.v0.schema.json`
- `schemas/system_compiler_summary.v0.schema.json`
- `scripts/export_system_compiler_artifact_report.ps1`
- `scripts/inspect_system_compiler_artifact_report.ps1`

这里的重复已经不仅是“同名字段多处出现”，而是：

> **同一批计数先在局部 summary 出现，再被 `system_formation` 二次汇总，再被 `system_compiler_summary` 三次投影。**

这说明它们已经具备非常强的“单一事实源候选”特征。

当前判断：

> `binding / bringup` 计数族与 `subject` 一样，适合作为第一试点。

### 3.4 `resource_contract + fact_resolution` 计数族

当前核心字段：

- `declared_contracts`
- `audited_count`
- `satisfied_count`
- `violated_count`
- `unknown_count`

以及近邻字段：

- `provided_facts`
- `resource_hotspots`
- `fact_inventory`
- `required_fact_resolution`

当前重复路径：

- `artifact report.resource_contract`
- `artifact report.fact_resolution`
- `resource_contract_summary`
- `fact_resolution_summary`
- compare side summary / state map
- export 脚本中的 contract summary、fact inventory、热点推导
- `resource_contract_v0` 文档中的法律文本

当前最直接的仓库锚点包括：

- `schemas/system_compiler.artifact_report.v0.schema.json`
- `schemas/fact_resolution_summary.v0.schema.json`
- `scripts/export_system_compiler_artifact_report.ps1`
- `docs/system/resource_contract_v0.md`

这组字段的问题不是“不重要”，而是它们当前更深地耦合了：

- declared facts
- audit provided facts
- contract state
- hotspot 推导
- compare drift

当前判断：

- 这组字段也已经有重复事实税
- 但它们更像“合法性层联动对象”
- 如果第一波就把它们也拉进试点，复杂度会明显上升

默认处理：

> `resource_contract / fact_resolution` 进入第二波，而不是第一波。

## 4. 哪些重复仍然合理

不是所有重复都该消灭。

当前仍应视为合理投影的重复包括：

- 同一事实在 full report 与 summary report 的不同裁剪
- 同一事实在 export object 与 inspect 查询面中的不同展示
- compare mode 对 baseline / candidate 的双侧展开
- 文档中为了解释词义而出现的字段名引用

这些重复的问题不在“出现多次”，而在于：

- 是否同源
- 是否可以稳定投影
- 是否需要人工同步字段语义

所以本轮判断不是“去重一切”，而是：

> **把手工同步的重复，改造成受控投影。**

## 5. 当前已经形成维护税的地方

目前最像“维护税”的地方有三类：

### 5.1 subject 解析与 case context 形状反复手写

`profile / board / active_facets` 的空值、resolved source、case context 现在已在：

- `artifact report`
- 多份 summary schema
- export 脚本
- inspect 索引摘要

之间反复出现。

### 5.2 binding / bringup 计数在多层摘要间二次、三次复制

当前同一批计数会在：

- 局部 summary
- `system_formation`
- `system_compiler_summary`

之间重复镜像。  
这类字段很适合先抽出统一投影描述。

### 5.3 resource / fact 计数字段与库存字段同时承载法律文本和报告文本

这里的问题不是字段多，而是：

- 一部分字段像法律文本
- 一部分字段像导出结果
- 一部分字段像 compare 输入

如果没有统一事实源，后续很容易继续分叉。

## 6. 第一试点建议

基于当前体检，第一试点建议固定为两组字段族：

- `subject`
- `binding_result / bringup_order` 计数族

原因很直接：

- 它们的重复事实税已经明显
- 语义相对稳定
- 与 `artifact report`、summary、inspect 三个面都有直接关系
- 尚未深度卷入 `resource_contract / fact_resolution` 的法律联动复杂度

当前明确不建议第一波先碰：

- `resource_contract / fact_resolution` 全量
- compare mode 全量字段
- runtime_observe 全量对象
- 任意 C++ 类型的自动反射

## 7. 本轮结论

这轮体检最重要的结论不是“应该引入某个元编程库”，而是：

> **Charm 当前已经出现了足够明确的重复事实税，值得先在 `artifact report` 链上建立受控投影层。**

更具体地说：

- `subject` 族已经适合作为第一批 canonical fact source 候选
- `binding / bringup` 计数族已经适合作为第一批 projection pilot 候选
- `system_input` 族值得进入后续范围，但第一波先收窄到 subject 子集更稳
- `resource_contract / fact_resolution` 明确放到第二波

这也是当前为什么应先走 **projector-first**，而不是一上来做通用反射层的直接原因。
