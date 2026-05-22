# Schema Examples Hygiene v0

## 定位

这是一份 `schemas/examples/` 级别的样本卫生边界页，不是新的 schema governance，也不是 validator 设计。

它只说明：样本 JSON 里的静态仓库引用应该如何被轻量 smoke 检查，避免把 fixture 里的说明性路径、文档回指和脚本回指静默写坏。

## 目前范围

- 只检查 `schemas/examples/*.json`。
- 只认仓库内的静态前缀：`docs/`、`schemas/`、`Examples/`、`scripts/`、`.github/`。
- 明确放过 `out/`、绝对路径、URL 和其它运行时生成引用。
- 入口层使用 [`../../scripts/schema_examples_hygiene_smoke.ps1`](../../scripts/schema_examples_hygiene_smoke.ps1)。

## 非目标

- 不做 schema 结构校验。
- 不做 compare。
- 不接默认 CI gate。
- 不把 sample hygiene 提升成 schema governance verdict。
- 不改变现有 sample JSON 形状。

## 使用方式

当样本引用需要做最小卫生检查时，先跑 hygiene smoke；如果失败，再回到对应 sample 修正静态引用。

- [`../../scripts/schema_examples_hygiene_smoke.ps1`](../../scripts/schema_examples_hygiene_smoke.ps1)
- [`../../scripts/schema_examples_static_refs_smoke.ps1`](../../scripts/schema_examples_static_refs_smoke.ps1)
