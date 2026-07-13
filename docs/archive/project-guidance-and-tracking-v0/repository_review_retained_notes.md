# 早期全仓 Review 保留笔记

> status: `archived`
>
> scope: 旧全仓 backlog 中仍可复用的结构风险与拆分方法，不表示当前任务状态

旧 review 曾按文件行数、当时路径和 P0/P1/P2 排列任务。这些快照已失效，不能用于判断当前实现。
现状必须重新检查 source、CMake、真实 target 与当次测试。本文只保留跨版本仍成立的审查问题。

## 聚合入口

入口模块宽并不自动构成问题；风险在于消费方无法解释它为何需要整组导出，或稳定入口持续吸收
产品、兼容和内部实现。审查时应区分：

- 稳定消费入口；
- 迁移兼容入口；
- 产品或场景聚合；
- 已退役的 tombstone；
- 仅供内部组装的实现面。

先用真实 import 与 target wiring 找消费者，再决定拆分。不能仅按 export 数量下结论。

## 复合职责文件

大文件应按变化原因和失败边界拆，不按行数机械切片。旧 review 中反复出现的有效切法包括：

- schema / storage / executor；
- public facade / builder / runtime payload；
- core state machine / trace and formatting；
- protocol framing / command semantics / backend bridge；
- lifecycle storage / input dispatch / semantic policy；
- production implementation / global C bridge / disabled stub。

第一刀应尽量只搬家、不改行为，保留 public module 名称和调用边界。拆分后先补局部 smoke、exhaustive
dispatch 和失败路径，再评估是否需要缩小 public facade。

## 核心逻辑与观察面

调度器、协议状态机或 runtime 可以拥有最小 trace 数据，但复杂文本、JSON、CSV 和报告格式不应反向
成为核心依赖。观察模块读取稳定快照或事件，核心逻辑不依赖具体输出 sink。

同理，debug self-check 和 synthetic provider 可以有价值，但不应挂在正式业务 API 上。它们应进入内部
验证模块、host smoke 或显式 debug target，避免扩大生产导出面。

## Bridge、Stub 与真实实现

将真实 backend、C 风格全局桥接和 feature-disabled stub 放在同一模块，会混淆 ownership、初始化顺序
和失败语义。更稳定的切分是：

- core adapter 负责真实协议或库语义；
- bridge 只拥有外部 ABI、全局 callback 或 trampoline；
- stub 明确表达 feature unavailable，不伪装成功路径。

配置开关应由 target 选择实现，不在业务逻辑中铺开大量条件编译。

## `void* + ops` 边界

`void* context + ops table + trampoline` 可提供静态分配和 ABI 隔离，但生命周期与类型约束依赖人工
维护。它适合 C ABI、vendor callback、跨语言或异构边界；普通 C++ 组合默认先考虑 typed ref、concept
或局部 provider view。

使用类型擦除时至少要说明：

- context 的所有者和有效期；
- ops table 是否静态且完整；
- 空操作、解绑和并发调用行为；
- 错误如何穿过 trampoline；
- 为什么 typed boundary 不适用。

## 分刀顺序

1. 重新核对当前路径、消费者、构建目标与失败证据。
2. 固定或记录现有 public surface，先做无语义变化的内部拆分。
3. 为拆出的窄边界补局部 contract 与 smoke。
4. 将迁移、接口变化和旧文件清理分成独立提交。
5. 最后才调整 public naming 或建立新聚合入口。

新治理任务应从当前代码证据重新建立。
