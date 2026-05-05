# semantic_focus_admission_demo

This demo verifies Semantic Focus Admission v0.

Evidence target:

- Semantic focus lookup can be turned into a focus transfer plan.
- Admitted transfer reports destination handle and expected FocusOut/FocusIn plan.
- Already-focused admission remains admitted but has no transfer plan.
- Admission is planning-only and does not mutate focus truth or emit focus events.
- Not-focusable, disabled, outside-active-scope, ambiguous, missing, invalid-root, and missing-id requests are rejected explicitly.
- Final `causal_chain` closes plan artifact, rejection status coverage, and no-mutation evidence.

CTest guards:

```text
[sfa] run=semantic_focus_admission_demo phase=end result=ok cases=9
```
