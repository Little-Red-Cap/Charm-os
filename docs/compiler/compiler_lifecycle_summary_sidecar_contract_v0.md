# Compiler Lifecycle Summary Sidecar v0

> `status`: `supporting`

`compiler_lifecycle.summary.json` 是已有 artifact report、runtime session/ledger、witness bundle 和 world
compare 的只读投影。它不是 world truth、freeze receipt、archive manifest 或 compare verdict，也不扫描
源码、build tree 或 raw log 推断隐藏事实。

字段、状态标签和派生规则由
[`export_compiler_lifecycle_summary.py`](../../scripts/export_compiler_lifecycle_summary.py) 定义；gate 只检查
sidecar shape 与下列 honesty constraints，不是通用 schema validator。Report consumer 只渲染，wrapper
只编排 exporter、gate 与 report。

## Honesty Constraints

- 没有 freeze receipt 时，`frozen` 保持 missing/interpretive。
- artifact report 只能为 `lowered` 提供解释性投影。
- 没有 archive manifest 时，`archived` 不能升级为强证明。
- runtime observation 不修改语义输入。
- compare source 只投影已有结果，不产生新事实。
- sidecar 自身的存在不能提高任何状态的 coverage。
- 显式输入缺失或 JSON 无法解析必须进入 violations，不能以默认值伪造成功。

## 验证

基础投影由
[`compiler_lifecycle_summary_sidecar_smoke.ps1`](../../scripts/compiler_lifecycle_summary_sidecar_smoke.ps1)
覆盖；runtime evidence bundle 接线由同目录 hook smoke 维护。通过只证明投影与 honesty checks 成立，
不证明完整 compiler lifecycle 已实现。
