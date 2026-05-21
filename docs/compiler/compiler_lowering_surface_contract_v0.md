# Compiler Lowering Surface Contract v0

本文是 `Compiler World Lifecycle v0`、`Compiler Lifecycle Projection Coverage Matrix v0` 与 `Compiler Pass Authority & Semantic Freeze Boundary v0` 的下游 contract。

它定义 lowering surfaces 应承担的法律责任：说明 frozen semantic world 如何被投影成 target artifacts、docs、metadata、inspector surfaces、witness-ready surfaces 或 compare-ready summaries，同时防止这些 lowered surfaces 被误读为完整 world truth。

本文不实现 lowering manifest，不定义 schema、字段、hash、world id、artifact id、canonical identity、storage model、validator、exporter、smoke、C++ 类型、IR 或 LLVM/MLIR 接入。

## 1. 定位

`Compiler Lowering Surface` 是 frozen world 的 lossy projection surface。

它回答的是：

> **某个 target artifact 或 explanation surface 如何作为 lowered residue 被阅读。**

Lowering surface 不是：

- 新的 truth owner。
- 新的 world schema。
- 新的 codegen pipeline。
- 新的 canonical identity 机制。
- 新的 proof engine。
- 新的 compare verdict。
- 新的 witness verdict。
- 新的 lower brain。

它只能承载 frozen world 的某个投影。它不能替代 constitution、authority law、freeze receipt、witness bundle、artifact report 或 compare 的原有判决模型。

## 2. Surface Responsibility

任何声称承接 `lowered` state 的 surface 至少应承担这些法律责任：

- 说明自己来自 frozen semantic world 或 future freeze-equivalent proof surface。
- 承认自己是 lossy projection，不是完整 world truth。
- 保留足够 provenance，让 witness、inspector、compare 或 archive 能回指 world facts。
- 不得改变 topology identity。
- 不得改变 resource ownership。
- 不得让旧 legality proof 套用到被改写后的 semantic truth。
- 不得为了 target artifact 便利而重命名 semantic identity。
- 不得把 omitted compile-time richness 伪装成不存在。
- 不得成为第二套 canonical truth source。

这些责任是 contract 级责任，不是 v0 字段承诺。

## 3. v0 Lowering Surface Classes

v0 承认这些常见 lowering surfaces：

| Surface class | Lowering meaning | Must not |
| --- | --- | --- |
| `firmware / target artifact` | frozen world 的物理执行残留 | claim binary-only truth |
| `docs` | 面向人类的 world explanation projection | become hand-maintained parallel truth |
| `metadata` | 面向工具的 lowered context | redefine semantic identity |
| `inspector surface` | 面向交互阅读的 observation/explain surface | mutate world truth through UI |
| `witness-ready surface` | 可被 witness 回指的 artifact surface | replace witness verdict |
| `capability matrix` | capabilities 的可读 projection | become resource legality owner |
| `compare-ready summary` | 可供 compare 消费的 exported surface | rerun builder / verifier |
| `artifact_report` | system compiler 结果物的只读解释面 | replace raw artifact lineage or lower brain |

这个表不是 schema。它只是 v0 的 surface reading law。

## 4. Lossy Lowering Law

Lowering must be lossy by declaration.

compile-time world 必然比任一 lowered surface 更 rich。任何 target artifact、docs、metadata、inspector surface、witness-ready surface、capability matrix 或 compare-ready summary 都只携带 world truth 的某个投影。

因此：

- firmware artifact 不等于完整 world truth。
- docs artifact 不等于完整 world truth。
- metadata artifact 不等于完整 world truth。
- inspector surface 不等于完整 world truth。
- witness-ready surface 不等于 witness verdict。
- compare-ready summary 不等于 compare verdict。
- artifact report 不等于 lower brain。

Lowering 的职责不是把所有 world truth 塞进一个 artifact，而是诚实说明：这个 surface 保留了哪些语义、丢失了哪些语义、还能如何回指 world facts。

## 5. Authority Boundary

Lowering pass 可以生成 target artifacts、lowered metadata、symbolic maps、docs、inspector surfaces 或 compare-ready summaries。

Lowering pass 不得：

- 改写 frozen topology identity。
- 改写 frozen resource ownership。
- 改写 frozen legality identity。
- 改写 semantic fact identity。
- 创建 observed facts。
- 创建 source facts。
- 修改 witness verdict。
- 修改 compare verdict。
- 通过 target-specific convenience 暗改 semantic truth。

Witness、inspector、compare、projection、coverage matrix 与 archive manifest 可以消费 lowered surfaces。它们不得通过 lowered surface 反向修改 semantic truth。

## 6. Relationship to Lifecycle and Coverage

`Compiler World Lifecycle v0` 定义 `lowered` 是 world lifecycle state。

`Compiler Lifecycle Projection Coverage Matrix v0` 当前将 `lowered` 标记为：

```text
coverage strength: medium
projection kind: interpretive
sidecar eligibility: optional
```

本文承接这个中等覆盖，但不补实现。

换句话说：

- coverage matrix 说明 `lowered` 已有 `artifact_report`、witness / metadata surfaces 承接。
- lowering surface contract 定义这些 surfaces 应遵守什么法律责任。
- 当前 coverage 仍然是 interpretive，不因为本文存在而自动变成 direct。
- 未来 sidecar 可以提升 lowering projection 的审计清晰度。

## 7. Consumer Rules

任何消费 lowering surface 的对象都必须遵守只读或 authority-boundary 规则。

### 7.1 Witness consumer

Witness 可以读取 lowered surface 来回指 world facts。

Witness 不得：

- 把 lowered surface 当成完整 world truth。
- 重新运行 lower brain。
- 修改 lowered surface 所来自的 semantic truth。
- 用 artifact convenience 覆盖 witness verdict。

### 7.2 Inspector consumer

Inspector 可以展示 lowered surface、provenance 与可追问入口。

Inspector 不得根据 UI 操作反向修改 lowered surface、semantic truth、witness truth 或 compare result。

### 7.3 Compare consumer

Compare 可以比较 exported lowered surfaces。

Compare 不得：

- 重跑 builder / deriver / verifier。
- 重跑 lowering。
- 读取 raw logs 来替代 exported surface。
- 创建新的 semantic truth。
- 修复 drift 或 collapse。

### 7.4 Projection and coverage consumer

Projection 与 coverage matrix 可以读取 lowering surfaces 来说明 `lowered` 的覆盖强弱。

它们不得让 coverage 状态成为新的 lowering truth。Coverage 只能审计 lowered surfaces 是否可见，不能成为 lowered surface 背后的 world 本体。

## 8. Future Sidecar Direction

未来可以考虑一个只读 sidecar：

```text
compiler_lowering_surface_manifest.json
```

这个名字只是方向，不是 v0 接口。

若未来实现 sidecar，它必须保持：

- read-only lowering projection surface
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
- world id
- artifact id
- storage model
- serialization
- validator
- exporter
- smoke

## 9. Typical Violations

以下行为在 v0 中视为越权：

- lowering 为了 target artifact 便利改写 resource ownership
- docs 手写出一套与 semantic world 不一致的 parallel truth
- inspector surface 根据用户选择反向修改 source facts
- witness exporter 把 lowered surface 当作完整 proof 并覆盖 witness verdict
- compare 读取 raw runtime evidence 来绕过 exported lowered surface
- artifact report 伪装成 lower brain 或 canonical world
- projection coverage matrix 把 `lowered` 从 `medium` 改成 `strong`，但没有实际 direct lowering marker 或等价 proof surface

这些行为如果未来确实需要，应通过新的 authorized pass、new semantic world 或新的 lowering sidecar 明确建模。

## 10. 非目标

本 v0 不做：

- 不新增 JSON schema。
- 不新增 `compiler_lowering_surface_manifest.json` 文件。
- 不新增 validator、exporter、smoke 或脚本。
- 不新增 C++ 类型或 IR。
- 不定义 canonical identity。
- 不定义 world id、artifact id、hash、node ref、serialization 或 storage model。
- 不实现 observation import pass。
- 不定义 semantic debugging protocol。
- 不接入 LLVM/MLIR。
- 不改变现有 artifact report、bringup evidence、runtime ledger、witness bundle、world compare 的字段或判决模型。
