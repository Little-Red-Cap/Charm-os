# World Compare v0

这份文档回答的不是：

- 某个单点 witness 有没有跑通
- 某个 report 有没有生成

它回答的是：

> 当一个 canonical world 已经能导出 witness bundle 之后，
> 我们怎样正式回答“这个世界和昨天相比哪里漂了、哪里塌了、最小塌陷面落在哪”。

## 一句话版本

- `canonical world` 负责声明“这个世界想证明什么”。
- `witness bundle` 负责声明“这次交付拿什么作证”。
- `world compare` 则负责回答“这个世界相对基线还站不站得住”。

如果说 `witness bundle` 更像整套证词，
那么 `world compare` 更像反事实审判后的结论页。

## 为什么值得单独收这一层

只有：

- baseline / candidate 两份 witness bundle
- 零散 report
- 人脑里的对照记忆

还不够。

因为这仍然会留下三个问题：

- 世界是否只是“有变化”，还是已经“塌了”
- 第一条先坏掉的 witness 到底是哪条
- 这次漂移打到的是 `bundle / upper_half / ingress / system_compiler` 哪一层

`world compare` 的目标，
就是把这些问题收成一个可交付、可验证、可继续追问的对象。

## 当前对象边界

当前 `world compare` 对应：

- schema：
  - `schemas/system_compiler.world_compare.v0.schema.json`
- sample：
  - `schemas/examples/system_compiler.world_compare.v0.sample.json`
- compare 脚本：
  - `scripts/compare_system_compiler_world.py`
- validate 脚本：
  - `scripts/validate_system_compiler_world_compare.py`

它当前输入非常克制：

- `baseline witness bundle`
- `candidate witness bundle`

也就是说，v0 先不直接接：

- 原始 smoke log
- 原始 runtime summary
- 原始 artifact root

而是坚持站在 witness bundle 之上做世界级 compare。

## 当前输出语义

当前导出的 `world compare` 会稳定收这些对象：

- `world`
- `artifact_context`
- `bundle_status`
- `world_changes`
- `contract_drift`
- `witness_summary`
- `witness_changes`
- `collapse_surface`
- `questions`

其中：

### `world_verdict`

当前 verdict 只有四种：

- `standing`
- `improved`
- `drifted`
- `collapsed`

它回答的不是“脚本是否执行成功”，
而是“这个世界相对基线还站不站得住”。

### `world_changes`

回答：

- world summary 是否变了
- subject 是否漂了
- compare questions / contract refs / witness plan ids 是否变了

也就是说，它先看“宪法文本有没有改”。

### `contract_drift`

回答：

- 现有 contract refs 的 present / missing 状态是否变化

也就是说，它不是只看 world 声称依赖哪些法律，
而是继续看这些法律锚点在 candidate 里是否真的还在。

### `witness_changes`

回答：

- 哪些 witness 变了
- 变动是 `added / removed / changed`
- 影响是 `neutral / improvement / regression`
- 状态是 `ok / missing / fail / absent` 怎样迁移

这层开始正式把“哪条 witness 先坏”收成结构化对象。

### `collapse_surface`

这是 v0 最重要的部分。

它当前直接回答：

- 哪些 witness regressed
- 哪些是 required regressions
- candidate 里哪些 witness 已经 `fail / missing`
- 新增了哪些 missing contract refs
- 受影响层和受影响 focus 是什么

也就是说，`collapse_surface` 当前先不做完整自动根因分析，
但已经能把“最小塌陷面”压到一个相对可行动的层级。

## 当前推荐工作流

1. 先导出 baseline witness bundle。
2. 再导出 candidate witness bundle。
3. 用 `compare_system_compiler_world.py` 生成 world compare。
4. 再用 `validate_system_compiler_world_compare.py` 校验引用完整性。

示例：

```powershell
python ./scripts/compare_system_compiler_world.py `
  --baseline schemas/examples/system_compiler.witness_bundle.v0.sample.json `
  --candidate schemas/examples/system_compiler.witness_bundle.v0.candidate_drift.sample.json `
  --output-root out/system-compiler-world-compare

python ./scripts/validate_system_compiler_world_compare.py `
  --bundle-root out/system-compiler-world-compare
```

如果你已经在走最小内核 runtime 的 system-compiler witness 总入口，
也可以直接让 wrapper 在导出当前 witness 后顺手产出 compare：

```powershell
./scripts/minimal_kernel_runtime_system_compiler_witness_bundle.ps1 `
  -RuntimeEvidenceSummary out/minimal-kernel-runtime-system-compiler-witness/runtime_evidence/summary.json `
  -BaselineWitnessSummary out/baseline-witness/summary.json `
  -OutputRoot out/minimal-kernel-runtime-system-compiler-witness
```

如果要在真实 runtime evidence 上做 self-compare 烟测，也可以把同一份
runtime summary 同时作为 current / baseline 输入：

```powershell
./scripts/minimal_kernel_runtime_system_compiler_witness_bundle.ps1 `
  -RuntimeEvidenceSummary out/minimal-kernel-runtime-system-compiler-witness/runtime_evidence/summary.json `
  -BaselineRuntimeEvidenceSummary out/minimal-kernel-runtime-system-compiler-witness/runtime_evidence/summary.json `
  -OutputRoot out/minimal-kernel-runtime-system-compiler-witness/self-compare `
  -Clean
```

此时预期 `world_compare/summary.json` 中的 `world_verdict` 为 `standing`。

当 wrapper 走了 compare 路径后，根输出目录下的 `report.md` 与 `check.txt`
也会同步带上 world-compare verdict、collapse surface 与 next questions，
不需要再手工进 `world_compare/` 子目录才能看见世界级结论。

此时 compare 产物默认落在：

- `out/minimal-kernel-runtime-system-compiler-witness/world_compare/summary.json`
- `out/minimal-kernel-runtime-system-compiler-witness/world_compare/report.md`
- `out/minimal-kernel-runtime-system-compiler-witness/world_compare/check.txt`

## 与现有对象的关系

### 1. 与 `canonical world`

`canonical world` 负责声明：

- 这个世界是什么
- 它想证明什么

`world compare` 不重写这个定义，
而是回答：

- 这个定义相对基线是否还站得住

### 2. 与 `witness bundle`

`witness bundle` 负责收证词。

`world compare` 负责比较证词，
并把比较结果提升成一个世界级 verdict。

### 3. 与 `artifact report compare`

`artifact report compare` 更偏 case 级或 stage 级差异。

`world compare` 不替代它，
而是站在更高一层回答：

- 哪个 witness 面先碎了
- 哪个 world 因此开始 drift / collapse

## 当前非目标

当前这层仍然不处理：

- 自动根因定位到具体代码行
- 自动修复建议生成
- 全仓 world compare 调度器
- 交互式尸检浏览器

v0 更克制的目标只有一个：

> 先让 Charm 能正式把“这个世界为什么还成立，或者为什么已经塌了”导出成对象。
