# Compiler World IR / Pipeline Contract v0

本文是 `Charm Compiler Constitution v0`、`Compiler Pass Authority & Semantic Freeze Boundary v0` 与 `Compiler World Lifecycle v0` 之间的桥接合同。

它不定义 `WorldIR.hpp`、`TopologyIR.hpp`、C++ IR 类型、LLVM/MLIR dialect 或 codegen pipeline。它只定义：**第一条可执行的窄管道应该如何把 semantic world 送到 lowering/witness surfaces，同时不把 truth 让位给实现细节**。

## 1. 定位

前面几份文档已经把法律层立住了：

- `constitution` 定义 world、fact、witness、lowering 的世界法理。
- `authority / freeze` 定义谁能改世界、何时必须停止改世界。
- `lifecycle` 定义 declared / materialized / proven / frozen / lowered / witnessed / observed / archived / compared 的状态语言。

本合同回答的是更窄的一句：

> **这些法律如何收束成第一条可执行管道，并为未来 LLVM landing 保留一个清晰、可替换的 lowering seam。**

## 2. Pipeline Shape

v0 先承认一条窄管道的最小形状：

```text
ingress
  -> extract
  -> normalize
  -> freeze
  -> lower
  -> witness
  -> observe / compare
```

这里的名字是语义阶段名，不是 C++ 类名，也不是 schema name。

### `ingress`

- 读取声明侧输入、board/profile/resource 事实、显式路径与已知 evidence 入口。
- 只负责把“世界要从哪里进来”说清楚。
- 不得偷偷引入第二套 truth source。

### `extract`

- 从 ingress facts 中抽出可派生的 semantic candidates。
- 可以做结构化归一，但不得改写 provenance。
- 不得把 derived facts 冒充成已证明事实。

当 extraction 使用 C++26 static reflection 时，`<meta>` 只能出现在 hosted extraction surface 中；firmware 侧只能消费 generated freestanding residue。该边界见：[`compiler_hosted_reflection_extraction_surface_v0.md`](compiler_hosted_reflection_extraction_surface_v0.md)。

### `normalize`

- 把同一世界中的事实整理成可比较、可 lower 的稳定形态。
- 允许 canonical-looking 变换，但不得改变 semantic identity。
- 这一层仍然不是 freeze 之后的 target artifact。

### `freeze`

- 把当前 semantic world 送入 freeze boundary。
- 一旦过线，后续只能 lower / witness / observe / compare。
- 不得在此后悄悄修改 topology、ownership、legality 或事实身份。

### `lower`

- 把 frozen world 投影到 target artifacts、metadata、debug maps、witness seeds 等 surface。
- 这里是未来 LLVM 最可能接入的位置，但 LLVM 只能是实现选择，不是法律主语。
- lowering 必须保留可回指的 provenance。

### `witness`

- 只读消费 frozen world 与 lowered surfaces。
- 生成可回指的 witness / evidence / summary surface。
- 不得改写 semantic truth，也不得回头重跑 lower-layer brain。

### `observe / compare`

- 只消费已导出的 surfaces。
- 只比较、不重建 truth。
- 不得把 compare 变成新的 semantic extraction engine。

## 3. First Landing Slice

v0 只要求这条管道足够小，能承载一个可审计切片：

- source facts 明确
- derived facts 可解释
- freeze boundary 明确
- lowering surface 可见
- witness 可回指

第一块可落地切片的更细边界见：[`compiler_world_ir_first_landing_slice_v0.md`](compiler_world_ir_first_landing_slice_v0.md)。

## 4. Landing Rule

如果未来要把 LLVM 真的接进来，它只能落在 `lower` 这一段，并满足：

- 输入必须是 frozen semantic world，不是未冻结的草稿世界
- 输出必须保留 provenance 与 witness 可回指性
- 不能借 LLVM 重新定义 semantic truth
- 不能让 backend 变成 world owner

也就是说，LLVM 是 `lower` 的一个候选实现，不是本合同的中心。

## 5. Non-goals

本合同 v0 不做：

- 不定义 `World IR` 数据结构。
- 不定义 `WorldIR.hpp`、`TopologyIR.hpp`、`Node` 或任何 C++ IR 类型。
- 不定义 LLVM/MLIR dialect、pass runner 或 codegen pipeline。
- 不定义 canonical identity、fork id、hash、storage model 或 import pass。
- 不改变现有 witness bundle、artifact report、world compare、bringup evidence 的字段或判决模型。

## 6. 与上位文档的关系

- Constitution 负责立法。
- Authority / freeze 负责权限与边界。
- Lifecycle 负责状态语言。
- 本合同负责第一条窄 pipeline 的形状与落点。

后续如果要继续实现，应围绕这条管道去展开，而不是先把 LLVM 或 `WorldIR` 名字提前固化成 truth source。
