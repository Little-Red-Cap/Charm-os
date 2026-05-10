# semantic_action_request_demo

This demo verifies Semantic Action Request v0.

Evidence target:

- Semantic action request crosses from intent resolution into controlled execution.
- Request ledger records final stage, status, reject reason, focus readiness, and click execution evidence.
- A committed activate request prepares focus, emits a click, and uses normal widget behavior.
- Already-focused execution avoids hidden focus transfer.
- Unsupported, outside-scope, ambiguous, and missing-id requests are rejected without click or focus pollution.
- Final `causal_chain` closes request execution, state mutation, event artifact, and rejected-no-mutation evidence.

CTest guards:

```text
[sar] run=semantic_action_request_demo phase=end result=ok cases=11
```
