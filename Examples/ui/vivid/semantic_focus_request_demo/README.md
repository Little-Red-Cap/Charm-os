# semantic_focus_request_demo

This demo verifies Semantic Focus Request v0.

Evidence target:

- Semantic focus request crosses from focus admission into controlled focus transfer.
- Request ledger records final stage, status, admission result, transfer plan, and FocusOut/FocusIn evidence.
- A committed focus request mutates semantic focus truth and moves the focus-ring artifact.
- Focus style evidence remains stable because focus is outside the normal style mask.
- Rejected requests preserve focus truth and render artifact evidence.
- Final `causal_chain` closes request execution, focus state delta, style stability, artifact migration, and rejected-no-mutation evidence.

CTest guards:

```text
[sfr] run=semantic_focus_request_demo phase=end result=ok cases=12
```
