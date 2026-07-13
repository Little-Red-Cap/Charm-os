# Vivid Render Evidence Chain v0

## 文档状态

- `status`: `supporting`
- `scope`: UI state 到 render artifact 的因果证据分段
- `authority`: [`vivid_evidence_vocabulary_law_v0.md`](vivid_evidence_vocabulary_law_v0.md)、
  [`vivid_causal_verdict_law_v0.md`](vivid_causal_verdict_law_v0.md)

本文定义证据链及各段边界，不复制 demo stdout、case 数、helper API 或阶段 addendum。

## 因果链

```text
state truth
    -> invalidation intent
    -> dirty evidence
    -> draw command evidence
    -> render artifact evidence
    -> causal verdict
```

每一段都必须引用同一 case/run 中的前后事实。缺少中间段时只能声明相关性或局部结果，不能补写成
完整因果链。

## Evidence Segments

| 段 | 必须回答 | 不能单独证明 |
|---|---|---|
| State truth | 哪个稳定对象/字段从什么值变到什么值，来源是什么 | 已请求正确 invalidation 或已渲染 |
| Invalidation intent | 预期影响是 none、paint、layout、text/style/cache 等哪一类，作用域是什么 | runtime 真实 dirty 与 intent 一致 |
| Dirty evidence | 实际 dirty 数量、范围和摘要，是否越出声明边界 | draw 命令或最终像素正确 |
| Draw command evidence | record/execute 数量、失败、稳定摘要和支持边界 | 命令产生了预期像素 |
| Render artifact evidence | 尺寸、像素/区域摘要、前后是否变化及边界 | 主观视觉正确或变化来源唯一 |
| Style evidence | resolved color/metrics/state mask 与 impact | focus/navigation artifact 或产品主题正确 |
| Causal verdict | 上述 segment identity、成功/拒绝条件和最终结论 | 未引用 segment、其它证据域或未运行路径 |

字段的稳定名称、取值和 helper shape 由 evidence vocabulary/source 维护，本文不建立第二份字段表。

## State 与 Invalidation

State delta 必须使用稳定对象身份和字段名，并记录 old/new、changed 与 source。多个 child 同时变化时，
分别记录 delta；不能只输出 component 最终摘要后反推每个 cause。

Invalidation intent 是请求，不是结果。它至少包含 impact 和 claimed scope。`paint_only` 必须由实际 dirty
没有越界、layout/metrics 证据保持稳定来支持；没有 dirty 也需要解释是 no-op、rejected、deferred 还是
证据缺失。

Rejected query/admission/request 需要证明相关 state、focus、event、dirty 和 artifact 没有被污染。

## Dirty、Draw 与 Artifact

Dirty evidence 应能比较 baseline/after，并验证 component/page scope。单个 dirty rect 不自动表示工作量
最小；count、area、union 或 hash 的解释必须与实际 collector 一致。

Draw evidence只消费稳定统计和摘要，不依赖 `CmdHeader` payload、buffer partition 或 executor 私有布局。
详细边界见 [`vivid_draw_cmd_evidence_boundary_v0.md`](vivid_draw_cmd_evidence_boundary_v0.md)。

Artifact evidence至少保留尺寸、格式/范围上下文和稳定摘要。pixel hash 变化证明字节变化，不证明视觉
符合设计；hash 不变也不能证明所有不可见状态都未变化。PNG/screenshot 是可选投影，不能替代 state、
invalidation 和 draw segment。

## Style 与 Focus

Style evidence 应区分 color 与 metrics，记录 style state mask 和 impact。普通 style state、focus truth、
focus ring draw/artifact 是不同事实；focus 不得因视觉变化就被悄悄塞入普通 Button style mask。

Focus 的 state、transfer、scope 和 semantic artifact 从以下专题进入：

- [`vivid_focus_evidence_boundary_v0.md`](vivid_focus_evidence_boundary_v0.md)；
- [`vivid_focus_transfer_evidence_v0.md`](vivid_focus_transfer_evidence_v0.md)；
- [`vivid_focus_scope_evidence_v0.md`](vivid_focus_scope_evidence_v0.md)；
- [`vivid_focus_semantic_evidence_v0.md`](vivid_focus_semantic_evidence_v0.md)。

这些机制仍使用本链的 state/invalidation/draw/artifact segment，但执行和 admission 语义由 focus/semantic
专题约束。

## Semantic 与 Intent

Semantic tree、action/focus query、admission 和 request 不能合并为一个“语义成功”字段：

- tree/artifact 证明可读取的 semantic projection；
- query/resolution 必须无执行副作用；
- admission 只证明计划可形成；
- request 才能提交 click/focus/state 变化；
- transition 还需独立证明 page transaction 与 snapshot 收尾。

请求 ledger 与跨 state/render/transition 规则见：

- [`vivid_semantic_request_ledger_law_v0.md`](vivid_semantic_request_ledger_law_v0.md)；
- [`vivid_semantic_transition_law_v0.md`](vivid_semantic_transition_law_v0.md)；
- [`vivid_evidence_vocabulary_law_v0.md`](vivid_evidence_vocabulary_law_v0.md)。

semantic intent-to-artifact profile 还必须满足：

- semantic action 经正常 request ledger 和 input/action edge 执行，不能由 demo 直接 set widget state；
- committed action 对应一个 stable-id state delta；
- rejected action 证明无 edge、无 state delta、无 dirty/artifact mutation；
- invalidation impact 与实际 dirty containment 一致；
- DrawCmd/pixel hash 只作摘要，不作视觉审批。

该 profile 的 primary evidence route 是 `Examples/ui/vivid/intent_artifact_demo`，final `causal_chain`
必须连接 request、state、invalidation、artifact 与 rejected-no-mutation。

## Support 与 Promotion

[`vivid_evidence_support.hpp`](../../Examples/ui/vivid/support/vivid_evidence_support.hpp) 提供 demo-side
collector、comparison 和 stdout helper。它们用于减少 fixture 漂移，不是 Vivid core render/input API。

名称从 demo helper 提升为稳定 law/runtime artifact 前，必须有重复 consumer、字段语义、失败行为和迁移
证据。promotion 规则见
[`vivid_evidence_vocabulary_law_v0.md`](vivid_evidence_vocabulary_law_v0.md)。

## 验收要求

一个完整 component/intent case 至少覆盖：

1. baseline state 与 artifact；
2. 一个明确 cause 或 request；
3. state delta 和 invalidation intent；
4. dirty/draw/artifact 的 before/after；
5. scope、overflow、failed command 与 unsupported 状态；
6. 一个 no-op/reject/failure 负例，证明无污染；
7. 引用上述 segment 的 causal verdict。

推荐 fixture 与 stdout 规则从 [`vivid_evidence_lab_manifest_v0.md`](vivid_evidence_lab_manifest_v0.md) 和
[`vivid_evidence_stdout_law.md`](vivid_evidence_stdout_law.md) 进入。fixture 通过不等于产品视觉、性能或
真实板行为已通过。

component state/render 的 primary evidence routes 包括
`Examples/ui/vivid/component_card_state_demo` 与
`Examples/ui/vivid/component_settings_row_demo`；其 `causal_chain` 必须引用本页 required segments。

## 非目标

- 不用 screenshot/hash 代替完整因果链。
- 不把 demo helper、日志前缀或 case 名提升为 runtime API。
- 不依赖 draw command 私有 wire/layout 形成产品证据。
- 不把 semantic lookup、admission 和 execution 合并为隐式副作用。
- 不在本文维护 dated demo 结果、完成度或新增 case 记录。
