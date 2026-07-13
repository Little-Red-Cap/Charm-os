# Evidence Front-page Routing 保留笔记

> status: `archived`
>
> scope: 已有 verdict 到 reader/explain artifact 的只读路由边界

当前 provenance 兼容边界见
[`front_page_route_provenance_compatibility_contract_v0.md`](../../system/front_page_route_provenance_compatibility_contract_v0.md)。
本文不定义新 schema、verdict、compare 或 front-page object。

## Verdict Ownership

产生 evidence/compare verdict 的 exporter 或 validator 拥有判断。上层 reader 只能消费已经验证的 summary
和 artifact refs，不能重新解析 raw log、baseline/candidate 或 lower-level graph 后给出第二个 verdict。

排序 focus、选择默认 artifact 或提供 fallback 只是阅读策略。它不得新增 failure code、runtime fact、
severity 语义或“更正确”的 compare 结论。

## Route 与 Explain

一条最小阅读路径只需要：

1. validated source summary；
2. source 已声明的 selected/default focus；
3. preferred artifact ref 与有限 fallback；
4. 选择理由和 provenance；
5. 最终可读、可解释的 target。

Route、explain entry 和 handoff 可以是不同工具投影，但不需要各自成为新的领域对象或 schema family。
每增加一层都必须有独立 consumer，不能只把同一组 refs 改名再导出。

## Compare

Reader 复用现有 compare object。它可以按 regression、failure domain 或 focus 排序，但不能绕过 compare
summary 重新读取两侧工件。结构、runtime、resource 或 evidence compare 保持各自 verdict 和证据域。

## Provenance

路由结果至少记录 source summary、selected focus、selected artifact、fallback、选择规则版本和生成工具。
路径存在只证明可定位，不证明 artifact 内容有效；reader 仍需检查 schema、可读性和 source result。

Compatibility fallback 必须可见。缺失新 provenance 时使用旧字段，不得伪造“native provenance”。

## Failure Surface

以下情况应显式失败或降级，不能静默打开任意相邻 artifact：

- source summary 缺失、无效或 result 非法；
- selected/default focus 不存在；
- preferred artifact ref 缺失或不可读；
- route root 不受支持或 provenance 不完整；
- fallback 全部失效；
- handoff target 会绕过 explain surface 重新打开 raw evidence。

这些是 failure family，不是冻结错误码表。具体 code 由拥有该 exporter/consumer 的 schema 定义。

## 不增加的证明

- report/check/route 生成成功不证明 runtime 或 board 已运行；
- front-page summary 不增加 lower-level evidence；
- handoff 不等于执行、部署或 App launch；
- reader convenience 不构成新的 Core World、Witness 或 Judgment 模型。

## 当前入口

- [`system_compiler_roadmap.md`](../../architecture/system_compiler_roadmap.md)
- [`system_compiler_vocabulary_v0.md`](../../architecture/system_compiler_vocabulary_v0.md)
- [`artifact_report_v0.md`](../../system/artifact_report_v0.md)
- [`minimal_kernel_runtime_session_witness_inspect_compare_consumer_v0.md`](../../system/minimal_kernel_runtime_session_witness_inspect_compare_consumer_v0.md)
