# Canonical World v0

这份文档不是在发明又一套 demo 分类法。

它要回答的是：

> 当 Charm 开始把系统当成“可作证世界”而不是“能跑的程序堆”时，
> `Examples` 里的样本该怎样升级成正式世界对象。

## 一句话版本

- `case` 证明一条 seam。
- `canonical world` 把多条 seam、合同和问题面收成一个可复盘对象。
- 它不是替代单点 verifier，而是把单点 verifier 组织成一个“这个世界为何成立”的叙述单元。

## 为什么现在值得收这一层

当前仓库已经有很多高价值样本：

- `runtime_*_host`
- ARMv7-A lower-half QEMU smoke
- `artifact report`
- `runtime evidence bundle`

但它们仍然大多按“某一条证据线”或“某一个脚本入口”组织。

这会留下两个缺口：

- 人知道这些样本都很重要，但不知道它们共同在证明哪个世界
- compare 漂移之后，很难第一时间回答“这次漂移打碎的是哪一个世界”

`canonical world` 的职责，就是把这些样本先升格成：

- 一个世界名
- 一组核心问题
- 一组比较问题
- 一组合同引用
- 一份 witness 计划

## 当前对象边界

当前 `canonical world` 对应：

- schema：
  - `schemas/system_compiler.canonical_world.v0.schema.json`
- sample：
  - `schemas/examples/system_compiler.canonical_world.v0.sample.json`
- 实际样本目录：
  - `Examples/kernel/canonical_worlds/`

当前最小字段只收这些：

- `name / title / summary`
- `subject`
- `first_class_terms`
- `core_questions`
- `compare_questions`
- `contract_refs`
- `witness_plan`

其中 `witness_plan` 当前只支持四类 witness：

- `artifact_report`
- `runtime_evidence_bundle`
- `kernel_runtime_session`
- `example_ref`

这是刻意收敛的。

当前阶段不急着把所有“可引用东西”都拉成 witness kind，
而是先覆盖：

- system compiler 报告对象
- 最小内核运行时总证据包
- 最小内核运行会话 witness
- 代表性样本目录

## 当前语义

### 1. world 不替代 case

`case` 仍然是单点证据对象。

`canonical world` 只是声明：

- 哪些 case / bundle / example 共同组成这个世界的 witness 面

### 2. world 不替代 contract

世界不是法律本身。

它只是显式引用：

- 这个世界成立时依赖哪些 contract / contract-like 入口

因此当前 `contract_refs` 更像世界的法律锚点，
而不是把合同文本重新抄一遍。

### 3. world 先回答“我在证明什么”

当前比“把字段做大做全”更重要的是，
每个 world 至少能稳定回答：

- 它想证明什么
- 它最怕什么漂移
- 它依赖哪些 witness

也就是说，`core_questions` 和 `compare_questions`
在 v0 里不是可有可无的注释，
而是世界对象最有价值的语义压缩。

### 4. world 是 compare 的锚点，不是 compare 本身

`canonical world` 本身不输出 drift verdict。

它只负责提供：

- 世界名
- 核心问题
- compare 问题
- contract refs
- witness plan

真正的 baseline / candidate 对照，
由后续 `world compare` 对象来完成。

## 当前推荐读法

如果你正在看“最小内核 runtime / syscall / trap 为什么已经开始像一个世界”，
优先读：

- `Examples/kernel/canonical_worlds/minimal_kernel_runtime.world.json`
- `docs/system/minimal_kernel_runtime_evidence_bundle_contract.md`
- `docs/system/world_compare_v0.md`
- `docs/system/minimal_kernel_runtime_evidence_matrix.md`
- `docs/system/minimal_kernel_task_message_session_roundtrip_contract.md`
- `docs/system/minimal_kernel_trap_ingress_contract.md`

## 当前非目标

当前这层仍然不处理：

- 自动生成所有 world
- 自动做全量 compare 审判
- 把所有 `Examples/*` 一次性升级
- 运行时交互式世界浏览器

v0 更克制的目标只有一个：

> 先让 Charm 能把“这一组样本共同证明了一个怎样的世界”说成正式对象。
