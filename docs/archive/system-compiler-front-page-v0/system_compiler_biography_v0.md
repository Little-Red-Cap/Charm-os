# System Compiler Biography v0

这份对象不替代：

- `runtime evidence bundle`
- `witness bundle`
- `world compare`

它要做的是把这三层已经成立的证据对象，再收成一张更适合交付和追问的“系统自证首页”。

## 一句话版本

- `runtime evidence bundle` 负责回答“这一轮 runtime 证据有没有跑齐”。
- `witness bundle` 负责回答“这个 canonical world 拿什么作证自己成立”。
- `world compare` 负责回答“这个世界相对基线是 standing、drifted 还是 collapsed”。
- `system compiler biography` 则负责把这些回答压成一份顶层传记式交付物。

如果说 `witness bundle` 更像整套证词，
`world compare` 更像反事实审判结论，
那么 `biography` 更像封面页和摘要页：

> 这个系统是谁，它为什么成立，它现在站不站得住，以及下一步该追问什么。

## 当前对象边界

当前 `system compiler biography` 对应：

- schema
  - `schemas/system_compiler.biography.v0.schema.json`
- export 脚本
  - `scripts/export_system_compiler_biography.py`
- validate 脚本
  - `scripts/validate_system_compiler_biography.py`
- gate 脚本
  - `scripts/check_system_compiler_biography_summary.ps1`
- wrapper 接入
  - `scripts/minimal_kernel_runtime_system_compiler_witness_bundle.ps1`

它当前输入：

- `minimal kernel runtime evidence bundle summary`
- `system compiler witness bundle summary`
- 可选的 `system compiler world compare summary`

它当前输出：

- `biography.summary.json`
- `biography.report.md`
- `biography.check.txt`

## 当前导出语义

当前 `biography` 会稳定收这些对象：

- `world_verdict`
- `world`
- `biography`
- `front_page`
- `delivery`
- `artifact_context`
- `runtime_evidence`
- `witness_bundle`
- `world_compare`
- `questions`
- `violations`

其中：

### `biography`

回答：

- 这个世界的身份是什么
- 它当前为何成立、改进、漂移或塌陷
- 它的最小证据路径是什么
- 下一步最值得追问什么

当前字段保持刻意克制：

- `identity`
- `thesis`
- `evidence_path`
- `next_questions`

### `delivery`

回答：

- 这份顶层自证对象自己的落点在哪里
- 哪个 `summary/report/check` 才是推荐优先阅读的首页

### `front_page`

回答：

- 这份 biography 自己的 machine-readable front page 路径是什么
- 如果 explain surface / report router 不想硬编码内部结构，它下一步应先跟到哪些 supporting surfaces

它不替代 `delivery`。

- `delivery` 更偏“这份对象导出到了哪里”
- `front_page` 更偏“工具应该先看谁，再顺着谁继续追问”

### `artifact_context`

回答：

- 这份 biography 引用了哪份 runtime summary
- 哪份 witness bundle 是当前候选世界
- 如果 compare 已接入，哪份 baseline / compare summary 参与了反事实判断

### `runtime_evidence / witness_bundle / world_compare`

这三块不是复制下层对象的完整结构，
而是保留最小但足够追问的投影：

- runtime 现在绿不绿
- witness 有没有 required 缺口
- compare verdict 是什么
- collapse surface 有没有被击中

## 推荐用法

如果已经走的是最小内核 runtime 的 system-compiler wrapper，
那么 `biography` 会由 wrapper 自动顺手导出：

```powershell
./scripts/minimal_kernel_runtime_system_compiler_witness_bundle.ps1 `
  -RuntimeEvidenceSummary out/minimal-kernel-runtime-system-compiler-witness/runtime_evidence/summary.json `
  -BaselineRuntimeEvidenceSummary out/minimal-kernel-runtime-system-compiler-witness/runtime_evidence/summary.json `
  -OutputRoot out/minimal-kernel-runtime-system-compiler-witness/self-compare `
  -Clean
```

此时默认会新增：

- `out/minimal-kernel-runtime-system-compiler-witness/self-compare/biography.summary.json`
- `out/minimal-kernel-runtime-system-compiler-witness/self-compare/biography.report.md`
- `out/minimal-kernel-runtime-system-compiler-witness/self-compare/biography.check.txt`

如果要把这份首页对象正式作为 CI gate，
可以直接检查它自己的 summary：

```powershell
./scripts/check_system_compiler_biography_summary.ps1 `
  -Summary out/minimal-kernel-runtime-system-compiler-witness/self-compare/biography.summary.json `
  -RequireWorldCompareResult ok `
  -RequireVerdict standing `
  -MaxRequiredMissing 0 `
  -MaxRegressions 0 `
  -MaxRequiredRegressions 0 `
  -RequireCollapseSurfaceUnchanged
```

如果当前还没接 baseline compare，
也可以只 gate “runtime + witness 已成立，但 verdict 仍未附着”：

```powershell
./scripts/check_system_compiler_biography_summary.ps1 `
  -Summary out/minimal-kernel-runtime-system-compiler-witness/biography.summary.json `
  -RequireVerdict not-attached `
  -MaxRequiredMissing 0
```

## 与现有对象的关系

### 1. 与 `runtime evidence bundle`

`biography` 不替代 runtime 证据包，
而是把它提升成“这个世界为何成立”的第一层存在论说明。

### 2. 与 `witness bundle`

`witness bundle` 负责收证词，
`biography` 负责用更接近交付和治理的语气概括证词。

### 3. 与 `world compare`

`world compare` 负责给出世界级 verdict，
`biography` 负责把 verdict、collapse surface、next questions 写成封面摘要。

## 当前非目标

当前这层仍然不处理：

- 自动根因定位到具体代码行
- 自动修复建议生成
- 多世界编年史拼接
- 全仓级 biography 总索引

v0 的目标更克制：

> 先让 Charm 能把“系统知道自己为何成立，也知道自己为何失败”导出成一个正式、可复验、可交付的首页对象。
