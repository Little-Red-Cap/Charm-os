# Compiler Pass Authority & Semantic Freeze Boundary v0

本文是 `Charm Compiler Constitution v0` 的下游执法细则。

它只定义 pass authority、semantic freeze、identity preservation、world fork、lossy lowering 与 paired artifact 的法律边界，不定义 `World IR` 数据结构、schema、pass runner、LLVM/MLIR dialect 或 codegen pipeline。
World lifecycle 与 observation import boundary 的状态语言见：[`compiler_world_lifecycle_v0.md`](compiler_world_lifecycle_v0.md)。
未来 freeze receipt 的只读证明责任见：[`compiler_freeze_receipt_contract_v0.md`](compiler_freeze_receipt_contract_v0.md)。

## 1. 定位

`Charm Compiler Constitution v0` 先定义 compile-time world 的总法理。

本文继续回答一个更窄的问题：

> **谁有权改变 world，什么时候必须停止改变 world，freeze 后如果还想改变语义应该怎么办。**

这一步先于 `World IR` shape。世界的实现形状可以迭代，但改变世界的权限模型必须先收住。

## 2. Pass Authority Matrix

v0 先承认七类 pass authority：

| Pass | May read | May write | Must not |
| --- | --- | --- | --- |
| `Builder` | source inputs, board/profile facts | initial source/materialized facts | invent observed facts or proven facts |
| `Deriver` | source and derived facts | derived facts, annotations, normalized projections | silently redefine semantic identity |
| `Verifier` | semantic world and derived facts | proven facts, violations, proof summaries | mutate semantic truth to make proof pass |
| `Lowering` | frozen semantic world | target artifacts, lowered metadata, symbolic maps | change frozen topology/resource/legal identity |
| `Witness` | world, artifacts, summaries, evidence surfaces | witness artifacts, provenance summaries | rerun lower-layer judgment or mutate world truth |
| `Inspector` | exported world/artifact/witness surfaces | human/tool reading surfaces | become a second truth source |
| `Compare` | exported baseline/candidate artifacts or witnesses | compare reports and drift/collapse summaries | re-execute builder/deriver/verifier logic |

`Witness`、`Inspector`、`Compare` 默认是 read-only semantic consumers。它们可以解释、投影、引用和比较已经导出的 facts，但不能反向修改 world truth。

## 3. Identity Preservation Law

Passes may derive, annotate, materialize, lower, or observe facts.

Passes must not silently redefine semantic identity.

这条 law 约束的是“同一个东西为什么还是同一个东西”。例如：

- `UART1 exists` 是 source identity。
- `UART1 routed through DMA2_CH3` 可以是 derived fact。
- `UART1 topology normalized` 不得偷偷把 `UART1` 改成另一个 semantic identity。

在 semantic freeze 之后，任何 pass 都不得重写：

- topology identity
- resource identity
- ownership identity
- legality identity
- fact identity used by witness / inspector / compare

如果确实需要改变这些身份，必须进入新的 semantic world，而不是在旧 frozen world 上继续改写。

## 4. Semantic Freeze Boundary

`Semantic Freeze Boundary` 是 world 从 “semantic construction” 进入 “lowering / witness / observation” 的边界。

freeze 前允许：

- builder 建立 source/materialized facts
- deriver 派生 normalized facts
- verifier 产出 proven facts 或 violations
- semantic world 根据合法 pass 继续演化

freeze 后禁止：

- 改写 topology identity
- 改写 resource ownership
- 让旧 legality proof 继续套用到已经改变的 semantic world
- 让 witness / inspector / compare 反向污染 semantic truth
- 让 lowering 改变 semantic identity

freeze 后允许：

- lowering 到 firmware/docs/metadata
- 附加 witness/debug/inspector metadata
- 生成 evidence、report、compare surface
- 在不改写 semantic truth 的前提下建立 artifact provenance

v0 不定义 freeze 的触发命令、hash、node id、storage model 或 transaction implementation。

## 5. World Fork Rule

freeze 后如果仍需要改变 semantic truth，正确动作不是继续修改 frozen world，而是 fork new semantic world。

```text
World A (frozen)
  -> firmware A
  -> witness A

World B (forked from A with new semantic facts)
  -> firmware B
  -> witness B
```

`World Fork` 是法律规则，不是 v0 接口。

本文不定义：

- fork id
- hash
- node ref
- storage backend
- serialization format
- distributed build protocol

它只规定：frozen world 的 semantic identity 不应被原地重写。需要新语义时，应形成可比较、可见证的新 world。

## 6. Lossy Lowering Law

Lowering must be lossy by declaration.

compile-time world 必然比任一 target artifact 更 rich。`firmware.bin`、docs、inspector surface、capability matrix、witness report 都只能携带 world truth 的某个投影。

因此：

- firmware artifact 不等于完整 world truth
- docs artifact 不等于完整 world truth
- inspector surface 不等于完整 world truth
- witness bundle 不等于完整 world truth，但必须能回指 world facts
- compare surface 不等于完整 world truth，只比较已导出的合法 surfaces

Lowering 的职责不是把所有 world truth 塞进一个 artifact，而是显式承认丢失了哪些语义层，并保留可追踪回 world 的身份与 provenance。

## 7. Paired Artifact Rule

Charm 的系统事实不应由 binary-only artifact 独占。

更健康的交付单位应被理解为 paired artifact：

```text
firmware
+ world summary
+ witness bundle
+ evidence
+ topology/capability metadata
+ symbolic/debug/provenance map
```

这些名字不是 v0 schema 或文件名承诺。它们表达一条法律：

> **firmware is lowered residue; witness and metadata carry the semantic world needed to understand it.**

如果某个运行产物不能回指 semantic world，它只能证明“某个 binary 存在”，不能完整证明“这个系统世界成立”。
未来 archive manifest 的只读归档证明责任见：[`compiler_archive_manifest_contract_v0.md`](compiler_archive_manifest_contract_v0.md)。

## 8. 典型越权

以下行为在 v0 中视为越权：

- witness exporter 重新解析 raw evidence 并覆盖 session verdict
- inspector 根据 UI 选择反向改变 source facts
- compare 重新执行 lower-layer builder / verifier 逻辑
- lowering 为了目标方便改写 resource ownership
- deriver 在未声明 fork 的情况下改变 frozen topology identity
- report 产物成为第二套 canonical truth

这些行为如果未来确实需要，应通过新的 authorized pass 或 new semantic world 明确建模，而不是在当前 pass 中暗改。

## 9. 与 Constitution 的关系

本文不替代 `Charm Compiler Constitution v0`。

Constitution 定义：

- world 是什么
- fact 是什么
- witness 为什么是一等 artifact
- lowering 为什么必须保留 semantic identity
- single semantic world 为什么重要

本文细化：

- pass 权限
- identity preservation
- freeze 前后边界
- frozen world 如何 fork
- lowering 为什么必须承认 loss
- firmware 为什么需要 paired witness/metadata

## 10. 非目标

本 v0 不做：

- 不新增 schema、validator、smoke 或脚本。
- 不新增 `World IR`、`TopologyIR`、`Node` 或 C++ 类型。
- 不新增 pass runner。
- 不新增 Clang plugin、LLVM pass 或 MLIR dialect。
- 不定义 fork id、hash、node ref、serialization 或 storage model。
- 不改变现有 witness bundle、artifact report、world compare、bringup evidence 的字段或判决模型。
- 不把 C++ template、YAML、JSON 或 generator 提升为唯一 truth source。
