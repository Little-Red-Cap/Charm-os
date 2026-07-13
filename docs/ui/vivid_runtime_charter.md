# Vivid Product UI Runtime Charter

## 文档状态

- `status`: `supporting`
- `scope`: Vivid runtime ownership、语义/产物/策略/证据边界
- `authority`: [`ui_kernel_contract.md`](ui_kernel_contract.md)

本文不是 API 清单、实现日志或 roadmap。具体结构以 Vivid source、专题契约和当次 evidence 为准。

## Runtime 职责

Vivid runtime 负责把产品 UI 语义在给定资源和 backend 约束下转换为可执行渲染产物，并保留准入、
降级、生命周期和结果证据。它不拥有产品业务状态、板级设备、窗口系统或通用应用调度。

稳定处理链为：

```text
product state / semantic intent
    -> UI semantic state
    -> policy + budget + admission
    -> live tree or frozen render artifact
    -> compose / execute
    -> evidence
```

## 四个平面

| 平面 | 责任 | 不拥有 |
|---|---|---|
| Semantic | Page、component/pattern、focus、navigation、theme 与产品状态投影 | PixelSurface、DrawCmd、SDL/HAL、backend 选择 |
| Artifact | live tree、DrawCmd、CommandSnapshot、Pixel/Tile surface、glyph/image ref 的生命周期 | 产品业务含义与板级资源策略 |
| Policy | profile、budget、admission、motion tier 和 backend capability 下的执行形态选择 | widget 业务状态与隐式 fallback |
| Evidence | state delta、invalidation、capture/compose、fallback、资源峰值和 artifact 结果 | 新的系统事实或未执行路径的成功声明 |

同名数据出现在不同平面时不能互相替代。Semantic identity 不等于 widget handle，artifact hash 不等于
视觉正确，profile 请求也不等于 admission 成功。

## 核心不变量

- Pattern/component 表达产品语义、状态和布局关系，不直接选择 snapshot、surface 或 backend。
- live tree 与 frozen artifact 必须区分；`hide()` 改变可见性，`freeze()` 改变产物生命周期。
- profile 是请求；budget 和 backend capability 参与 admission；admission 在 capture/分配前完成。
- `StaticCut` 是合法执行形态；fallback 和 reject 必须带可观察原因。
- snapshot 的 acquire、stale、replay、release 和中断回收必须由 owner 闭合。
- runtime 时间由 frame/runtime owner 提供；motion 只能消费受控时间和已准入 recipe。
- focus、semantic action 和 transition 的 query/admission 阶段不得产生执行副作用。
- commit、cancel、interrupt 和 reject 必须保持 page/focus/state truth 与 artifact 生命周期一致。
- evidence 只能报告真实输入与执行结果，不能用 schema、demo 数量或日志格式代替行为证明。

## Layer、Motion 与 Style

`PageLayer` 是 live root 与 frozen artifact 的生命周期边界。snapshot 类型、预算、capture、compose、
release 与 fallback 由 [`vivid_layer_runtime_v0.md`](vivid_layer_runtime_v0.md) 定义。

页面 transition 按事务处理：begin 前 admission，commit 提交 page truth，cancel/interrupt 回滚并释放
snapshot。时间 tier、recipe、transition runner 和 evidence 见
[`vivid_motion_runtime_v0.md`](vivid_motion_runtime_v0.md)。

视觉 token 从产品/page 局部事实开始，只有存在重复 consumer 和一致语义时才上收。token、state mask、
metrics/color evidence 与 paint/layout impact 见
[`vivid_style_token_law_v0.md`](vivid_style_token_law_v0.md)。

## Focus 与 Semantic

Focus 的 state truth、visual artifact、scope、transfer 和 semantic identity 是不同边界：

- focus style/artifact：[`vivid_focus_evidence_boundary_v0.md`](vivid_focus_evidence_boundary_v0.md)；
- transfer：[`vivid_focus_transfer_evidence_v0.md`](vivid_focus_transfer_evidence_v0.md)；
- scope 与 navigation：[`vivid_focus_scope_evidence_v0.md`](vivid_focus_scope_evidence_v0.md)；
- semantic focus：[`vivid_focus_semantic_evidence_v0.md`](vivid_focus_semantic_evidence_v0.md)。

产品提供 stable semantic id；Vivid 可以派生 role、label、action mask 和 tree artifact，但不能凭 widget
位置发明稳定身份。semantic query/resolution 只读取；admission 只生成执行计划；request 才能通过正常
input/focus 路径提交副作用。请求 ledger 与 transition 边界见：

- [`vivid_semantic_request_ledger_law_v0.md`](vivid_semantic_request_ledger_law_v0.md)；
- [`vivid_semantic_transition_law_v0.md`](vivid_semantic_transition_law_v0.md)；
- [`vivid_semantic_action_state_transition_law_v0.md`](vivid_semantic_action_state_transition_law_v0.md)。

## Evidence 与验证

Evidence chain、stdout 和推荐 fixture 从以下入口进入：

- [`vivid_render_evidence_chain_v0.md`](vivid_render_evidence_chain_v0.md)；
- [`vivid_causal_verdict_law_v0.md`](vivid_causal_verdict_law_v0.md)；
- [`vivid_evidence_vocabulary_law_v0.md`](vivid_evidence_vocabulary_law_v0.md)；
- [`vivid_evidence_lab_manifest_v0.md`](vivid_evidence_lab_manifest_v0.md)。

每个机制至少需要一个成功行为、一个关键拒绝/失败行为和资源/生命周期收尾证据。Host fixture 只证明
对应语义和 artifact；产品视觉、性能与真实板 display/input/cache 仍需各自证据。

## 非目标

- 不建立窗口管理器、任意层 compositor 或通用 reactive runtime。
- 不把所有 Player 私有 pattern、页面状态或资源策略上收进 Vivid。
- 不让 motion/style API 外观优先于 admission、容量和失败语义。
- 不用全局 registry、backend 宏或 platform identity 绕过稳定边界。
- 不把 dated demo 结果或阶段排期写入 charter。

UI 文档总路由见 [`README.md`](README.md)。
