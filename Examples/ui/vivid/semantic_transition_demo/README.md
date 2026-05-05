# semantic_transition_demo

这个示例验证 Vivid `Semantic-to-Transaction Evidence v0` 的最小跨轴链路。

它不新增 navigation core API，也不让 semantic request 直接改 page truth。示例只证明：

```text
SemanticActionRequest(nav.library.open)
  -> emitted click
  -> demo-side application bridge starts PageTransitionRunner
  -> PageTransitionRunner admits / samples / commits
  -> page truth and snapshot lifecycle close
```

关键证据：

- `baseline_page_truth` 验证 source visible、destination hidden、无 snapshot。
- `semantic_request_ledger` 验证 request 为 `Executed` 且 `emitted_click=1`。
- `request_event_trace` 验证 click 是通过 normal semantic action path 发出。
- `transition_begin` / `transition_sample` / `transition_commit` 验证 PixelDouble 事务路径。
- `snapshot_lifecycle` 验证 commit 后 `snapshot_count == 0`。
- `rejected_request_no_transition` 验证 disabled target 被拒绝后不启动 transition、不改变 page truth。
- `causal_chain` 用 evidence-referenced 字段收束 semantic / edge / admission / transaction / layer evidence。

示例 stdout 遵守 `docs/ui/vivid_evidence_stdout_law.md`，由 CTest 约束最终：

```text
[stx] run=semantic_transition_demo phase=end result=ok cases=9
```
