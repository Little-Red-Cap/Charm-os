# semantic_action_demo

This demo verifies Semantic Action Artifact v0.

Evidence target:

- Button and ListItem expose role-derived `activate`.
- Container and Text expose no default action.
- Explicit action override can clear or promote action masks.
- Semantic tree nodes carry action masks.
- `semantic_hash` is stable when action facts are stable and changes when action masks change.
- Final `causal_chain` closes action capability, override, tree action, and hash evidence.

CTest guards:

```text
[sact] run=semantic_action_demo phase=end result=ok cases=7
```
