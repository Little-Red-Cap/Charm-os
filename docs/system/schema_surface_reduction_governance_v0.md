# Schema Surface Reduction Governance v0

## 定位

这份文档是 `schemas/` 的治理合同，不是新的 schema 实现计划。

目标是把 schema 面从“每新增一个 artifact 就复制一套 JSON 宇宙”收回到可复用、可解释、可归因的 artifact contract 语言：

```text
contract / schema / shared definition
  own public artifact shape

exporter / validator / smoke
  consume and verify that shape
```

本刀只立法和盘点，不删除 schema、不移动 schema、不修改 exporter / validator / smoke / compare，也不改任何现有 artifact JSON 输出。

`Examples/` 完全不纳入本轮治理。

## 四层 schema 分层

### `artifact contract schema`

一等 artifact 的公开边界。

职责：

- 声明 artifact 的 `schema` / `kind` / root shape。
- 约束人和工具可依赖的字段。
- 把 contract 中已经冻结的语义落成可校验对象。

边界：

- 不应把单个 exporter 的临时内部结构直接固化成长期协议。
- 不应靠字段名暗示 compare / route / session 语义；正式含义必须回指 contract。

### `projection schema`

由已有 summary / consumer / witness / route / ledger 投影而来的 artifact 边界。

职责：

- 只读取上游已经给出的判决或事实。
- 把上游事实投影成新的可读 artifact。
- 保留 provenance，让上层知道该判决来自哪里。

边界：

- 不重审下层 raw evidence。
- 不重新运行 selection / compare / opening judgment。
- 不把投影层写成第二套 system compiler brain。

### `compare schema`

既有 compare verdict 的公开边界。

职责：

- 表达 baseline / candidate 之间已经定义的 compare 结果。
- 让 drift / collapsed / standing 等 verdict 可由工具和 front-page 消费。
- 保持 compare input、surface 与 collapse surface 可追踪。

边界：

- 不新增第二个 compare brain。
- 不在 schema 中偷偷扩展 verdict 语义。
- 不绕过对应 compare contract 重新解释下层 artifact。

### `shared definition candidate`

跨多个 schema 重复出现、但尚未抽成共享定义的结构词汇。

职责：

- 登记重复结构，避免继续复制。
- 为后续 `$defs`、共享 schema library 或 validator helper 做准备。
- 让新 schema 先回答“为什么不能复用已有结构”。

边界：

- v0 只登记候选，不抽取实现。
- 候选不是新的 artifact kind。
- 候选不改变现有 JSON shape。

## 硬规则

### 新 artifact 不默认新增完整 schema 家族

新增 artifact 时，不再默认新增：

```text
summary schema
compare schema
sample schema
workspace schema
opening / route / witness sidecar schema
```

必须先判断是否能复用既有 artifact envelope、compare envelope、surface/ref shape 或 shared definition candidate。

### 语义不得只靠字段名存在

以下语义不得只通过 schema 字段名隐式表达：

- compare verdict。
- opening judgment。
- selected surface / selected focus。
- route / explain / handoff 判决。
- runtime session verdict。
- drift / collapse 的正式含义。

这些语义必须能在 contract、schema 注释、shared vocabulary 或源码侧找到第一解释位置。

### 重复结构先登记再抽取

下列结构在 v0 先登记为 shared definition candidate：

- `schema` / `kind`
- `result` / `status` / `verdict`
- `summary_path` / `report_path` / `check_path`
- `artifact_ref` / `source_artifact` / `evidence_refs`
- `surface_id` / `selected_surface`
- `consumer_summary_ref` / `explain_hop_ref`
- `baseline` / `candidate` / `compare_summary`

后续如果抽成 `$defs` 或共享 schema library，应另开 `Schema Shared Definitions Pilot v0`，并保持现有 artifact JSON 兼容。

### compare 与 projection 不合并

compare schema 只表达比较结果。

projection schema 只表达从上游判决到下游 artifact 的投影。

二者可以互相引用 provenance，但不能把 projection 变成 compare，也不能把 compare 变成新的 artifact selection engine。

## 当前盘点基线

详细族群清单与第一批 pilot 候选见：

- [`schema_surface_reduction_inventory_v0.md`](schema_surface_reduction_inventory_v0.md)

盘点口径：

- 当前 Git 已跟踪的 `schemas/` 下 JSON 文件。
- 不统计当前隔离的未跟踪 `world_shelf_review` schema 文件。
- 不统计 `Examples/`、build output、`out/` 或第三方资产。
- 使用 PowerShell 只读统计命令，未修改 schema 文件。

当前结果：

| 指标 | 数值 |
| --- | ---: |
| schema JSON 文件 | 89 |
| schema 总行数 | 45986 |
| 最大 schema | `schemas/system_compiler_summary.v0.schema.json` |
| 最大 schema 行数 | 2513 |
| `system_compiler` 族群 | 39 文件 / 21175 行 |
| `front_page.opening_flow` 族群 | 13 文件 / 8222 行 |
| `front_page.opening_testimony` 族群 | 6 文件 / 2943 行 |
| `minimal_kernel` 族群 | 10 文件 / 4842 行 |

这说明 schema 膨胀主体与脚本面类似，集中在 system compiler / front-page / opening-flow / testimony 这一侧，而不是 ARMv7-A QEMU lower-half。

## 后续收敛优先级

推荐顺序：

1. 先盘点同族 schema 中重复的 root envelope、ref、status、surface、compare shape。
2. 选择 `front_page.opening_flow` 作为第一批 shared definitions pilot。
3. 抽取只改变 schema 内部复用方式、不改变 artifact JSON shape 的 `$defs`。
4. 再考虑 validator helper 或 schema-aware library。
5. 最后才考虑合并 schema 文件或迁移 schema 命名。

## 非目标

本 contract v0 不做：

- 不修改任何现有 schema。
- 不新增 schema kind。
- 不新增 exporter、validator、smoke 或 compare。
- 不改变 artifact JSON 输出。
- 不改变 compare verdict。
- 不治理 `Examples/`。
- 不把 shared definition candidate 提前实现成 `$defs`。

## 验收标准

本刀完成后应满足：

- `docs/README.md` 与 `docs/system/README.md` 能找到这份治理合同。
- `docs/architecture/system_compiler_vocabulary_v0.md` 有 `SchemaSurface` / `SharedDefinitionCandidate` 最小词条。
- `git diff --check` 通过。
- 没有改动 `Examples/`。
- 没有新增或修改 schema、validator、exporter、smoke、compare verdict。
