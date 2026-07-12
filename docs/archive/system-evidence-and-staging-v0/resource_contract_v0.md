# 资源契约 v0

本文不是资源统计器，也不是完整证明系统。
它用于定义 Charm 在 `resource contract v0` 阶段的目标、最小法律文本、当前胚胎映射与明确边界。

它要回答的核心问题不是“系统把模块连起来了吗”，而是：

> **这些模块活在当前资源宇宙里，到底合不合法。**

在 Charm 的中长期主线上，`资源可立法` 不是额外附属能力，
它是 `system compiler` 从“解释系统长什么样”继续走向“解释系统在哪些条件下才成立”的关键一步。

## 1. 为什么 capability 还不够

`capability` 非常重要。
它回答的是：

- 系统里有什么能力
- 谁提供它
- 谁依赖它
- 它们如何被装配起来

但 capability 还不能单独回答下面这些问题：

- 一个路径是否允许阻塞
- 一个实现是否偷偷依赖 heap
- 一个单元是否要求 reactor 才能活
- 一个超时/定时语义是否要求 monotonic clock
- 一个调用是否允许在 IRQ 语境使用

也就是说：

> **capability 解决“能做什么”，
> resource contract 解决“在什么资源与执行宇宙里，这样做才合法”。**

如果缺少这一层，系统即使装配正确，也可能只是：

- 图上成立
- 运行时偶尔能跑
- 但资源与行为边界全靠人脑记忆

这正是 Charm 接下来需要补上的语言层。

## 2. 这条线为什么现在值得做

资源契约不是全新幻想。
仓库里其实已经有一些局部法律，只是它们还没有被升格成统一语言。

例如：

- `init.graph` 已经明确要求：
  - no dynamic allocation
  - no blocking inside init
- SSU 已经明确区分：
  - `non_blocking`
  - `may_block`
- 局部实现里已经出现：
  - `irq_safe`
  - task / ISR 语义分化

这说明问题不是“Charm 完全没有资源法律”，
而是：

> **资源法律已经零散存在，但还没有进入统一系统语言。**

`resource contract v0` 的目标，就是先把这些局部纪律从暗知识变成可导出、可审计、可报告的正式词汇。

## 3. v0 的位置与范围

`resource contract v0` 当前不直接接管调度器，也不直接改写所有模块。
它在现阶段的职责很收敛：

> **先定义资源法律文本，并把现有仓库里已经存在的局部资源语义收束成统一报告语言。**

在当前架构中的位置可以理解为：

- `system compiler`
  负责解释系统如何成立
- `bringup evidence pipeline`
  负责解释 bringup 过程如何被举证
- `resource contract`
  负责解释这些系统部分在哪些资源/执行条件下合法

它补上的，不是新的功能面，
而是系统语言中的“合法性层”。

## 4. v0 明确不做什么

当前版本明确不做：

- 不做完整 RAM / stack 精确预算器
- 不承诺自动推出所有模块的资源需求
- 不把全仓库一次性升级成强制阻断系统
- 不要求每条路径立即拥有完整机器证明
- 不因为追求统一而重写现有调度与驱动实现

当前更重要的是先做成下面这件事：

> **哪些约束属于 Charm 的语言，先写成法律文本。**

换句话说，v0 优先级是：

- 先命名
- 先导出
- 先审计
- 再逐步执法

## 5. v0 的最小法律文本

当前建议先把下面五个词固定为资源契约 v0 的最小元数据：

- `may_block`
- `needs_heap`
- `needs_reactor`
- `needs_monotonic_clock`
- `irq_safe`

它们不是“最终全集”，
但已经足够覆盖 Charm 当前最值得收束的几类资源与行为边界。

### 5.1 `may_block`

它回答的是：

> “这个单元/路径是否允许阻塞等待。”

当前最接近的现成载体已经存在于：

- `docs/system/ssu_contract.md`
- `docs/system/ssu_review_checklist.md`
- `Modules/system/kernel/ssu.cppm`
- `Modules/system/kernel/scheduler.cppm`

当前仓库里，这个概念已经不是空想，
而是 SSU 的一部分执行语义。

这说明 `may_block` 在 v0 阶段最适合先做：

- 跨子系统统一命名
- 审计导出
- 与其它资源需求联动报告

### 5.2 `needs_heap`

它回答的是：

> “这个实现是否要求堆分配能力才能成立。”

当前仓库里，heap 依赖更多还是隐性事实：

- 藏在实现细节里
- 藏在容器/分配策略里
- 藏在特定 profile 的默认假设里

v0 阶段先不追求自动静态推导，
而应先把它升格成可显式声明的法律文本。

### 5.3 `needs_reactor`

它回答的是：

> “这个单元是否只有在 reactor 语义存在时才合法。”

当前仓库里，reactor 相关依赖往往表现为：

- capability 依赖
- runtime context 依赖
- pump / drain 路径依赖

但这些依赖目前主要还在“功能装配语言”里，
没有被提升成统一资源契约字段。

`needs_reactor` 的价值在于，它可以把：

- “代码里 import 了 reactor”
- “逻辑上离不开 reactor”

这两件事明确区分开来。

### 5.4 `needs_monotonic_clock`

它回答的是：

> “这个语义是否要求稳定的单调时间源。”

当前仓库里，时间相关依赖已经大量存在：

- `system.clock`
- timer / timeout
- reactor pump / replay / runloop

但“是否需要 monotonic clock”这件事，
现在更多还是背景假设，而不是法律文本。

当 Charm 继续往托管时间推进时，
这个字段会是很关键的桥梁：

- 它连接资源契约
- 也连接未来的 managed time 语义

### 5.5 `irq_safe`

它回答的是：

> “这个路径/对象是否允许在 IRQ 语境下被安全调用或推进。”

当前仓库已经出现相关局部语义，例如：

- `Modules/core/service/ring_queue.cppm`
- `Modules/io/reactor/io.reactor.cppm`

但这些语义目前更像局部实现策略，
还没有升格为统一系统法律文本。

v0 阶段最重要的不是先争论它的最终细化粒度，
而是先承认：

> **IRQ 安全性必须是可见契约，不应只藏在实现习惯里。**

## 6. v0 报告语言

资源契约 v0 当前更适合先使用一组审计词汇，
而不是急着冻结成一个万能大枚举。

建议先把报告语言分成三层：

### 6.1 声明层

- `declared`
  - 某个模块、路径、profile 或 board 已显式声明资源元数据
- `provided`
  - 当前系统承载条件显式提供了某项资源事实

### 6.2 审计层

- `audited`
  - 报告系统已经把声明与承载条件拿来比较过

### 6.3 结论层

- `satisfied`
  - 当前资源契约在此上下文中合法
- `violated`
  - 当前资源契约与承载条件发生明确冲突
- `unknown`
  - 当前还没有足够元数据给出可靠结论

这组词的目的不是制造概念层次，
而是避免 v0 阶段把“未建模”“未审计”“明确违法”混成一团。

### 6.4 当前 v0 输入形状

当前真实导出链里，最小资源法律文本已经先落在：

- `scripts/materialized_graph.export_case_manifest.v1.json`

它当前按 per-case 承载 `declared_contracts`，最小形状为：

```json
{
  "contract": "needs_monotonic_clock",
  "requires": ["system.clock"]
}
```

当前这组字段刻意保持很小：

- `contract`
  - 当前固定在 `may_block / needs_heap / needs_reactor / needs_monotonic_clock / irq_safe`
- `requires`
  - 当前按 all-of 语义解释
  - 当 `requires` 为空时，当前报告链会把该条合同归入 `unknown`

这意味着 v0 目前不是在做复杂推理，
而是在做一件更克制、也更稳定的事：

> **先把资源法律写成机器可携带的最小条款。**

当前 artifact report 的最小审计来源也已经收敛为三类事实：

- 输入侧 `declared_facts`
- `subject` 派生事实，例如 `board.* / profile.* / facet.*`
- 图里实际提供的 capability / fact 名

也就是说，v0 不是“凭空判案”，
而是先把法律文本与当前系统已知事实做最小比对。

## 7. 当前仓库胚胎映射

Charm 这条线同样不是从零起步。
当前仓库已经有若干很强的胚胎，只是还分散在不同体系里。

| 资源契约要素 | 当前仓库胚胎 | 当前意义 |
| --- | --- | --- |
| `may_block` | SSU `BlockingKind`、scheduler 观测输出 | 阻塞语义已进入执行语义语言 |
| `irq_safe` | `ring_queue` / `io.reactor` 局部 policy | IRQ 安全性已出现实现级标记 |
| no dynamic allocation | `docs/system/init_graph_contract.md` | bringup 路径已存在局部资源硬法 |
| no blocking inside init | `docs/system/init_graph_contract.md` | bringup 路径已存在局部行为硬法 |
| `needs_reactor` | reactor capability / runtime 依赖 | 语义已存在，但仍多停留在装配层 |
| `needs_monotonic_clock` | `system.clock`、timer/timeout 相关路径 | 时间依赖已存在，但尚未升格为法律字段 |
| `needs_heap` | 各类实现隐性假设 | 目前仍主要是暗知识，需要显式化 |

这里最重要的观察是：

> **Charm 不是缺少资源语义，而是缺少统一资源法律语言。**

## 8. `init`、SSU 与资源契约的关系

这三者不能互相替代，但它们天然相关。

### 8.1 `init.graph`

`init.graph` 已经证明了一件事：

> **局部硬法是可以成立的。**

例如：

- no dynamic allocation
- no blocking inside init

这说明资源契约不是遥远愿景，
而是已经有成功先例，只是当前还局限在 bringup 局部。

### 8.2 SSU

SSU 已经证明了另一件事：

> **执行语义元数据可以进入文档、类型、调度观测与评审纪律。**

这意味着 `may_block` 这样的字段，
并不需要从“静态规范文本”直接一步跳到“全局门禁系统”。

更现实的路径是：

- 先成为语义字段
- 再进入观测与评审
- 再逐步进入更强检查

### 8.3 资源契约

资源契约 v0 的职责，就是把这些已经零散成立的局部事实，
提升成一套跨：

- bringup
- scheduler / SSU
- runtime service
- profile / board package

都能理解的统一法律语言。

## 9. 当前推荐的最小输出物

`resource contract v0` 近期更应该优先产出“资源法律解释物”，而不是复杂执行器。

建议最小输出物包括：

- resource contract report
- contract metadata inventory
- provided facts 列表
- satisfied / violated / unknown 摘要
- declared contract entries
- `may_block` / `irq_safe` 热点清单
- `needs_heap` / `needs_reactor` / `needs_monotonic_clock` 缺口清单

其中建议至少区分两层：

### 9.1 资源声明视图

回答：

- 谁声明了哪些资源/行为要求
- 哪些字段仍然缺失
- 哪些要求现在只是实现暗知识

### 9.2 合法性审计视图

回答：

- 当前 profile / board / runtime 条件提供了什么
- 哪些要求被满足
- 哪些要求被违反
- 哪些地方还无法得出确定结论

当前真实 `artifact report` 已经至少会把这些结果压成：

- `declared_contract_entries`
- `provided_facts`
- `satisfied_contracts`
- `violations`
- `unknown_contracts`
- `resource_hotspots`

而当前最小 explain 消费面也已经有了一个直接入口：

- `scripts/inspect_system_compiler_artifact_report.ps1 -ResourceSummary`

它当前会把：

- 输入侧 `declared_contract_entries`
- `declared_facts`
- `subject` 派生事实
- 图里实际提供的 fact
- 审计阶段命中的 `provided_facts`

一起压成一页稳定的 `resource summary` 查询结果，
让人和工具都可以继续追问：

- 哪条合同当前是 `satisfied`
- 哪条是 `violated`
- 哪条仍然 `unknown`
- 对应证据究竟来自哪里

为了把这张解释面真正守成回归，仓库现在还提供了一个最小 smoke：

- `scripts/materialized_graph_resource_contract_smoke.ps1`
- `scripts/materialized_graph_resource_contract_matrix_smoke.ps1`
- `scripts/materialized_graph_resource_contract_compare_smoke.ps1`
- `scripts/materialized_graph_resource_contract_compare_root_smoke.ps1`

它当前直接消费已有 `artifact-report` 输出，并复用
`inspect_system_compiler_artifact_report.ps1 -ResourceSummary` 的真实查询结果，
重点守住下面几类最小断言：

- 至少存在一条已声明资源契约
- `needs_monotonic_clock` 能稳定命中 `system.clock`
- `board.win_stub` 这类板级事实能同时出现在 `declared_facts / subject_facts`
- 资源事实来源里仍能明确指出 `audit_provided_facts`
- metadata-only diff 不会吞掉资源契约漂移
- compare 模式下的 `comparison.resource_contract` 能明确给出 `left / right / contract_changes`
- `-ResourceSummary -AsJson` 能继续把 compare 资源面暴露给 explain surface

也就是说，当前这条 smoke 守的不是“另写一套资源语义”，
而是：

> **确保 explain surface 对资源契约的说法，和正式 artifact report 保持同一事实来源。**

对 compare 场景来说，这条纪律还要再往前一步：

> **即使 case 级结构 diff 仍然是 `unchanged`，资源契约漂移也必须以正式 compare 负载被保留下来。**

与此同时，`inspect_system_compiler_artifact_report.ps1 -ArtifactRoot ... -ResourceSummary`
现在也已经支持直接返回 `artifact_root` 级聚合结果。
它会把多份 report 收束成：

- per-case 资源摘要
- contract matrix
- provided fact matrix
- resource hotspot matrix

这样资源契约不再只能按单 case 追问，
还可以直接横向查看：

- 哪条合同在哪些 case 中声明
- 哪些 case 满足 / 违反 / 仍未知
- 某个资源事实究竟覆盖了哪些 case
- 热点是否在多案例之间重复出现

如果这些 report 来自 compare 模式，
artifact_root 级 `-ResourceSummary -AsJson` 现在也会继续暴露
`query.comparison.resource_contract`，至少带出：

- `compared_case_count / changed_case_count / unchanged_case_count`
- `changed_cases / unchanged_cases`
- `summary_change_matrix`
- `contract_change_matrix`

这意味着资源契约 explain 现在已经不只会回答“单 case 相对 baseline 漂移了什么”，
还可以横向回答：

- 哪些 case 真正发生 compare drift
- 哪些 summary change 在多 case 之间重复出现
- 某条合同的 compare 变化究竟覆盖了哪些 case

## 10. v0 的工程边界

当前最健康的推进方式是：

1. 先冻结最小法律文本
2. 先把局部硬法映射进统一词汇
3. 先做导出、审计、报告
4. 先把高价值路径纳入视野
5. 再决定哪些字段值得升级成强约束

近期不建议：

- 一上来就追求全仓库精确 RAM / stack 证明
- 让资源契约反向拖死实现迁移
- 让“unknown” 被误当成“已经合法”
- 让所有模块为了过审而胡乱补元数据

v0 更合理的判断标准是：

> **当一个系统在某个 profile / board / runtime 下不该成立时，我们能否比“靠经验猜”更清楚地指出：它违法在哪里。**

## 11. 当前结论

Charm 的 `resource contract v0`，当前不应理解成一个庞大的资源证明工程。
它更像是 `system compiler` 主线上的第二批法律文本：

- 把局部“不要阻塞”“不要动态分配”提升成可迁移语言
- 把 `may_block` 这类执行事实从局部语义变成系统词汇
- 把 `irq_safe`、`needs_reactor`、`needs_heap`、`needs_monotonic_clock` 从暗知识拉到报告面
- 为未来的 profile 检查、board 检查与 explain surface 提前打地基

因此这条线的近期目标可以收束成一句话：

> **先让 Charm 能稳定说清楚“哪些资源与行为边界使一个系统合法”，再逐步让这些边界变成更强的检查。**
