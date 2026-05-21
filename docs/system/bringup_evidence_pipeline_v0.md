# bringup 证据流水线 v0

本文不是新的 bringup 框架，也不是新的 codegen 方案。  
它用于定义 Charm 在 `bringup evidence pipeline v0` 阶段的目标、状态语言、当前胚胎映射与明确边界。

它要回答的核心问题不是“系统能不能跑起来”，而是：

> **系统为什么成立，以及这些成立过程能否被稳定举证。**

在 Charm 的中长期主线里，`bringup 可举证` 不是孤立能力，  
它是 `system compiler` 主干上最容易先形成社会可见价值的一条线。

上位法理见：[`../compiler/charm_compiler_constitution_v0.md`](../compiler/charm_compiler_constitution_v0.md)。本文中的 `declared / materialized / published / observed` 可以被视为 Charm compile-time world state transition language 的现有胚胎，但本文仍只定义 bringup evidence pipeline v0 的状态语言，不改变现有 evidence/artifact 判决模型。
更完整的 compiler lifecycle 状态语言见：[`../compiler/compiler_world_lifecycle_v0.md`](../compiler/compiler_world_lifecycle_v0.md)，但该文档不改变本 bringup evidence pipeline 的原有语义。
现有 bringup evidence 到 compiler lifecycle states 的只读映射见：[`../compiler/compiler_world_lifecycle_projection_v0.md`](../compiler/compiler_world_lifecycle_projection_v0.md)。

## 1. 为什么 bringup 需要从手艺活变成证据流水线

传统 MCU bringup 往往依赖：

- 板级经验
- 串口 `printf`
- 一步步试时钟、IRQ、UART、flash、mount
- “现在看起来通了”的局部判断

这类方法并非不能工作，但它的问题很明确：

- 事实散在代码和脑子里，不稳定
- 依赖路径很难复盘
- 出问题时，难以区分是“没声明”“没装配”“没发布”还是“没被观察到”
- 板级移植容易退化成老师傅手感

Charm 这一条线想做的，不是把 bringup 魔法再包装一次，  
而是把 bringup 过程拆成一条可以被报告、被审计、被复盘的证据链。

最小目标不是“自动生成一切”，而是：

> **让系统能说清楚：它声明了什么、装配成了什么、发布了什么、观察到了什么。**

## 2. v0 的范围与位置

`bringup evidence pipeline v0` 当前只做一件事：

> **把现有 `init.graph -> materialize -> observe` 与 capability export 的结果，收束成统一的只读证据语言。**

它在当前架构中的位置可以理解为：

- `init.graph`
  负责静态装配硬规则
- `init.materialize`
  负责把输入 plan 规范化成可观察图
- `init.observe`
  负责把规范化结果投影成稳定只读视图
- `registry publish / stable slot export`
  负责把运行中的 capability 进入系统可消费表面
- `bringup evidence pipeline`
  负责把这些阶段连成一份可解释、可审计的证据报告

因此它不是新的执行后端，  
而是开在现有 bringup 主路径之上的观察与解释层。

## 3. v0 明确不做什么

当前版本明确不做：

- 不引入新的 DSL
- 不引入大规模 code generation
- 不把 runtime discovery plane 强行伪装成 `init.graph`
- 不要求所有子系统立即改成统一底层实现手法
- 不承诺已经有完整、统一的失败证明系统

当前更重要的是先把以下几件事说清楚：

- 证据流水线的状态语言是什么
- 这些状态与现有仓库概念如何映射
- 哪些状态已经有代码级载体
- 哪些状态当前仍只是报告语言

## 4. v0 状态语言

当前建议把 bringup 证据语言收敛为六种状态：

- `declared`
- `materialized`
- `published`
- `observed`
- `failed`
- `blocked`

这六个词的目标不是替换所有局部枚举，  
而是提供一套跨 `board fact / init graph / registry export / observe report` 的统一解释语言。

### 4.1 `declared`

含义：系统输入侧已经明确声明“有这件事”。

在当前仓库里，典型来源包括：

- `BoardCaps`
- `platform.board_facts` / `system_compiler.fact_evidence/v0`
  对 board package 已知事实的只读投影
- bringup helper 里声明的能力与节点
- `Recipe` / `ready_as<Cap>()`
- runtime stable slot export 的 descriptor 与命名约定

它回答的是：

> “这个能力、节点、绑定关系，是否进入了系统的事实输入。”

### 4.2 `materialized`

含义：声明已经被规范化成可执行、可观察的图结果。

当前主要落在：

- `init.materialize`
- `materialized_graph`
- 由 `observe(...)` 派生的 node / edge view

它回答的是：

> “这件事是否已经从输入声明，变成系统真正承认的一部分装配结果。”

这一步特别重要，因为它把“我以为系统会这么连”变成：

> “系统实际归一化之后，确实是这么连的。”

### 4.3 `published`

含义：能力已经进入系统可消费的对外表面。

当前在代码里已经有直接载体：

- `io::PublishState::{missing, published}`
- `block::PublishState::{missing, published}`

典型含义是：

- `io.registry` 里已经能看到该 endpoint
- `block.registry` 里已经能看到该 device

它回答的是：

> “这个 capability 现在对系统其他部分可见了吗。”

### 4.4 `observed`

含义：系统不仅完成了某一步，而且这一步已经进入稳定观察面。

当前主要有两类观察来源：

- `init.observe`
  把 `materialized_graph` 投影成稳定 DTO，并可导出 `DOT / JSON sample`
- stable slot export observer
  通过 `ExportTransition` 观察 `ensure_exported / attach / detach / unexport`

它回答的是：

> “这件事不只是发生了，而且系统已经能把它稳定说出来。”

因此 `observed` 不等于“功能可用”，  
它更接近“已经具备可解释证据”。

### 4.5 `failed`

含义：系统已经尝试推进，但在执行过程中明确失败。

典型例子包括：

- init 执行返回错误
- export / attach 动作返回显式失败
- bringup 节点执行过程中触发明确错误码

当前仓库里，`failed` 还没有被统一收敛成单一全局枚举；  
它目前更适合作为报告语言，由具体 `Errc`、阶段上下文与局部状态联合支撑。

它回答的是：

> “系统不是没走到，而是走到了并明确失败了。”

### 4.6 `blocked`

含义：系统没有继续推进，但原因是前置条件未满足，而不是已经执行失败。

典型情况包括：

- 缺少 required capability
- phase 不合法
- 拓扑上无法成立
- 上游未发布，导致下游暂不可达
- build/profile 过滤导致节点未能进入当前实例

在当前仓库里，`blocked` 也更接近报告语言，  
它往往来自：

- `materialize/build/start` 的错误原因
- capability 缺失
- publish/export 前置条件不满足

它回答的是：

> “系统没能继续，但卡在前置条件，而不是动作本身的显式失败。”

## 5. `published` 与 `live` 必须分开

这一点在当前驱动模型里已经非常关键，  
bringup 证据流水线必须继承同样的边界。

当前不能把以下两件事折叠成一个魔法状态：

- capability 是否已发布给系统消费面
- 底层目标是否仍处于 attached / live 状态

当前代码里已经有对应分层：

- `PublishState`
  表示“对系统是否已发布”
- `ExportState::{missing, detached, attached}`
  表示“稳定槽位当前是否 live”

这意味着：

- `published`
  是系统可见性语言
- `attached`
  是底层存活/挂接语言

`bringup evidence pipeline v0` 应把这两层一起纳入报告，  
但不能为了“看起来统一”而把它们强行压扁。

## 6. 当前仓库胚胎映射

Charm 这条线不是从零开始。  
当前仓库已经有一组非常接近证据流水线的胚胎。

| 证据阶段 | 当前仓库胚胎 | 当前语义 |
| --- | --- | --- |
| `declared` | `BoardCaps`、bringup helper、`Recipe`、capability 名称表 | 板级事实与装配意图已经进入输入 |
| `materialized` | `init.materialize`、`materialized_graph` | 输入被规范化为稳定图 |
| `published` | `io.registry` / `block.registry` 的 `PublishState` | capability 已进入系统可消费表面 |
| `observed` | `init.observe`、`DOT / JSON sample`、`ExportTransition` observer | 结果已被稳定导出和工具消费 |
| `failed` | `Errc`、局部执行返回、export/attach 失败 | 已尝试推进且明确失败 |
| `blocked` | 缺 capability、phase / topo 问题、未满足前置条件 | 未能推进，原因是依赖或秩序未成立 |

其中最值得注意的是：

- `declared -> materialized`
  当前主要由静态 capability plane 承担
- `published -> observed`
  当前已经同时覆盖静态 bringup 与 runtime stable export

这正是双平面架构可以用统一 capability 语言收口的原因之一。

## 7. 当前推荐的最小输出物

`bringup evidence pipeline v0` 近期更应该先产出“解释物”，而不是新运行时。

建议最小输出物包括：

- bringup evidence report
- capability 级 evidence matrix
- unresolved facts / unresolved bindings 列表
- materialized order
- published capability 列表
- observed capability / transition 摘要
- blocked / failed 原因摘要

其中 capability 级 evidence matrix 当前至少应能围绕每个 capability 回答：

- 是否已经 `declared / materialized / published / observed`
- 是否已经进入 `blocked / failed`
- `publish_state / export_state` 当前是什么
- provider / consumer 证据节点是谁

其中可以进一步区分两层：

### 7.1 静态 bringup 证据

建议至少覆盖：

- 图中有哪些节点和依赖边
- 哪些 capability 是声明态，哪些已经 materialized
- 当前实例实际采用了什么顺序
- 哪些要求无法解析

### 7.2 runtime export 证据

建议至少覆盖：

- 哪些 capability 已 published
- 哪些稳定槽位当前 attached / detached
- 最近发生过哪些 `ExportTransition`
- 哪些导出尝试失败或被阻塞

这两层输出物可以先分开实现，  
但它们最终应共享同一种报告语言。

当前仓库里，这条 explain 面已经有了两个正式入口：

- `scripts/inspect_system_compiler_artifact_report.ps1 -Case <name> -BringupEvidence`
  面向单 report，展开 capability 级证据矩阵
- `scripts/inspect_system_compiler_artifact_report.ps1 -ArtifactRoot <path> -BringupEvidence`
  面向 artifact root，收敛出跨 case 的 capability matrix 与 reason matrix

后者当前至少会回答：

- 每个 case 的 `declared / materialized / published / observed / blocked / failed` 摘要
- 每个 capability 在哪些 case 中 `declared / materialized / observed / published`
- capability 级 `publish_state / export_state`
- case-qualified `provider_nodes / consumer_nodes`
- `blocked_reason_matrix / failed_reason_matrix`

也就是说，bringup 证据不再只能按单 case 追问，
还可以横向查看：

- 某个 capability 是否只在部分 bringup case 中成立
- 板级事实是否持续停留在 declared-only 状态
- 哪些阻塞或失败原因在多 case 间重复出现

如果当前 report 来自 compare 模式，
`scripts/inspect_system_compiler_artifact_report.ps1 -Case <name> -BringupEvidence -AsJson`
当前还会额外暴露 `query.comparison.bringup_evidence`，
至少带出：

- `changed`
- `left / right`
- `summary_changes`
- `published_capability_changes`
- `capability_changes`

这意味着 bringup 证据当前已经不只会回答“当前 case 长什么样”，
还可以回答“相对 baseline，哪些 published / attached 语义发生了漂移”，
而不必把这类 sidecar-only 变化硬塞回结构 diff。

如果选择的是整组 compare report，
`scripts/inspect_system_compiler_artifact_report.ps1 -ArtifactRoot <path> -BringupEvidence -AsJson`
现在也会继续暴露 `query.comparison.bringup_evidence`，
至少带出：

- `compared_case_count / changed_case_count / unchanged_case_count`
- `changed_cases / unchanged_cases`
- `summary_change_matrix`
- `capability_change_matrix`

这意味着 bringup 证据现在已经能从“单 case compare explain”
继续抬升到“artifact_root 横向 compare explain”。

## 8. 当前最贴仓库现实的示例链路与 fixture catalog

### 8.1 `materialize_observe_demo`

路径：

- `Examples/init/materialize_observe_demo/main.cpp`

它验证的是最基础的一层：

- 声明如何进入 `materialized_graph`
- `observe(...)` 如何把图投影成稳定只读视图
- `DOT / JSON sample` 如何成为最小证据导出

### 8.2 `bringup_block_observe_demo`

路径：

- `Examples/init/bringup_block_observe_demo/main.cpp`

它验证的是：

- bringup helper 组合出的系统装配结果
- 在 `start_graph(...)` 之前也可以先被 materialize、观察和导出

这非常接近“bringup 还没真正跑，就先把系统将如何成立说出来”。

### 8.3 `bringup_minimal_observe_demo`

路径：

- `Examples/init/bringup_minimal_observe_demo/main.cpp`

它进一步验证：

- 更贴近真实系统入口的 `BringupMinimal`
- `BoardCaps`
- console alias / input / can 等板级组合

这说明当前仓库已经具备最关键的雏形：

> **系统入口 helper 本身，也可以被拿来举证。**

### 8.4 Board evidence fixture catalog

路径：

- `docs/system/board_evidence_fixture_catalog_v0.md`

它记录的是当前 board/probe evidence 输入形态：

- board/package facts
- I2C fact composition
- no-hardware WHOAMI probe evidence
- Host fixture `board.bringup` evidence
- 一键 chain smoke 与分步调试入口

这份 catalog 不替代本文件的状态语言。
它只负责回答“当前哪些 evidence producer 已经可以被导出、比较、校验和复验”。

## 9. runtime discovery plane 如何接入这条线

当前需要明确一个边界：

> `bringup evidence pipeline v0` 的主战场仍然是静态 capability plane。

也就是说：

- 片上控制器
- 板级已知资源
- 静态 capability 依赖

这些东西的主路径，仍然是：

- `BoardCaps`
- `init.graph`
- `materialize`
- `observe`

而 runtime discovery plane 这边：

- `device::*`
- `RuntimeBus`
- runtime manager
- stable slot export

它们不应被强行塞回 `init.graph` 世界。  
它们更适合通过以下方式接入证据语言：

- 复用 `published`
- 复用 `observed`
- 保留 `attached/detached` 这类 live 语义
- 在报告层表达 `failed / blocked`

这样做的好处是：

- 双平面不变成两套完全互不相认的话语体系
- 同时也不强迫动态平面伪装成静态装配图

当前与这条边界最相关的 runtime 样板包括：

- `Examples/system/device_runtime_block_slot_demo/main.cpp`
- `Examples/system/device_runtime_channel_slot_demo/main.cpp`
- `Examples/usb/usb_host_runtime_multi_smoke/main.cpp`

## 10. v0 的工程边界

当前最健康的推进方式是：

1. 先冻结状态语言
2. 先冻结当前胚胎映射
3. 先产出只读报告
4. 先把示例链路变成可复盘证据
5. 再决定哪些字段需要升级成更稳定的协议

近期不建议：

- 过早引入新的配置语言
- 过早要求统一失败语义到一个“万能大状态机”
- 为了追求宏大一致性而重写现有 bringup / runtime 代码

v0 更合理的判断标准是：

> **当系统 bringup 出现问题时，我们能否比“再打一串 log 看看”更快地说清楚：问题卡在哪一层。**

为了把这条 explain 面守成回归，仓库当前还提供了最小 smoke：

- `scripts/materialized_graph_bringup_evidence_matrix_smoke.ps1`
- `scripts/materialized_graph_bringup_evidence_compare_smoke.ps1`
- `scripts/materialized_graph_bringup_evidence_compare_root_smoke.ps1`

其中 matrix smoke 直接复用
`inspect_system_compiler_artifact_report.ps1 -ArtifactRoot ... -BringupEvidence -AsJson`
的真实输出，重点守住：

- 预期 bringup case 仍能进入 artifact_root 级矩阵
- `board.win_stub` 这类板级事实仍保持 declared-only
- `system.clock` 这类公共 capability 仍能跨 case materialized / observed
- `block.sd0` 这类 case-specific capability 不会误扩散到其它 bringup case
- `blocked_reason_matrix / failed_reason_matrix` 在当前 happy-path 示例里保持为空

而 compare smoke 则通过“左右 bundle 共用同一路径 sidecar、但内容不同”的合成场景，
重点守住：

- `bundle diff` 在 sidecar-only 漂移下仍可保持 `status = unchanged`
- `artifact report.comparison.bringup_evidence` 仍能捕获 `published_count` 与 capability 级状态漂移
- `inspect_system_compiler_artifact_report.ps1 -Case <name> -BringupEvidence -AsJson`
  会稳定暴露 `query.comparison.bringup_evidence`

而 compare root smoke 则继续往前守住：

- compare report 进入 `artifact_root` 聚合后不会丢失 compare 负载
- `query.comparison.bringup_evidence.changed_cases` 能稳定指出真正漂移的 case
- `capability_change_matrix` 能继续把 capability 级 compare 变化收束成横向矩阵

## 11. 当前结论

Charm 的 bringup 证据流水线，当前不该理解成一个全新子系统。  
它更像是 `system compiler` 主线上的第一批解释物：

- 把板级事实从隐性经验变成输入语言
- 把装配结果从“脑补拓扑”变成 `materialized graph`
- 把 capability 可见性从“能不能 find 到”提升成显式状态
- 把运行时导出变化从偶然 log 提升成稳定观察面

因此这条线的近期目标可以收束成一句话：

> **先让 Charm 把 bringup 过程稳定说出来，再逐步让它更强地约束和验证 bringup。**
