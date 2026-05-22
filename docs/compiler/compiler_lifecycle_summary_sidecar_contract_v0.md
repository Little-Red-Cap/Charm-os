# Compiler Lifecycle Summary Sidecar Contract v0

本文是 `Compiler Sidecar Landing Order v0` 的下游 contract。

它定义 `compiler_lifecycle.summary.json` 应承担的职责边界：只读投影现有 artifact/report/witness/ledger/compare surfaces，把九个 lifecycle states 的 projection status 与 coverage status 机器可读化。

当前最小实现入口为 `scripts/export_compiler_lifecycle_summary.py`，只读 gate 为 `scripts/check_compiler_lifecycle_summary.ps1`，只读 report consumer 为 `scripts/report_compiler_lifecycle_summary.ps1`，按需 sidecar wrapper 为 `scripts/compiler_lifecycle_summary_sidecar.ps1`，定向 smoke 为 `scripts/compiler_lifecycle_summary_sidecar_smoke.ps1`。这个 gate 只检查 sidecar 自身形状与 v0 honesty constraints，不是 JSON schema validator；report consumer 只渲染 summary，不创建新 truth；wrapper 只串接 export/gate/report，不接入大 CI。本文仍不定义 JSON schema、validator、C++ 类型、World IR、canonical identity、observation import pass 或 LLVM/MLIR 接入。

## 1. 定位

`Compiler Lifecycle Summary Sidecar` 是 lifecycle coverage 的只读机器可读投影。

它回答的是：

> **现有导出 surfaces 当前如何承接 lifecycle states。**

它不是：

- 新的 truth owner。
- 新的 world schema。
- 新的 freeze receipt。
- 新的 lowering manifest。
- 新的 archive manifest。
- 新的 compare verdict。
- 新的 witness verdict。
- 新的 lower brain。

它只能消费已经导出的合法 surfaces。它不能替代 artifact report、runtime ledger、witness bundle、world compare、coverage matrix 或任何未来 receipt/manifest 的原有职责。

## 2. Sidecar Responsibility

`compiler_lifecycle.summary.json` 至少应承担这些责任：

- 列出九个 lifecycle states 的 projection status。
- 说明每个 state 的 coverage strength。
- 说明每个 state 的 projection kind。
- 说明每个 state 是否有 sidecar gap。
- 指向已消费的 exported surfaces。
- 保持 `frozen` 可被诚实标记为 `missing`。
- 保持 `lowered` 可被诚实标记为 `medium / interpretive`。
- 保持 `archived` 可被诚实标记为 `weak-to-medium / interpretive`。
- 明确自己不创建 source facts、proof facts、freeze facts、archive facts 或 compare facts。

这些责任是 contract 级责任，不是 v0 字段承诺。

## 3. Source Boundaries

Lifecycle summary sidecar 只允许消费 exported surfaces。

v0 允许的来源类型是：

| Source surface | May consume | Must not |
| --- | --- | --- |
| `artifact_report` | lifecycle declared/materialized/lowered projection | treat report as lower brain |
| `runtime_ledger.json` | observed projection | replace session verdict |
| `kernel_runtime_session.summary.json` | proven/witnessed projection | rerun runtime evidence |
| `witness_bundle` | witnessed/archived projection | create witness truth |
| `world_compare` | compared projection | create compare truth |
| `coverage matrix` | coverage vocabulary and expected gaps | become source of world truth |

It must not consume raw host logs, raw QEMU logs, raw serial logs, source code, build trees, or ad hoc filesystem scans to infer hidden truth.

## 4. Required Honesty

The first lifecycle summary sidecar must preserve missing and interpretive states honestly.

In particular:

- `frozen` must not become covered merely because lifecycle summary exists.
- `lowered` must not become direct merely because artifact report exists.
- `archived` must not become strong merely because witness bundle exists.
- `observed` must not become semantic mutation.
- `compared` must not create new truth.

The sidecar can summarize coverage. It cannot improve coverage by declaration.

## 5. Relationship to Future Sidecars

Lifecycle summary sidecar can point at future sidecars when they exist:

- `compiler_freeze_receipt.json`
- `compiler_lowering_surface_manifest.json`
- `compiler_archive_manifest.json`

But it must not emulate them.

If those sidecars are absent, lifecycle summary should represent their corresponding states as absent, missing, interpretive, weak, or optional according to the coverage matrix and available surfaces.

## 6. Consumer Rules

Consumers may use lifecycle summary to quickly understand lifecycle projection status.

Consumers must not:

- treat lifecycle summary as canonical world truth
- treat lifecycle summary as proof of freeze
- use lifecycle summary to repair witness gaps
- use lifecycle summary to repair compare drift
- use lifecycle summary to create source facts
- use lifecycle summary to override artifact report, runtime ledger, witness bundle, world compare, freeze receipt or archive manifest

## 7. Current Implementation Direction

当前最小实现会生成：

```text
compiler_lifecycle.summary.json
```

当前 exporter 保持最小只读：

```text
exported surfaces
  -> lifecycle projection summary
```

当前 smoke 应断言：

- all nine lifecycle states are present in the summary
- `frozen` remains missing when no freeze receipt exists
- `lowered` remains interpretive when only artifact report / metadata surfaces exist
- `archived` remains weak-to-medium when no archive manifest exists
- no raw logs are parsed
- existing verdicts are not modified

当前 gate 应保持更窄：

- 检查 summary schema/kind/result/state count。
- 检查九个 lifecycle states 与每个 state 的最小字段存在。
- 检查 `frozen` 仍为 `missing / interpretive / recommended`。
- 检查 present 的 `lowered` / `archived` 不被升级成 direct truth。
- 不读取 raw logs，不重跑 exporter，不替代 schema validator，不修改任何 source surface 或 verdict。

当前 report consumer 应保持只读：

- 只读取 `compiler_lifecycle.summary.json`。
- 只输出 lifecycle states、honesty markers、source surfaces 与 summary violations 的人类可读报告。
- 不执行 gate，不解析 raw logs，不重跑 exporter，不修复 missing states，不创建 source facts。

当前 sidecar wrapper 应保持按需：

- 只把 exporter、gate 与 report consumer 串成一个人工或局部 CI 可调用入口。
- 只接受显式传入的 exported surfaces，不扫描 artifact root 来猜测 hidden truth。
- 不接入默认 runtime evidence bundle、大 CI 或 compare 判决模型。

当前 runtime evidence bundle hook 应保持可选：

- 只有显式传入 `-ExportCompilerLifecycleSummary` 时才调用 sidecar wrapper。
- 只转交 bundle 已导出的 session summary、runtime ledger 与可用 witness bundle summary。
- 不猜测 artifact report 或 world compare，不读取 raw logs，不改变 runtime evidence / witness / compare verdict。

该实现不新增 schema，不接入大 CI，也不替代 artifact report、runtime ledger、witness bundle 或 world compare。

## 8. 非目标

本 v0 不做：

- 不新增 JSON schema。
- 不新增 validator。
- 不新增 C++ 类型或 IR。
- 不定义 canonical identity。
- 不实现 observation import pass。
- 不定义 semantic debugging protocol。
- 不实现 freeze receipt、lowering surface manifest 或 archive manifest。
- 不接入 LLVM/MLIR。
- 不改变现有 artifact report、bringup evidence、runtime ledger、witness bundle、world compare 的字段或判决模型。
