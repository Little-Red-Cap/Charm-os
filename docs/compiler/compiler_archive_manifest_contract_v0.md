# Compiler Archive Manifest Contract v0

本文是 `Compiler World Lifecycle v0`、`Compiler Lifecycle Projection Coverage Matrix v0` 与 `Compiler Pass Authority & Semantic Freeze Boundary v0` 的下游 contract。

它定义未来 archive manifest 应承担的法律责任：证明 world 与 paired artifacts 已被作为一组可复盘交付保存，并说明该归档证明面如何被 witness、inspector、compare、projection 与 coverage matrix 消费。

本文不实现 `compiler_archive_manifest.json`，不定义 schema、字段、hash、world id、artifact id、storage model、retention policy、validator、exporter、smoke、C++ 类型、IR 或 LLVM/MLIR 接入。

## 1. 定位

`Compiler Archive Manifest` 是 paired artifact 的只读归档证明面。

它回答的是：

> **什么东西证明 world 与 paired artifacts 被作为一组可复盘系统事实保存。**

Archive manifest 不是：

- 新的 truth owner。
- 新的 world schema。
- 新的 artifact id 机制。
- 新的 canonical identity 机制。
- 新的 storage backend。
- 新的 retention policy。
- 新的 compare verdict。
- 新的 lower brain。

它只能保存和引用已经导出的合法 surfaces。它不能替代 constitution、authority law、lifecycle law、freeze receipt、witness bundle 或 compare 的原有判决模型。

## 2. Manifest Responsibility

未来 archive manifest 至少应承担这些法律责任：

- 证明 world 与 paired artifacts 被作为一组可复盘交付保存。
- 说明 archive 可以回指 world summary。
- 说明 archive 可以回指 target artifacts，例如 firmware 或其他 lowered residue。
- 说明 archive 可以回指 witness bundle。
- 说明 archive 可以回指 evidence summary。
- 说明 archive 可以回指 topology / capability metadata。
- 说明 archive 可以回指 symbolic / debug / provenance map。
- 证明 binary-only artifact 不足以代表完整 system truth。
- 证明 archive manifest 只能保存与引用已导出的 surfaces。
- 证明 archive manifest 不得重跑 verifier、freeze、lowering、witness 或 compare。

这些责任是 contract 级责任，不是 v0 字段承诺。

## 3. Paired Artifact Law

Charm 的系统事实不应由 binary-only artifact 独占。

一个健康的 archive 应被理解为 paired artifact collection：

```text
target artifacts
+ world summary
+ freeze receipt when available
+ witness bundle
+ evidence summary
+ topology/capability metadata
+ symbolic/debug/provenance map
+ compare-ready exported surfaces when available
```

这些名字不是 v0 schema 或文件名承诺。它们表达一条法律：

> **archive preserves the context needed to understand lowered residue.**

如果某个交付物只保存 binary，它只能证明“某个产物存在”。它不能完整证明：

- 这个 binary 来自哪个 semantic world。
- 这个 world 是否经过合法 freeze。
- 这个 artifact 如何回指 witness。
- 运行时或 bringup evidence 如何解释它。
- 后续 compare 应该比较哪些 exported surfaces。

## 4. Manifest Authority

Archive manifest 可以由未来 authorized archive/export pass 产生。

v0 不定义该 pass 的实现、命令、runner、schema 或 storage model。但它定义 authority 边界：

| Actor | May consume manifest | May create manifest | May mutate semantic truth through manifest |
| --- | --- | --- | --- |
| `Builder` | yes | no | no |
| `Deriver` | yes | no | no |
| `Verifier / Freeze pass` | yes | no | no |
| `Lowering` | yes | no | no |
| `Witness` | yes | no | no |
| `Inspector` | yes | no | no |
| `Compare` | yes | no | no |
| `Projection / Coverage matrix` | yes | no | no |
| `Archive / Export pass` | yes | yes | no |

Issued archive manifest 应被视为 immutable archive surface。若 archive 内容需要变化，正确动作是产生新的 archive manifest，而不是让旧 manifest 伪装成同一次归档事实。

## 5. Relationship to Lifecycle and Coverage

`Compiler World Lifecycle v0` 定义 `archived` 是 world lifecycle state。

`Compiler Lifecycle Projection Coverage Matrix v0` 当前将 `archived` 标记为：

```text
coverage strength: weak to medium
projection kind: interpretive
sidecar eligibility: optional
```

本文承接这个弱覆盖，但不补实现。

换句话说：

- coverage matrix 暴露 `archived` 只有 witness bundle 与 paired artifact direction 的间接支撑。
- archive manifest contract 定义未来 manifest 应承担什么责任。
- 未来 sidecar 可以提升 archive projection 的审计清晰度。
- 当前 v0 仍不新增 sidecar 文件或字段。

## 6. Consumer Rules

任何消费 archive manifest 的对象都必须遵守只读规则。

### 6.1 Witness consumer

Witness 可以引用 archive manifest 来说明 witness bundle 被保存为 paired artifact collection 的一部分。

Witness 不得通过 manifest：

- 补写 missing witness。
- 修改 witness verdict。
- 重新解析 raw logs。
- 创建新的 source facts。

### 6.2 Inspector consumer

Inspector 可以展示 archive manifest、paired artifact collection 与可复盘入口。

Inspector 不得根据 UI 操作反向修改 archive manifest、semantic truth、witness truth 或 compare result。

### 6.3 Compare consumer

Compare 可以读取 archive manifest 来定位 exported witness bundles 或合法 compare surfaces。

Compare 不得通过 manifest：

- 重跑 builder / deriver / verifier。
- 重跑 freeze pass。
- 重跑 lowering。
- 重跑 witness exporter。
- 修复 compare drift 或 collapse。
- 创建新的 semantic truth。

### 6.4 Projection and coverage consumer

Projection 与 coverage matrix 可以读取 archive manifest 来提升 `archived` coverage strength。

它们不得让 coverage 状态成为新的 archive truth。Coverage 只能审计 archive 是否有镜头，不能成为归档事实本身。

## 7. Future Sidecar Direction

未来可以考虑一个只读 sidecar：

```text
compiler_archive_manifest.json
```

这个名字只是方向，不是 v0 接口。

若未来实现 sidecar，它必须保持：

- read-only archive proof surface
- no raw log parsing
- no lower brain rerun
- no verdict mutation
- no source fact creation
- no canonical identity definition by accident
- no observation import implementation by accident
- no storage model commitment by accident
- no retention policy commitment by accident

本文不定义：

- JSON schema
- required fields
- hash algorithm
- world id
- artifact id
- storage model
- retention policy
- serialization
- validator
- exporter
- smoke

## 8. Typical Violations

以下行为在 v0 中视为越权：

- archive exporter 重新解析 raw logs 并覆盖 witness verdict
- archive manifest 补写不存在的 witness bundle
- inspector 根据用户选择改写 archive manifest truth
- compare 发现 drift 后修改 archive manifest 或 frozen world
- projection coverage matrix 把 `archived` 从 `weak` to `medium` 改成 `strong`，但没有实际 manifest 或等价 archive proof surface
- archive manifest 定义自己的 canonical identity、artifact id 或 storage truth
- runtime observation 直接更新 manifest 来创建新 source facts

这些行为如果未来确实需要，应通过新的 authorized pass、new semantic world 或新的 archive manifest 明确建模。

## 9. 非目标

本 v0 不做：

- 不新增 JSON schema。
- 不新增 `compiler_archive_manifest.json` 文件。
- 不新增 validator、exporter、smoke 或脚本。
- 不新增 C++ 类型或 IR。
- 不定义 canonical identity。
- 不定义 world id、artifact id、hash、node ref、serialization、storage model 或 retention policy。
- 不实现 observation import pass。
- 不定义 semantic debugging protocol。
- 不接入 LLVM/MLIR。
- 不改变现有 artifact report、bringup evidence、runtime ledger、witness bundle、world compare 的字段或判决模型。
