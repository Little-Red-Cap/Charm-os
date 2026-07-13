# Schema Surface Reduction Governance v0

> **文档状态：`supporting`**

这份文档约束 `schemas/` 的新增与复用，不是 schema 实现计划，也不定义 Charm Core。
公开 artifact shape 由 contract/schema 负责；exporter、validator 和 smoke 只消费并验证该 shape。

历史数量盘点与 pilot 建议见
[`tool-surface-reduction-v0`](../archive/tool-surface-reduction-v0/)。

## 四类边界

### Artifact Contract Schema

- 声明公开 artifact 的 identity、root shape 和可依赖字段；
- 不把单个 exporter 的临时内部结构直接冻结成长期协议。

### Projection Schema

- 只投影上游已经给出的事实和 verdict，并保留 provenance；
- 不回读 raw evidence，不重新执行 selection、compare 或 opening judgment。

### Compare Schema

- 表达 contract 已定义的 baseline/candidate 比较结果；
- 不通过新增字段偷偷扩展 drift、standing 或 collapsed 的语义。

### Shared Definition

- 用于复用 identity、result/status、artifact ref、path 和 compare envelope 等重复结构；
- `$defs` 或共享 schema 的抽取必须保持现有 artifact JSON shape 与 validator 兼容。

## 准入规则

- 新 artifact 不默认新增 summary、compare、sample、workspace、route 或 witness schema 家族；
- compare verdict、selected focus/surface、route/explain/handoff 和 runtime verdict
  不得只靠字段名隐式表达；
- compare 与 projection 保持分离，二者只能通过 provenance 引用；
- 重复结构先证明消费者和兼容需求，再抽取 shared definition；
- schema presence 只证明格式可校验，不证明 producer、consumer 或运行行为成立。

## 增改前检查

- 是否能复用已有 envelope、path/ref、status/result 或 shared definition；
- 新字段是否在 contract、词汇表或源码中有第一解释位置；
- exporter、validator、sample 和 consumer 是否需要同步；
- schema 内部复用是否保持 wire shape 不变；
- `Examples/`、build output、`out/` 和未跟踪实验材料是否被排除。

不要因字段重复就新增 artifact kind，也不要把 schema 数量或 sample 完整度当作系统能力证据。
