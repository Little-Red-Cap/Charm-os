# Compiler World Lifecycle v0

本文是 `Charm Compiler Constitution v0` 与 `Compiler Pass Authority & Semantic Freeze Boundary v0` 的下游 lifecycle law。

它定义 compile-time world 从 source facts 进入、被证明、被冻结、被 lowering、被 witness、被观察、被归档、被比较的状态语言。本文不定义 canonical identity，不实现 observation import，不新增 schema、validator、smoke、IR 类型或 LLVM/MLIR 接入。
现有 artifact/report/witness/ledger/compare 到 lifecycle states 的只读映射见：[`compiler_world_lifecycle_projection_v0.md`](compiler_world_lifecycle_projection_v0.md)。

## 1. 定位

`World Lifecycle` 回答的问题是：

> **一个 Charm semantic world 在什么阶段成立、冻结、交付、被观察和被比较。**

它不回答：

- world 的节点结构是什么
- canonical identity 如何编码
- fork id / hash / node ref 如何生成
- observation import pass 如何实现
- artifacts 如何序列化

这些都属于后续实现或更低层 contract。

## 2. Lifecycle States

v0 先承认九个 lifecycle states：

```text
declared
  -> materialized
  -> proven
  -> frozen
  -> lowered
  -> witnessed
  -> observed
  -> archived
  -> compared
```

这条链不是强制所有 world 都必须线性经过每一步。它是判断 world facts 是否有权进入某个阶段的共同语言。

## 3. `declared`

`declared` 表示 source facts 进入 world。

典型来源包括：

- `SystemSpec`
- `Profile`
- `BoardPackage`
- `BoardCaps`
- declared resource facts
- declared contracts
- manifest / case input facts

`declared` 不等于事实已经成立。它只表示事实已经被 world 接收为输入侧声明。

## 4. `materialized`

`materialized` 表示 facts 被规范化成可观察、可执行或可导出的 world surface。

典型胚胎包括：

- materialized graph
- binding result
- bringup order
- artifact report projection
- normalized topology surface

`materialized` 允许 world 从“输入意图”进入“系统承认的结构”。但它仍不自动等于 legality 已证明。

## 5. `proven`

`proven` 表示 legality、verifier 或 evidence rule 已经证明某些 facts 成立。

典型胚胎包括：

- resource legality satisfied
- contract-required facts satisfied
- runtime session standing
- witness entry ok
- violation-free proof summary

`proven` 必须由 authorized verifier 或既有 evidence contract 产生。report、inspector、compare consumer 不得为了展示便利伪造 proven facts。

## 6. `frozen`

`frozen` 表示 semantic truth 已进入 freeze boundary。

进入 `frozen` 后：

- topology identity must remain stable
- resource ownership must not change
- legality proof must remain valid
- semantic fact identity must remain traceable
- later passes must not mutate semantic truth

如果 freeze 后需要改变 semantic truth，必须 fork new world，而不是原地修改 frozen world。

## 7. `lowered`

`lowered` 表示 frozen world 被投影成 target artifact 或 observation surface。

典型 lowering surfaces 包括：

- firmware
- docs
- metadata
- inspector surface
- witness bundle
- capability matrix
- compare-ready summary

`lowered` 必须遵守 lossy lowering law：任一 lowered artifact 都不等于完整 world truth。它必须尽可能保留回指 world facts 的 semantic identity 或 provenance。
Lowering surfaces 的只读投影责任见：[`compiler_lowering_surface_contract_v0.md`](compiler_lowering_surface_contract_v0.md)。

## 8. `witnessed`

`witnessed` 表示 lowered artifact 或 observed behavior 已被 witness 回指到 world facts。

`witnessed` 不等于重新判定 semantic truth。它表示：

- artifact 能解释自己来自哪些 world facts
- observed behavior 能被已有 facts / verdicts / summaries 解释
- witness 没有越权修改 world

如果 witness 发现缺失或 collapse，它只能报告、归因或形成 proposal，不能原地修补 frozen world。

## 9. `observed`

`observed` 表示 runtime、smoke、inspector 或 evidence harness 产生了稳定观察。

典型来源包括：

- QEMU lower-half evidence
- runtime ledger facts
- host smoke summaries
- bringup observed state
- inspector-readable surfaces

`observed` 是事实观察，不是 semantic mutation。observed evidence 不得直接修改 frozen world。

## 10. `archived`

`archived` 表示 world 与 paired artifacts 被保存为可复盘交付。

健康的 archive 不应只包含 binary。它应尽量保留：

- firmware or target artifact
- world summary
- witness bundle
- evidence summary
- topology/capability metadata
- symbolic/debug/provenance map

`archived` 的目标是让系统事实可复盘，而不是只证明某次构建产出了文件。
未来 archive manifest 的只读归档证明责任见：[`compiler_archive_manifest_contract_v0.md`](compiler_archive_manifest_contract_v0.md)。

## 11. `compared`

`compared` 表示两个 exported worlds、witness bundles 或合法 surfaces 被比较。

`compared` 只允许比较已导出的 surfaces。它不得：

- 重跑 lower brain
- 重新执行 builder / deriver / verifier
- 直接读取 raw runtime evidence 来替代 witness
- 创建新的 semantic truth

如果 comparison 发现 drift 或 collapse，它只能产出 compare result、failure surface 或 source fact proposal。

## 12. Observation Import Boundary

Observation 可以提出 new facts，但不能原地改写 semantic truth。

正确方向是：

```text
observed runtime evidence
  -> source fact proposal
  -> new semantic world branch
  -> new proof
  -> new freeze
  -> new lowering / witness / compare
```

错误方向是：

```text
observed runtime evidence
  -> mutate frozen world
```

`Observation Import Pass` 未来可以成为 authorized pass，但 v0 不实现它，也不定义 proposal schema、canonical identity、fork id、hash 或 storage model。

## 13. 回流规则

v0 回流规则如下：

- `observed` may create `source fact proposal`
- `source fact proposal` may seed a new world branch
- `compared` may report drift/collapse or produce proposal context
- `witnessed` may report missing provenance or collapse
- none of them may mutate frozen world in place

这让 Charm 保留 adaptive intelligence 的入口，同时不破坏 semantic determinism。

## 14. 与现有 evidence state 的关系

`bringup_evidence_pipeline_v0.md` 中的 `declared / materialized / published / observed / failed / blocked` 是当前最成熟的 lifecycle 胚胎。

本文不替代 bringup evidence pipeline。它只把其中已经稳定的词提升为 compiler lifecycle 的一部分，并补上 `proven / frozen / lowered / witnessed / archived / compared` 这组 world-level 状态。

## 15. 非目标

本 v0 不做：

- 不定义 canonical identity。
- 不实现 observation import pass。
- 不新增 schema、validator、smoke 或脚本。
- 不新增 `World IR`、`TopologyIR`、`Node` 或 C++ 类型。
- 不新增 Clang plugin、LLVM pass 或 MLIR dialect。
- 不定义 fork id、hash、node ref、serialization 或 storage model。
- 不改变现有 witness bundle、artifact report、world compare、bringup evidence 或 runtime ledger 的字段与判决模型。
