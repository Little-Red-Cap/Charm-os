# semantic_tree_demo

This demo verifies Vivid Semantic Tree Artifact v0.
It keeps the scene deliberately small and audits the runtime semantic collector instead of visual polish.

Evidence target:

- semantic nodes are collected from a root in deterministic preorder.
- decorative widgets without semantic entries are excluded.
- the current input focus is marked in the semantic artifact.
- root choice is an explicit artifact policy: root tree includes outside semantic nodes, scope tree does not.
- capacity overflow is explicit and stable.
- semantic hash is stable for identical semantic facts.

Stdout follows `docs/ui/vivid_evidence_stdout_law.md` and CTest guards:

```text
[stree] run=semantic_tree_demo phase=end result=ok cases=6
```

Build:

```bash
cmake -S Examples/ui/vivid/semantic_tree_demo -B cmake-build-vivid-semantic-tree-demo-codex -G Ninja
cmake --build cmake-build-vivid-semantic-tree-demo-codex -j 22
ctest --test-dir cmake-build-vivid-semantic-tree-demo-codex --output-on-failure
```
