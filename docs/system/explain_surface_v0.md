# Explain Surface / Artifact Report v0

本文不是新的调试 UI，也不是最终冻结的外部协议。  
它用于定义 Charm 在 `system compiler v0` 阶段面向人类与工具的最小输出面：`artifact report` 与 `explain surface`。

它要回答的核心问题不是“系统内部是不是很优雅”，而是：

> **当系统已经被编译、物化、导出之后，人和工具到底该通过什么表面理解它。**

在 Charm 的中长期主线上，`explain surface` 不是额外酷炫功能，  
它更接近 `system compiler` 的自然副产物：

- 如果系统可以被编译
- 如果 bringup 可以被举证
- 如果资源边界可以被审计

那么这些结果就不应只留在源码和人脑里，  
而应成为可以被报告、被查询、被自动化消费的对象。

## 1. 为什么需要单独定义输出面

Charm 现在已经不是只在追求“结构比较整齐”的阶段。  
当前主线已经开始要求：

- 系统秩序可解释
- bringup 过程可举证
- 资源边界可审计

一旦这些东西成立，新的问题就会立刻出现：

- 哪些结果是给人读的
- 哪些结果是给脚本和 CI 读的
- 哪些结果只是样例协议
- 哪些结果已经可以作为稳定桥接面
- runtime 观察与静态导出如何对齐

如果没有单独的输出面定义，  
这些解释物就会再次退化成：

- 零散脚本
- 临时 JSON
- 各种只对当前作者有意义的导出格式

这会直接削弱 system compiler 最关键的价值之一：

> **把系统秩序交给人和工具共同消费。**

## 2. `artifact report` 与 `explain surface` 的区别

这两个词相关，但不应混成同一件事。

### 2.1 `artifact report`

`artifact report` 更偏：

- 批量生成
- 文件工件
- CI / 脚本 / IDE 原型消费
- 构建后或导出后查看

它回答的是：

> “本次系统编译/导出，稳定产出了哪些可审计结果。”

### 2.2 `explain surface`

`explain surface` 更偏：

- 人类可追问
- 工具可查询
- 读路径统一
- 静态结果与运行时观察的桥接面

它回答的是：

> “当我想追问某个系统事实时，应该通过什么统一问题模型拿到答案。”

可以把两者理解为：

- `artifact report`
  是工件面
- `explain surface`
  是问题面

前者更像“系统吐出什么”，  
后者更像“人和工具如何追问系统”。

## 3. v0 的位置与范围

`explain surface v0` 当前不追求做成完整 runtime inspector 平台。  
它在现阶段的职责很收敛：

> **先把现有静态导出、报告工件与少量运行时观察收束成一套统一输出语言。**

这意味着 v0 的重心应是：

- 只读
- 可导出
- 可引用
- 可审计
- 可被脚本消费

而不是：

- 新做一个庞大的交互式工具
- 提前冻结所有长期协议
- 一步到位做成“系统全知视角”

## 4. 当前已经存在的胚胎

Charm 这条线并不是从零开始。  
当前仓库已经有几组很强的输出面胚胎。

### 4.1 `materialized_graph` 观察导出链

当前已经存在：

- `docs/system/init_materialized_graph_observe.md`
- `init.observe`
- `DOT / JSON sample` 导出

这条链已经证明：

> **系统装配结果可以稳定投影成只读语义视图。**

### 4.2 机器可读协议分层

当前 `schemas/` 已经明确区分了几层协议：

- `sample/v2`
- `export_bundle/v1`
- `bundle_diff/v1`
- `ci_summary/v1`
- `report_manifest/v1`

对应说明见：

- `schemas/README.md`

这非常重要，因为它说明当前仓库已经不是只有“随手导个 JSON”，  
而是在开始定义：

- 哪些协议偏样例
- 哪些协议偏稳定桥接
- 哪些协议是 CI / report 链消费面

### 4.3 bringup 与资源这两条新子主线

当前已经新增：

- `docs/system/bringup_evidence_pipeline_v0.md`
- `docs/system/resource_contract_v0.md`

它们已经给出两类新的解释物需求：

- bringup evidence report
- resource contract report

也就是说，`materialized_graph` 不再是唯一输出源，  
system compiler 正在长出多张可被解释的面。

### 4.4 runtime 可观察胚胎

在 runtime 这一侧，当前也已经出现稳定观察缝：

- `PublishState`
- `ExportState`
- `ExportTransition`

它们虽然还不是完整 explain surface，  
但已经证明：

> **运行时变化也可以被提升成稳定观察语言，而不是只靠临时日志。**

## 5. v0 的最小 `artifact report`

当前建议把 system compiler 的最小工件面先收敛成一份统一的 `artifact report` 语义，而不是继续平铺很多彼此独立的小报告。

具体字段分组、最小样例对象与工程边界，
见：`docs/system/artifact_report_v0.md`

v0 阶段建议至少覆盖：

- capabilities
- materialized order
- required facts
- unresolved bindings
- active facets
- bringup evidence summary
- resource contract summary
- supporting artifacts 引用

这里最关键的一点是：

> **报告本身应成为可引用对象，而不是“看完就散”的终端输出。**

这意味着 v0 报告至少需要稳定描述：

- 它是谁生成的
- 它引用了哪些底层工件
- 它覆盖哪些 case / profile / board / facet
- 它的摘要结论是什么

## 6. v0 的最小 `explain surface`

当前建议先把问题面收敛为五类最小查询，而不是先发明很大的命令系统。

### 6.1 `cap list`

它回答：

- 当前系统有哪些 capability
- 哪些是 materialized 结果
- 哪些已经进入 published 表面

它的输入可以来自：

- materialized graph 导出
- registry publish 状态
- bringup evidence report

### 6.2 `why unavailable`

它回答：

> “为什么某个 capability / 绑定 / 服务当前不可用。”

这条查询是 explain surface 里最有价值的能力之一。  
它应优先能区分：

- 未声明
- 未 materialized
- 依赖未满足
- 未 published
- 被 blocked
- 已 failed
- 资源契约 violated

也就是说，它不只是回答“没有”，  
而要回答：

> **到底卡在哪一层。**

### 6.3 `graph path`

它回答：

- 某个 consumer 依赖链是如何成立的
- 某条 bringup 路径经过哪些关键节点
- 某个 capability 的 provider 链接在哪里

当前最自然的静态输入来源仍然是：

- `materialized_graph`
- 依赖边导出
- bringup evidence summary

### 6.4 `recent transitions`

它回答：

- 最近有哪些运行时状态变化发生
- 哪些 published / attached 状态发生过切换
- 哪些 runtime export 事件值得被观察

这条查询当前更适合先挂在：

- `PublishState`
- `ExportState`
- `ExportTransition`

之上，而不急着追求全系统统一事件总线。

### 6.5 `resource summary`

它回答：

- 当前系统声明了哪些资源/行为要求
- 哪些条件由 profile / board / runtime 提供
- 哪些要求已 satisfied
- 哪些 violated
- 哪些仍 unknown

这条查询正是：

- `resource contract v0`
- `explain surface v0`

之间最自然的连接点。

## 7. 当前推荐的协议分层

当前最健康的做法，不是再发明一套完全脱离现有工件链的新协议，  
而是沿着现有分层继续扩展。

建议先维持这样一组层次：

### 7.1 样例层

- `sample`

用途：

- 字段勘探
- 原型接入
- 快速结构观察

### 7.2 工件组织层

- `bundle`
- `report manifest`

用途：

- 批量导出结果组织
- 报告工件发现
- 上层工具稳定引用

### 7.3 比较与 CI 层

- `bundle diff`
- `ci summary`

用途：

- 增量变化分析
- 自动化审阅
- CI 状态汇总

### 7.4 解释层

- `artifact report`
- `explain surface`

用途：

- 把多种底层工件收束成统一的人类/工具问题面

这里最关键的边界是：

> **explain surface 应优先消费已经存在的稳定工件，而不是绕过工件层直接依赖内部实现。**

## 8. v0 的工程边界

当前最健康的推进方式是：

1. 先定义最小问题面
2. 先把现有报告工件收束成统一语义
3. 先让静态导出与少量 runtime 观察可以互相引用
4. 先服务报告、CI、脚本与审阅
5. 再逐步增强实时查询能力

近期不建议：

- 直接承诺一个全局、实时、完整的运行中系统检查器
- 过早把所有查询格式冻结成终局协议
- 为了 explain 而给运行时塞入过重负担
- 跳过现有 `bundle / diff / report manifest` 体系另起炉灶

v0 更合理的判断标准是：

> **当人或工具追问“这个系统为什么长成这样、为什么现在不可用、为什么这里违法”时，Charm 能否比过去更稳定地给出结构化答案。**

## 9. 当前结论

`Explain Surface / Artifact Report v0` 当前不应理解成又一个平行子系统。  
它更像是 system compiler 把自己“说给外部世界听”的第一版语言：

- `artifact report` 让系统结果变成可引用工件
- `explain surface` 让这些工件变成可追问问题面
- `materialized_graph` 提供静态结构胚胎
- bringup evidence 与 resource contract 提供新的解释维度
- runtime transition 观察提供少量动态事实

因此这条线的近期目标可以收束成一句话：

> **先让 Charm 能把系统事实稳定吐出来、稳定被追问，再逐步把这套输出面做成更强的工具与运行时桥接层。**
