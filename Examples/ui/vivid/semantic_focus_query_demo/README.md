# semantic_focus_query_demo

This demo verifies Semantic Focus Query v0.

Evidence target:

- Semantic focus lookup is scoped to the requested root.
- Query reports resolved, not-focusable, disabled, outside-active-scope, ambiguous, missing, and invalid-request states explicitly.
- Query remains lookup-only and does not commit focus transfer or input events.
- Rejected focus queries leave focus truth unchanged.
- Final `causal_chain` closes lookup, rejection status coverage, traversal artifact, and no-mutation evidence.

CTest guards:

```text
[sfq] run=semantic_focus_query_demo phase=end result=ok cases=9
```
