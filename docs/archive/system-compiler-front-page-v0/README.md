# Evidence Front-page Routing 保留笔记

> `status`: `archived`

本文只保留 verdict 到 reader/explain artifact 的只读路由边界。当前 provenance 兼容规则见
[`front_page_route_provenance_compatibility_contract_v0.md`](../../system/front_page_route_provenance_compatibility_contract_v0.md)。

## Ownership

产生 evidence/compare verdict 的 exporter 或 validator 拥有判断。Reader 只能消费已验证的 summary 与
artifact refs，不能重新解析 raw log、baseline/candidate 或 lower-level graph 后生成第二个 verdict。

排序 focus、选择默认 artifact 或 fallback 是阅读策略，不得新增 failure code、severity、runtime fact 或
compare 语义。Route、explain entry 和 handoff 可以是不同工具投影，但每一层都必须有独立 consumer，
不能只改名导出同一组 refs。

## Route 与 Provenance

最小路由包含：

- validated source summary 与 selected/default focus；
- preferred artifact ref、有限 fallback 与选择理由；
- 生成工具/规则版本和最终 target。

路径存在只证明可定位，不证明 artifact 有效。Compatibility fallback 必须可见；缺失新 provenance 时
可以读取旧字段，但不得伪造 native provenance。

Reader 复用现有 compare object。它可以排序 regression、failure domain 或 focus，但不能绕过 compare
summary 重新比较 baseline/candidate，也不能混合 structure、runtime、resource 与 evidence verdict。

## Failure Surface

以下情况必须显式失败或降级，不能静默打开相邻 artifact：source summary 无效、focus 不存在、preferred
ref 不可读、route root 不受支持、provenance 不完整、fallback 全部失效，或 handoff 绕过 explain surface
重开 raw evidence。具体 code 由拥有该 exporter/consumer 的 schema 定义。

Report、route 或 handoff 成功不增加 lower-level evidence，也不证明 runtime、board、deployment 或 App
launch。相关边界见 [`system_compiler_roadmap.md`](../../architecture/system_compiler_roadmap.md)、
[`artifact_report_v0.md`](../../system/artifact_report_v0.md) 和
[`minimal_kernel_runtime_session_witness_inspect_compare_consumer_v0.md`](../../system/minimal_kernel_runtime_session_witness_inspect_compare_consumer_v0.md)。
