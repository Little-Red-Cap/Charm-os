# Compiler Freeze Receipt Contract v0

本文是 `Compiler Pass Authority & Semantic Freeze Boundary v0` 与 `Compiler Lifecycle Projection Coverage Matrix v0` 的下游 contract。

它定义未来 freeze receipt 应承担的法律责任：证明某个 semantic world 已进入 `frozen` 边界，并说明该证明面如何被 lowering、witness、projection、coverage matrix 与 compare 消费。

本文不实现 `compiler_freeze_receipt.json`，不定义 schema、字段、hash、node ref、canonical identity、storage model、validator、exporter、smoke、C++ 类型、IR 或 LLVM/MLIR 接入。

## 1. 定位

`Compiler Freeze Receipt` 是 frozen world 的只读证明面。

它回答的是：

> **什么东西证明 semantic freeze point 已经发生。**

Freeze receipt 不是：

- 新的 truth owner。
- 新的 world schema。
- 新的 canonical identity 机制。
- 新的 proof engine。
- 新的 compare verdict。
- 新的 observation import pass。
- 新的 lower brain。

它只能证明 authorized freeze 已经把 world 从 semantic construction 推入 freeze boundary。它不能替代 constitution、authority law、lifecycle law 或实际 verifier 的判决。

## 2. Receipt Responsibility

未来 freeze receipt 至少应承担这些法律责任：

- 证明 semantic freeze point 已发生。
- 证明 topology identity 已进入不可原地语义改写边界。
- 证明 resource ownership 已进入不可原地语义改写边界。
- 证明 legality proof 不得在 semantic truth 被改写后继续复用。
- 证明 semantic fact identity 必须保持可追踪。
- 证明后续 pass 只能 lowering、attach metadata、generate witness 或 produce compare surface。
- 证明若需要改变 semantic truth，必须 fork new semantic world，而不是 mutate frozen world。

这些责任是 contract 级责任，不是 v0 字段承诺。

## 3. Receipt Authority

Freeze receipt 只能由 authorized verifier / freeze pass 产生。

v0 不定义该 pass 的实现、命令、runner、schema 或 storage model。但它定义 authority 边界：

| Actor | May consume receipt | May create receipt | May mutate receipt truth |
| --- | --- | --- | --- |
| `Builder` | yes | no | no |
| `Deriver` | yes | no | no |
| `Verifier / Freeze pass` | yes | yes | no after issue |
| `Lowering` | yes | no | no |
| `Witness` | yes | no | no |
| `Inspector` | yes | no | no |
| `Compare` | yes | no | no |
| `Projection / Coverage matrix` | yes | no | no |

Issued receipt 应被视为 immutable proof surface。若 receipt 本身需要变化，正确动作是产生新的 semantic world / new proof / new freeze / new receipt，而不是原地修改旧 receipt 的 truth。

## 4. Relationship to Freeze Boundary

`Compiler Pass Authority & Semantic Freeze Boundary v0` 定义 freeze 前后谁能改 world。

本文只定义未来 receipt 如何证明这条边界已经被跨越。

Freeze receipt 必须保持这些边界：

- freeze 前，builder / deriver / verifier 可以在 authority law 内构建、派生和证明 world facts。
- freeze 后，lowering 只能生成 target artifacts、metadata、symbolic maps 或 witness-ready surfaces。
- freeze 后，witness、inspector、compare、projection 只能消费已导出的 surfaces。
- freeze 后，任何 observed evidence 都不得直接修改 frozen world。
- freeze 后，任何 semantic truth change 都必须进入 new world branch。

## 5. Relationship to Lifecycle and Coverage

`Compiler World Lifecycle v0` 定义 `frozen` 是 world lifecycle state。

`Compiler Lifecycle Projection Coverage Matrix v0` 当前将 `frozen` 标记为：

```text
coverage strength: missing
projection kind: interpretive
sidecar eligibility: recommended
```

本文承接这个缺口，但不补实现。

换句话说：

- coverage matrix 暴露 `frozen` 没有直接 artifact marker。
- freeze receipt contract 定义未来 marker 应承担什么责任。
- 未来 sidecar 可以补齐 projection。
- 当前 v0 仍不新增 sidecar 文件或字段。

## 6. Consumer Rules

任何消费 freeze receipt 的对象都必须遵守只读规则。

### 6.1 Lowering consumer

Lowering 可以读取 freeze receipt 来确认 world 已经进入 target projection 阶段。

Lowering 不得因为目标平台、代码生成便利或 artifact shape 需要而改写 receipt 所证明的 semantic truth。

### 6.2 Witness consumer

Witness 可以引用 freeze receipt 来说明 artifact 或 observed behavior 对应的是哪个 frozen world。

Witness 不得：

- 重新生成 freeze receipt。
- 修改 receipt truth。
- 重新判断 lower-layer raw evidence 来绕过 receipt。
- 把 witness failure 自动修复成新的 frozen truth。

### 6.3 Inspector consumer

Inspector 可以展示 receipt、freeze state 与 frozen boundary。

Inspector 不得根据 UI 操作反向修改 receipt 或 semantic truth。

### 6.4 Compare consumer

Compare 可以比较 exported receipt-aware surfaces。

Compare 不得：

- 重跑 verifier / freeze pass。
- 读取 raw runtime evidence 来替代 receipt。
- 修复 drift 或 collapse。
- 把 compare result 写回 frozen world。

### 6.5 Projection and coverage consumer

Projection 与 coverage matrix 可以读取 receipt 来提升 `frozen` coverage strength。

它们不得让 coverage 状态成为新的 freeze truth。Coverage 只能审计有没有镜头，不能成为镜头背后的世界本体。

## 7. Future Sidecar Direction

未来可以考虑一个只读 sidecar：

```text
compiler_freeze_receipt.json
```

这个名字只是方向，不是 v0 接口。

若未来实现 sidecar，它必须保持：

- read-only proof surface
- no raw log parsing
- no lower brain rerun
- no verdict mutation
- no source fact creation
- no canonical identity definition by accident
- no observation import implementation by accident
- no storage model commitment by accident

本文不定义：

- JSON schema
- required fields
- hash algorithm
- node ref
- world id
- fork id
- serialization
- validator
- exporter
- smoke

## 8. Typical Violations

以下行为在 v0 中视为越权：

- witness exporter 根据 raw logs 重新签发 freeze receipt
- inspector 根据用户选择改写 receipt truth
- compare 发现 drift 后修改 frozen world 或 receipt
- lowering 为适配 target artifact 重命名 semantic identity
- projection coverage matrix 把 `frozen` 从 `missing` 改成 `strong`，但没有实际 receipt 或等价 proof surface
- runtime observation 直接更新 receipt，而不是形成 source fact proposal 并进入 new world branch

这些行为如果未来确实需要，应通过新的 authorized pass、new semantic world 与新的 receipt 明确建模。

## 9. 非目标

本 v0 不做：

- 不新增 JSON schema。
- 不新增 `compiler_freeze_receipt.json` 文件。
- 不新增 validator、exporter、smoke 或脚本。
- 不新增 C++ 类型或 IR。
- 不定义 canonical identity。
- 不定义 world id、fork id、hash、node ref、serialization 或 storage model。
- 不实现 observation import pass。
- 不定义 semantic debugging protocol。
- 不接入 LLVM/MLIR。
- 不改变现有 artifact report、bringup evidence、runtime ledger、witness bundle、world compare 的字段或判决模型。
