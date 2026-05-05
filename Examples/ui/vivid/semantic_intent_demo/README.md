# semantic_intent_demo

This demo verifies Semantic Intent Resolution v0.

Evidence target:

- Semantic id/action lookup is scoped to the requested root.
- Intent resolution reports resolved, unsupported, missing, ambiguous, disabled, and invalid-request states explicitly.
- Resolution remains lookup-only and does not synthesize input, focus, press, or click side effects.
- Rejected resolutions leave input state unchanged.
- Final `causal_chain` closes lookup, rejected status coverage, traversal artifact, and no-mutation evidence.

CTest guards:

```text
[sint] run=semantic_intent_demo phase=end result=ok cases=9
```
