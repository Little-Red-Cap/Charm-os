# semantic_default_demo

This demo verifies Pattern Semantic Defaults v0.

The rule is opt-in:

- Vivid can derive default semantic role from `WidgetKind`.
- Vivid can derive default label from widget text when no label override is supplied.
- Product code still supplies stable semantic id.
- Decorative widgets stay non-semantic until explicitly opted in.
- Explicit semantic assignment remains able to override defaults.
- A final `causal_chain` closes role derivation, label source, override, decorative boundary, and tree artifact evidence.

CTest guards:

```text
[sdef] run=semantic_default_demo phase=end result=ok cases=7
```
