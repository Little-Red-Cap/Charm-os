# Charm System Compiler Roadmap

本页不是功能愿望单，也不是某个子系统的详细设计。  
它用于定义 Charm 中长期架构主轴、近中期路线，以及当前明确不应过早深入的传奇路线。

这份文档要回答的核心问题不是“Charm 还缺哪些模块”，而是：

> **Charm 接下来要把什么东西做成自己的主语言。**

当前判断是：

> **Charm 的主任务，不是继续长模块，  
> 而是把“系统如何成立”编译出来。**

这意味着 Charm 的长期方向，不是再堆叠一个又一个子系统，
而是把嵌入式系统从“手工装配的工程对象”，推进为“可编译、可举证、可审计、可托管”的系统对象。

## 0. 一句话宣言

Charm 的长期目标，不是继续堆叠子系统，而是把嵌入式系统从“手工工程”推进为“可编译系统”。

Charm 未来真正要立住的，不是模块总数，而是以下四个维度：

- 系统可编译
- 时间可托管
- 资源可立法
- bringup 可举证

这些维度一旦立住，USB、网络、文件系统、UI、POSIX、板级支持都应成为挂载在世界观上的内容，而不是持续打散架构的新坑。

## 1. 主干、维度与边界

当前建议把 Charm 的中长期结构拆成三层：

### 1.1 主干

- 系统编译器化

### 1.2 长在主干上的三个大维度

- 时间可托管
- 资源可立法
- bringup 可举证

### 1.3 远方传奇路线

- 多语义宿主
- 全局确定性时间宇宙
- 完整资源证明系统
- 统一所有底层驱动实现手法

这套分层的目的，是避免一个常见灾难：

> **把最终世界观误当成当前工程目标。**

Charm 需要保留大野心，但近期推进必须严格区分：

- 哪些是主轴
- 哪些是维度
- 哪些是近程能落地的版本
- 哪些是现在碰了会拖死主干的诱惑

## 2. 核心主轴：系统编译器

### 2.1 主判断

Charm 的主干不是“再多一个子系统”，而是成为一个真正的 system compiler。

输入不应只是若干零散模块，而应逐步收敛为系统描述：

- `SystemSpec`
- `Profile`
- `BoardPackage`
- `Binding`
- `Facet`

输出不应只是“是否能编译通过”，而应逐步形成可审计的系统编译结果：

- capability graph
- binding result
- materialized graph
- bringup order
- audit report
- artifact report

### 2.2 v0 边界

`system compiler v0` 不追求完整 DSL，不追求全面 code generation。

v0 首先要做到的是：

- 冻结核心术语和输入/输出语言
- 明确现有仓库概念到该语言的映射
- 产出只读、可审计、可解释的系统编译结果

这意味着 v0 的首要任务是：

> **解释系统，而不是自动生成一切。**

如果过早滑向大规模 code generation，Charm 很容易被模板、目录、样板、替换机制拖走，
反而失去当前最稀缺的价值：

> **把系统世界描述清楚、编译清楚、举证清楚。**

### 2.3 输入语言草表

当前建议把几个核心词汇先固定为架构语言，而不是立刻做成新 DSL：

| 词汇 | 关注点 | 当前建议定位 |
| --- | --- | --- |
| `SystemSpec` | 这个系统想成为什么样 | 系统级目标描述 |
| `Profile` | 这个系统允许活在哪种资源宇宙里 | 资源/功能等级 |
| `BoardPackage` | 板级已知事实是什么 | 事实声明，不负责生命周期推进 |
| `Binding` | 事实如何连接到能力与实现 | 装配与桥接语言 |
| `Facet` | 同一架构在不同 embodiment 下启用哪些面 | 裁剪与呈现维度 |

这些词汇的职责边界，在 v0 阶段先以文档与报告形式固定，
不急着做成新的配置格式或宏体系。

### 2.4 与现有仓库的胚胎映射

Charm 已经有一批 system compiler 的前身，不是从白纸起步：

- `init.graph / init.materialize / init.observe`
  当前最接近 system compiler 的雏形，已经具备图构建、物化、观察导出的基础
- `BoardCaps + capability`
  当前最接近 board fact language 的形态，已经能表达板级已知资源与绑定关系
- `PublishState / ExportState / transition observer`
  当前最接近系统状态导出与可观察契约的形态
- `Vivid + replay` 倾向
  当前最接近托管时间切入点的现成土壤
- POSIX 执行面
  当前最接近 guest world 候选的宿主桥接能力

### 2.5 当前应优先产出的结果物

在 system compiler 主线上，近期更重要的不是新增能力，而是新增“解释物”：

- 系统词汇表
- 概念映射表
- capability / binding / bringup 的只读报告
- unresolved bindings 与 active facets 的可见结果
- 最小 explain surface 的输入清单

## 3. 三个月路线

### 3.1 系统编译器化 v0

目标：冻结术语与编译结果形态，而不是先扩展功能面。

建议最小落地点：

- 建立 `SystemSpec / Profile / BoardPackage / Binding / Facet` 词汇表
- 给现有核心机制建立正式映射表
- 输出最小 artifact report，至少覆盖：
  - capabilities
  - materialized order
  - required facts
  - unresolved bindings
  - active facets

### 3.2 bringup 证据流水线 v0

目标：把“图”转成“证据”。

这条线当前最贴仓库现状，也最适合作为 Charm 第一批对外可见价值。

建议最小落地点：

- 每个 bringup 节点有明确状态
- 生成 bringup evidence report
- 在报告中区分以下状态：
  - declared
  - materialized
  - published
  - observed
  - failed / blocked

`bringup evidence pipeline` 的价值不仅在于“容易先做出来”，
更在于它同时满足三件事：

- 对现有体系最顺
- 对工程现实价值极高
- 对外部理解成本最低

这条线很适合成为 Charm 第一种社会可见价值：

> **板级 bringup 不再只是老师傅手感和串口 printf 的手艺活。**

### 3.3 资源契约 v0

目标：先有法律文本，再谈执法。

建议先引入最小资源/行为元数据：

- `may_block`
- `needs_heap`
- `needs_reactor`
- `needs_monotonic_clock`
- `irq_safe`

第一阶段先做到：

- 可导出
- 可审计
- 可报告

第一阶段暂不以“全面卡构建”为目标。

Charm 这条线要先解决的是：

> **哪些约束属于 Charm 的语言。**

而不是一上来就把所有约束都变成高强度门禁。

### 3.4 托管时间 MVP

目标：只打通一条窄链，不追求全局时间宇宙。

建议切入链路：

- `input -> runloop -> UI/replay`

建议最小能力：

- pause
- replay
- fast-forward

这一阶段不追求整个世界都 deterministic，
只需要证明：

> **时间可以从背景机制，变成受管资源。**

### 3.5 驱动模型语义面 v0

目标：统一“向上暴露的世界”，而不是统一所有底层做法。

建议先把以下语义面正式命名为上层语言：

- `channel`
- `block`
- `control`
- `timebase`
- `input_source`
- `display_surface`
- `net_iface`

这条线近期以文档与映射为主，
不急着立刻改目录或重铸全部驱动实现。

Charm 真正应该统一的是：

> **对上层暴露的世界观，而不是底层实现手法。**

## 4. 一年路线

### 4.1 系统编译器 MVP

目标：形成真正可展示、可复盘的系统编译结果。

建议至少形成以下能力：

- profile 裁剪结果
- capability graph
- binding result
- bringup order
- artifact report

### 4.2 托管时间宇宙（局部）

目标：把托管时间从一条窄链扩展到几个核心运行时面。

建议优先纳入统一时间语义的对象：

- reactor
- timer
- UI replay
- 部分 POSIX timeout

形成的核心能力应是：

- 可录制
- 可回放
- 可扰动

### 4.3 资源法律升级为检查

目标：从“只读报告”升级为“局部法律”。

建议升级路径：

- profile 级检查
- board 级检查
- 关键路径构建期检查

这一步不追求全仓库一次性收紧，
而应优先覆盖：

- bringup 关键链
- runtime 关键链
- 资源边界最敏感的 profile

### 4.4 自解释系统 / explain surface

“自解释固件”不应作为独立飘走的平行路线，
它更适合作为 system compiler 对人类和工具暴露的观察面。

近程应先做只读 inspector / report / graph explain。

中程建议形成最小 explain surface：

- `cap list`
- `why unavailable`
- `graph path`
- `recent transitions`
- `resource summary`

它的定位不是“额外酷炫功能”，而是：

> **system compiler 的自然副产物。**

### 4.5 正式故障注入

目标：把坏世界从测试技巧升级为受管能力。

建议逐步把以下内容纳入正式注入能力：

- jitter
- timeout
- disconnect
- partial failure
- starvation

这条线未来应和托管时间、explain surface、bringup evidence 形成联动，
但近期不应先追求宏大统一平台。

## 5. 传奇路线与明确边界

### 5.1 多语义宿主

远景上，Charm 可以作为 host，逐步托管不同 guest world：

- POSIX guest
- USB class guest
- script guest（候选）

但这条线体量极大，当前只允许以边缘试点方式推进，
禁止它反向拖动主干架构。

### 5.2 全局确定性时间宇宙

这是远景，不作为近期承诺。

近期只允许推进局部托管时间，
不允许把“最终想象中的确定性宇宙”变成当前工程负担。

### 5.3 完整资源证明系统

这是长期野心，不应过早把仓库推向元系统泥潭。

近期目标只应限定为：

- 资源元数据
- 审计
- 局部检查

### 5.4 统一所有底层驱动实现

Charm 不应以“统一底层实现手法”为目标。

当前明确禁止：

- 为了统一而抹平底层现实差异
- 把所有控制器都强行塞进单一 discovery / probe 范式
- 让上层世界观倒逼底层实现必须长得一模一样

Charm 真正要统一的，是：

> **向上暴露的语义面。**

## 6. 当前最该避免的事

- 同时实装四条大维度
- 继续以“多一个模块”作为主要推进方式
- 让概念先于证据
- 过早滑向大规模 code generation
- 让传奇路线反向劫持主干

## 7. 当前主叙事

Charm 的中长期叙事不是：

> “又一个嵌入式框架。”

而是：

> **Charm 正在把嵌入式系统从“手工装配的工程对象”，推进为“可编译、可举证、可审计、可托管”的系统对象。**

这条叙事对当前仓库的要求也很明确：

- 先把主语言讲清楚
- 先把证据面做出来
- 先把边界写死
- 再逐步扩展能力面

它要求的不是“现在就实现最终宇宙”，
而是：

> **用极其清楚的边界，保护一个足够大的野心。**
