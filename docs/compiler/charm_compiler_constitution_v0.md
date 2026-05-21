# Charm Compiler Constitution v0

本文是 Charm compile-time world 的编译器宪法。

它不是新的 DSL，不是 `World IR` schema，也不是 LLVM/MLIR 接入计划。它用于定义 Charm 作为 system compiler 时必须遵守的世界法理：世界如何存在、事实如何进入世界、哪些 pass 有权修改世界、何时冻结语义、lowering 如何保留语义身份，以及 witness 如何回指世界真相。

Pass authority、identity preservation、semantic freeze boundary、world fork 与 lossy lowering 的细化规则见：[`compiler_pass_authority_and_freeze_boundary_v0.md`](compiler_pass_authority_and_freeze_boundary_v0.md)。
World lifecycle、observation import boundary 与 archived/compared 状态语言见：[`compiler_world_lifecycle_v0.md`](compiler_world_lifecycle_v0.md)。

## 1. 定位

Charm 的长期主语不是 runtime framework，而是 compile-time world builder。

可以把当前方向压成一句话：

```text
开发期/编译期：世界是活的。
固件期/运行期：世界是被裁剪、证明、lowering 后残留下来的物理执行层。
```

因此，firmware 不是唯一最终产物。Charm 的完整系统事实至少应由这些观察面共同描述：

```text
firmware
docs
inspector metadata
witness bundle
capability matrix
compare surface
debug/provenance metadata
```

这些观察面不应成为六套平行 truth source。它们必须来自同一个 semantic world 的不同 lowering 或 observation surface。

## 2. World

`World` 是 Charm 的 compile-time semantic universe。

它由 source facts 构建，经 lawful passes 派生、验证、冻结、lowering，并最终形成 target artifacts 与 witnesses。

v0 中的 `World` 是法律主语，不是数据结构名。本文不定义 `WorldIR`、`TopologyIR`、`Node`、C++ class 或 schema shape。

一个安全的理解是：

```text
source facts
  -> semantic world
  -> lawful passes
  -> semantic freeze point
  -> lowering / witness / observation surfaces
  -> firmware + docs + evidence + inspector + compare
```

## 3. Fact

`Fact` 是 world 中可被引用、派生、验证、lowering 或观察的语义事实。

v0 先承认五类 fact：

| Fact | 含义 | 当前胚胎 |
| --- | --- | --- |
| `Source Fact` | 输入侧直接声明或承认的事实 | `SystemSpec`、`Profile`、`BoardPackage`、`BoardCaps`、declared board/resource facts |
| `Derived Fact` | 由 source facts 和规则派生出的事实 | binding result、capability graph、resource route、topology edge |
| `Proven Fact` | 已经通过 legality / verifier / evidence rule 证明的事实 | resource legality、contract-required facts satisfied、session standing |
| `Materialized Fact` | 已被规范化为可执行、可观察或可导出的系统结果 | materialized graph、bringup order、artifact report projection |
| `Observed Fact` | 从运行、smoke、witness 或 inspect surface 观察回来的事实 | runtime evidence、QEMU lower-half witness、bringup observed state |

这些分类不是新 schema，也不是完整 failure taxonomy。它们先定义事实在 world 中的身份来源，避免把所有内容都压成“日志”“配置”或“模板展开结果”。

## 4. Pass Authority Model

不是所有 pass 都有权修改 world。

v0 先定义五类 pass authority：

| Pass | 权限 | 禁止事项 |
| --- | --- | --- |
| `Builder` | 可从 source facts 建立 world 初始事实 | 不得伪造 observed facts |
| `Deriver` | 可从既有 facts 派生新 facts | 不得绕过 source provenance |
| `Verifier` | 只读 world，并产出 proven facts 或 violations | 不得为了证明通过而修改 semantic truth |
| `Lowering` | 可把 frozen world 投影为 firmware/docs/metadata 等 target artifacts | 不得改变 frozen semantic identity |
| `Witness` | 只读 world 和 artifacts，并产出可回指的 witness | 不得修改 world truth 或重新裁决下层语义 |

`evidence`、`inspector`、`report`、`compare consumer` 这类 pass 默认属于 read-only observation/witness 平面。它们可以解释、投影、引用或比较已导出的 facts，但不能成为新的下层 brain。

## 5. Semantic Freeze Point

`Semantic Freeze Point` 是 world 从“可构建/可派生”进入“可 lowering/可见证”的边界。

freeze 后必须成立：

- topology identity must remain stable
- resource ownership must not change
- legality proofs must remain valid
- semantic fact identity must remain traceable
- later passes may lower representations
- later passes may attach witness/debug/inspector metadata
- later passes must not mutate semantic truth

v0 不定义 freeze 的具体算法、数据结构或触发命令。它只先冻结原则：一旦 world 进入 witness、inspector、compare 或 firmware lowering 阶段，后续 pass 不得再悄悄改变 topology、ownership 或 legality 语义。

## 6. Lowering

`Lowering` 是 semantic-preserving projection。

它不是普通模板替换，也不等同于“代码生成脚本”。lowering 的职责是把 frozen semantic world 投影到目标产物，同时保留可追踪的 semantic identity。

典型 lowering surfaces 包括：

- firmware layout / init sequence
- register init / clock tree / IRQ route
- docs / capability matrix
- inspector metadata
- artifact report
- witness bundle
- compare surface

同一个 fact 可以被 lowering 到多个 surface。关键约束是：这些 surface 必须能回指同一个 semantic fact，而不是各自维护一套互相漂移的真相。

## 7. Witness

`Witness` 是 first-class artifact。

它不是日志副产品，也不是“跑完之后顺手留下的 report”。在 Charm compiler constitution 中，witness 的职责是把 lowered result 或 observed behavior 回指到 semantic world facts。

v0 中 witness 必须遵守：

- witness may reference source, derived, proven, materialized, and observed facts
- witness may summarize standing / missing / collapsed style outcomes when existing contracts already define them
- witness must not mutate world truth
- witness must not re-run lower-layer judgment logic unless it is explicitly authorized as a verifier
- witness must preserve provenance from artifact back to semantic fact where available

因此，未来 `firmware.bin` 不应单独代表完整系统事实。更完整的交付形态应接近：

```text
firmware.bin
firmware.world
firmware.evidence
firmware.topology
firmware.capability
firmware.symbolic-map
```

这些名字不是 v0 接口承诺，只表达 witness-first artifact 的方向。

## 8. Single Semantic World

Charm 的 compile-time world 必须避免 truth fragmentation。

传统 embedded 工程常见漂移是：

| Surface | Truth source |
| --- | --- |
| firmware | C/C++ |
| docs | Markdown |
| inspector | 手写 UI |
| resource table | Excel/YAML |
| topology | wiki |
| legality | 人脑 |

Charm compiler constitution 的要求是：

```text
single semantic world
  -> firmware lowering
  -> docs lowering
  -> inspector lowering
  -> witness lowering
  -> capability matrix lowering
  -> compare/diagnostic observation
```

如果某个 surface 不能回指 semantic world，它只能被视为辅助材料，不能被视为 canonical truth。

## 9. 与现有 System Compiler 语义的关系

本文不替代现有 system compiler roadmap、vocabulary、artifact report、resource contract 或 bringup evidence pipeline。

它们在 constitution 下的关系是：

- `system_compiler_roadmap.md`：说明 Charm 为什么要成为 system compiler。
- `system_compiler_vocabulary_v0.md`：收敛当前允许使用的 system compiler 词汇。
- `artifact_report_v0.md`：当前 system compiler 结果物的只读解释面。
- `resource_contract_v0.md`：resource legality 的当前契约面。
- `bringup_evidence_pipeline_v0.md`：`declared / materialized / published / observed / failed / blocked` 的当前 evidence state language。

其中 `declared / materialized / published / observed` 已经可以视为 World State Transition Language 的现有胚胎，但本刀不改变它们在 bringup evidence pipeline v0 中的原有语义。

## 10. 非目标

本 constitution v0 不做：

- 不新增 `World IR` schema/sample。
- 不新增 `WorldIR.hpp`、`TopologyIR.hpp`、`Node` 或任何 C++ IR 类型。
- 不新增 pass runner、Clang plugin、LLVM pass 或 MLIR dialect。
- 不新增 schema、validator、smoke 或 codegen。
- 不把 C++ template、YAML、JSON 或 generator 提升为唯一 truth source。
- 不改变现有 witness bundle、artifact report、world compare、bringup evidence 的字段或判决模型。
- 不冻结完整 failure taxonomy、pass pipeline implementation 或 semantic freeze algorithm。

后续如果要推进实现，应另开 `Charm World Model / World IR Schema v0` 或 `Compiler Pass Pipeline v0`，并以本 constitution 的 law 为上位边界。
