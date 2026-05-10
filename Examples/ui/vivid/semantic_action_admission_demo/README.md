# semantic_action_admission_demo

This demo verifies Semantic Action Admission v0.

Evidence target:

- Semantic intent resolution can be turned into an action execution plan.
- Admitted targets report focus preparation and click emission plans.
- Admission is planning-only and does not mutate focus, press, input events, or checked state.
- Unsupported, disabled, ambiguous, missing, invalid-root, and missing-id requests are rejected explicitly.
- Rejected admissions do not emit click plans or mutate input truth.
- Final `causal_chain` closes plan artifact, rejection status coverage, and no-mutation evidence.

CTest guards:

```text
[saa] run=semantic_action_admission_demo phase=end result=ok cases=9
```
