# Compiler Sidecar Landing Order v0

本文是 `Compiler Lifecycle Projection Coverage Matrix v0` 之后的 sidecar 落地顺序合同。

它回答的是：在 Charm compiler law 已经建立 constitution、authority/freeze、lifecycle、projection、coverage、lowering surface、freeze receipt 与 archive manifest 之后，哪些 sidecar 可以先落地，哪些 sidecar 必须继续推迟。

本文不新增 schema、summary 文件、validator、exporter、smoke、C++ 类型、World IR、canonical identity、observation import pass 或 LLVM/MLIR 接入。

## 1. 定位

`Compiler Sidecar Landing Order` 是 first sidecar landing 的顺序法。

它不是 sidecar schema，也不是实现计划。它只定义：

- 哪个 sidecar first。
- 哪些 sidecar later。
- 为什么不能先落更危险的 sidecar。
- first sidecar 必须遵守哪些只读边界。

## 2. Landing Principle

第一条原则：

> **先机器可读化现有 coverage truth，不补造尚未存在的 world truth。**

因此 first sidecar 应该只做：

```text
existing exported surfaces
  -> lifecycle projection status
  -> coverage summary
```

它不得：

- 解析 raw logs。
- 重跑 lower brain。
- 修改 verdict。
- 创建 source facts。
- 生成 new semantic truth。
- 定义 canonical identity。
- 实现 observation import。
- 伪造 freeze authority。

## 3. v0 Landing Order

v0 固定 sidecar 落地顺序：

| Order | Sidecar candidate | Landing decision | Reason |
| --- | --- | --- | --- |
| 1 | `compiler_lifecycle.summary.json` | first | 只读投影现有 coverage，能诚实表达 missing / interpretive / weak states |
| 2 | `compiler_freeze_receipt.json` | later | 需要真实 freeze authority，否则容易伪造 `frozen` |
| 3 | `compiler_lowering_surface_manifest.json` | later | 需要更稳定 artifact lineage，否则容易把 lowered residue 当 truth |
| 4 | `compiler_archive_manifest.json` | later | 会过早碰 storage、retention、artifact identity 与 archive truth |

这个顺序不是实现接口。它是风险排序。

## 4. First Sidecar: Lifecycle Summary

`compiler_lifecycle.summary.json` 是推荐 first sidecar。

它的职责是把 coverage matrix 机器可读化：

- `declared` 可以显示为 covered / mixed。
- `materialized` 可以显示为 covered / mixed。
- `proven` 可以显示为 covered / direct。
- `frozen` 必须仍可诚实显示为 missing / interpretive。
- `lowered` 必须仍可显示为 medium / interpretive。
- `witnessed` 可以显示为 covered / mixed。
- `observed` 可以显示为 covered / direct。
- `archived` 必须仍可显示为 weak-to-medium / interpretive。
- `compared` 可以显示为 covered / direct。

它不是 proof sidecar。它只是 coverage projection sidecar。

## 5. Deferred Sidecars

### 5.1 Freeze receipt

`compiler_freeze_receipt.json` 必须推迟。

原因：真实 freeze authority 尚未实现。如果先生成 receipt，很容易让 `frozen` 从法律 state 被误读为已有 direct artifact marker。

### 5.2 Lowering surface manifest

`compiler_lowering_surface_manifest.json` 必须推迟。

原因：artifact lineage、lowering marker、target artifact provenance 还需要更稳定的边界。过早实现会让 lowered surface 变成第二套 truth owner。

### 5.3 Archive manifest

`compiler_archive_manifest.json` 必须推迟。

原因：archive manifest 会牵涉 storage、retention、artifact identity、paired artifact collection 与 archive mutation policy。它不应在 canonical identity 之前被实现成强接口。

## 6. Relationship to Existing Contracts

本文不替代：

- `Compiler World Lifecycle Projection v0`
- `Compiler Lifecycle Projection Coverage Matrix v0`
- `Compiler Lowering Surface Contract v0`
- `Compiler Freeze Receipt Contract v0`
- `Compiler Archive Manifest Contract v0`

它只定义 first sidecar 的落地顺序。

`compiler_lifecycle.summary.json` 未来必须遵守 lifecycle projection 与 coverage matrix 的只读边界。它不能因为自己是第一个 sidecar，就获得 world ownership。

## 7. 非目标

本 v0 不做：

- 不新增 JSON schema。
- 不新增 `compiler_lifecycle.summary.json` 文件。
- 不新增 validator、exporter、smoke 或脚本。
- 不新增 C++ 类型或 IR。
- 不定义 canonical identity。
- 不实现 observation import pass。
- 不定义 semantic debugging protocol。
- 不接入 LLVM/MLIR。
- 不改变现有 artifact report、bringup evidence、runtime ledger、witness bundle、world compare 的字段或判决模型。
