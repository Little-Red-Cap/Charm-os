# Artifact Report 最小投影试点 v0

本文不是新的元编程框架设计，也不是第一阶段的代码实现说明。  
它用于定义 Charm 在 `projector-first` 路线上，第一个足够小、足够硬、足够不易失控的试点。

它要回答的核心问题不是“未来整套事实语言长什么样”，而是：

> **如果现在就要在 `artifact report` 链上试一次受控投影，第一刀应该从哪组事实下手。**

## 1. 试点目标

第一试点只做一件事：

> **证明一份受控事实描述，可以稳定投影成多个一致观察面。**

成功标准不是：

- 自动反射了很多类型
- 生成了很多代码
- 一次统一整个 `artifact report`

成功标准是：

- 找到一份 canonical fact source
- 用它稳定投影到多个现有结果面
- 且不先污染当前主线

## 2. 试点范围

当前试点范围固定为两组字段族：

### 2.1 `subject` 族

字段固定为：

- `case`
- `profile`
- `board`
- `active_facets`

这组字段的优势是：

- 语义稳定
- 当前多处重复
- 与输入、摘要、inspect 都有直接关系

### 2.2 `binding_result / bringup_order` 计数族

字段固定为：

- `required_binding_count`
- `resolved_binding_count`
- `unresolved_binding_count`
- `ordered_node_count`
- `blocked_node_count`

这组字段的优势是：

- 已经在多个 summary 与 `system_formation` 中重复出现
- 足够小
- 对 explain / CI / compare 价值都高

## 3. canonical fact source

第一试点不要求自动从任意实现对象发现字段。  
canonical fact source 当前固定采用：

- 显式定义
- 手写字段描述
- 显式声明哪些字段进入哪个投影面

第一试点中的 canonical fact source 只需支持两类对象：

- `artifact report` 的 `subject`
- `artifact report` 的 `binding / bringup` 最小计数摘要

这里的关键纪律是：

- 同一事实源先定义字段名
- 再定义字段语义
- 再定义哪些消费面能看见它

而不是让每个消费面各自重新拼一遍对象。

## 4. 固定投影面

第一试点固定只覆盖 4 个投影面。

### 4.1 `artifact report` 顶层对应子对象

覆盖：

- `subject`
- `binding_result` 最小计数
- `bringup_order` 最小计数

### 4.2 summary schema 对应摘要对象

覆盖：

- `system_input_summary`
- `binding_result_summary`
- `bringup_order_summary`

这里的目标不是统一整个 summary schema，而是让对应子对象来自同一份事实描述。

### 4.3 `system_compiler_summary` 聚合摘要片段

当前只覆盖与第一试点字段直接对应的聚合片段：

- case context
- unresolved binding headline
- blocked node headline

不扩展到与 `resource_contract / fact_resolution` 紧耦合的段落。

### 4.4 inspect 默认摘要或 `-ListCases` 一类只读消费面

目标是保证：

- `inspect` 不再需要对这组字段重新发明自己的语义
- 只做展示和聚合，不重新定义事实

## 5. 第一试点明确不做什么

这部分必须钉死，避免实现时膨胀。

当前不做：

- `resource_contract / fact_resolution` 全量接入
- compare mode 全量字段自动化
- `runtime_observe` 全量对象统一
- 从任意 C++ 类型自动推 schema
- 新的配置 DSL
- 公共宏接口
- MCU runtime core 接入

这里的核心边界是：

> **第一试点只处理已经在 `artifact report` 链中成熟、重复、且相对稳定的字段。**

## 6. 为什么当前不先碰 `resource_contract / fact_resolution`

不是因为它们不重要，而是因为它们现在更深地卷入了：

- 法律文本
- fact inventory
- hotspot 推导
- contract state
- compare 漂移

如果第一波就一起推进，会把试点从：

- “受控投影试点”

拖成：

- “合法性层总改造”

这不符合当前阶段收窄原则。

## 7. 验收标准

第一试点的验收标准固定为：

- 能明确指出哪一份对象是 canonical fact source
- 能明确指出 4 个投影面各自消费了哪些字段
- 这组字段不再需要在每个消费面重新定义一遍语义
- 实现侧即使暂时仍有手写，也必须有明确的单一事实源与投影关系
- 第一阶段不依赖自动反射
- 第一阶段不依赖 GMP
- 第一阶段不触碰 MCU critical path

如果试点完成后仍然回答不了下面这三个问题，就算失败：

- `subject` 到底是谁说了算
- `binding / bringup` 计数到底谁是单一事实源
- inspect / summary / report 到底是在展示事实，还是在重新创造事实

## 8. 下一步真正实现时的第一刀

如果进入实现阶段，第一刀默认顺序固定为：

1. 先统一 `subject`
2. 再统一 `binding_result / bringup_order` 计数族
3. 最后再评估是否需要把 `system_input` 更大子集拉进来

这条顺序的意义在于：

- 先拿最稳定、最通用的 case context 建立 canonical fact source
- 再拿最像“重复计数税”的 binding / bringup 计数证明投影价值
- 先不让试点卷入更复杂的合法性与 compare 语义

## 9. 当前实现状态

截至当前实现：

- `subject` 族已经完成第一刀，export 与 inspect 侧都已有单一 projection path
- `binding_result / bringup_order` 计数族进入第二刀，当前实现以内部 projection context 统一 count side
- 这两刀仍然都保持 `projector-first`，不引入 GMP、宏 DSL 或通用反射门面

## 10. 本轮结论

`Artifact Report` 第一试点当前已经足够明确：

- 范围小
- 价值高
- 与现有主线贴合
- 不要求先引入反射门面

所以当前最稳的推进方式是：

> **先用 `subject` 与 `binding / bringup` 计数族，证明 Charm 可以把一份系统事实稳定投影成多个一致结果面。**
