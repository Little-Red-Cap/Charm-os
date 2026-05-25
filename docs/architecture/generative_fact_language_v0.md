# 受控产生式事实语言 v0

本文不是 GMP 的引入提案，也不是新的代码生成框架说明。  
它用于定义 Charm 在当前阶段为什么需要一层受控的产生式事实语言，以及这层语言为什么应先走 `projector-first`。

它要回答的核心问题不是“怎样少写几千行代码”，而是：

> **当同一组系统事实开始被 schema、summary、导出脚本、inspect/explain 面同时消费时，Charm 应该用什么语言把它们收回同一个事实源。**

## 1. 为什么现在值得谈这件事

`docs/architecture/system_compiler_roadmap.md` 已经明确：

- Charm 当前主任务是把“系统如何成立”编译出来
- `system compiler v0` 优先做解释物，而不是全面 code generation
- `artifact report`、`resource contract`、`explain surface` 已经是正式主线的一部分

这意味着 Charm 当前面对的，不再只是“模块够不够多”，而是：

- 同一个系统事实有多少观察面
- 这些观察面是否共享事实源
- 这些观察面之间的同步是否仍靠人工维持

一旦这些问题出现，Charm 缺的就不是更多手写样板，而是：

> **一层受控的产生式事实语言。**

## 2. 它与 GMP 的关系是什么

GMP 的启发很重要，但 Charm 现在不该把 GMP 原样引入核心。

当前更准确的关系是：

- GMP 是设计样本
- 不是当前核心依赖候选

Charm 需要借鉴的是 GMP 背后的一个高阶范式：

> **单一事实源，可以投影成多个一致观察面。**

但 Charm 当前不该照搬的，是它作为通用 C++ 代码生成工具箱的整体形态。

当前判断固定为：

- 借 GMP 的方向
- 不借 GMP 的公共形态

## 3. 明确借什么

当前最值得借的，不是某个宏技巧，而是下面三件事。

### 3.1 借“单一事实源 -> 多投影”这个范式

Charm 当前最稀缺的，不是多一个生成工具，而是：

- 一份事实
- 多个一致投影

这份事实未来应能投影到：

- `artifact report`
- summary schema
- inspect / explain 输入
- compare 输入
- CI headline

### 3.2 借“显式 opt-in”的纪律

受控产生式事实语言不能假设“任意 struct 都自动进入系统语言”。  
Charm 更需要：

- 显式 opt-in
- 显式字段边界
- 显式投影关系

也就是说，进入这套机制的是“系统事实对象”，不是“所有 C++ 类型”。

### 3.3 借“生成能力服务于一致性，而不是炫技”

Charm 现在最需要的不是：

- 10 行宏生成 3000 行陌生代码

而是：

- 10 行事实声明生成多个一致结果面

这里的目标不是压行数，而是压重复事实税。

## 4. 明确不借什么

当前阶段，Charm 明确不借下面这些形态。

### 4.1 不把 GMP 作为核心依赖

原因不是 GMP 不好，而是 Charm 当前要求更特殊：

- 模块边界要审
- 诊断质量要审
- MCU 路径要审
- 公共接口稳定性要审

当前更好的策略是：

- 先在 Charm 内部定义自己的事实语言边界
- 未来如有必要，再吸收外部实现技巧

### 4.2 不先做宏框架

宏可以是局部打字机，但不能先成为系统语言。

Charm 当前不应走向：

- 到处靠宏写系统语义
- 宏直接暴露为公共宪法层接口

宏未来最多只应作为局部辅助，不应成为第一阶段的公共门面。

### 4.3 不先做“自动反射所有 struct”

这件事当前既不必要，也危险。

Charm 现在真正需要进入这套机制的，是：

- `artifact report` 相关事实
- `system_input` 相关事实
- `binding / bringup` 相关事实

而不是整个仓库的任意类型。

### 4.4 不先把 MCU runtime core 拉进来

第一阶段只适合落在：

- host/tool/report 层
- system compiler 结果物层

当前不应先污染：

- MCU critical path
- runtime hot path
- 驱动实时路径

### 4.5 不把第一阶段做成 codegen 工具秀

第一阶段的成功标准不是：

- 生成了很多代码

而是：

- 一份系统事实进入了多个稳定结果面

## 5. 为什么当前要 projector-first

当前路线建议是：

> **先做 projector-first，而不是一上来做 reflect-first。**

原因有三层。

### 5.1 当前最明显的痛点在投影层

从 `artifact report` 现状看，最先暴露问题的是：

- schema
- summary schema
- export object
- inspect / explain 面

这些问题首先是“结果物同步问题”，不是“类型发现问题”。

### 5.2 当前事实对象仍可先手写

Charm 当前完全可以先采用：

- 手写 canonical fact descriptor
- 手写 projection profile

而不必马上引入自动反射。

这能让第一阶段专注于：

- 什么是事实源
- 什么是投影关系
- 什么是稳定消费面

### 5.3 `reflect` 候选还需要更强边界

`reflect` 并没有被否定。  
它只是当前不该先占据主轴。

更准确地说：

- `projector-first` 是阶段性判断
- `reflect` 是中期候选

等第一批 canonical fact source 和 projection profile 站稳后，再决定是否需要单独的 `reflect` 门面，会更安全。

## 6. 当前建议的三层切法

这三层不是未来最终宇宙，只是当前阶段最合适的收敛方式。

### 6.1 `fact descriptor`

它负责：

- 定义单一事实源
- 显式列出字段
- 明确语义边界

它的要求是：

- 显式 opt-in
- 允许手写
- 不要求自动反射任意类型

它当前最接近的目标对象是：

- `subject`
- `system_input` 的局部子集
- `binding / bringup` 计数族

### 6.2 `projection profile`

它负责：

- 定义事实如何投影到不同结果面
- 区分 full report、summary、inspect、compare 输入
- 约束哪些字段出现在什么消费面

它当前应先服务于：

- `artifact report`
- `*_summary`
- `system_compiler_summary`
- inspect 默认摘要或 case list

### 6.3 `consumer surface`

它负责：

- 人和工具真正看到的工件面
- 查询面
- CI headline

这一层不是事实本身，而是事实的消费出口。

## 7. 当前阶段明确不做什么

这一条必须说重一点。

当前阶段不做：

- 不引 GMP 核心依赖
- 不先发明新的配置 DSL
- 不先发明公共宏接口
- 不先要求 compare mode 全量统一自动化
- 不先把 `resource_contract / fact_resolution` 全量卷入
- 不先把运行时核心路径放进这套机制

这些不是长期否定，而是当前主动收窄。

## 8. 当前结论

这轮架构碰撞之后，当前最稳的判断是：

> **Charm 现在需要的不是通用反射框架，而是一层受控的产生式事实语言。**

并且这层语言当前应先走：

> **projector-first**

更具体地说：

- 第一阶段先定义 canonical fact source
- 先定义 projection profile
- 先把 `artifact report` 这条成熟结果物链收拢
- `reflect` 保留为中期候选，不作为当前试点前提

这样做的目标不是“减少多少代码”，而是：

> **把同一份系统事实稳定地投影成 report / summary / inspect / CI 这些一致结果面。**
