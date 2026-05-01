# page_transition_demo

这个示例验证 Vivid 的双页 `PageTransitionRunner` 最小事务语义。

它不追求动画效果，而是验证这些收尾律：

- normal commit 后 source snapshot / destination snapshot 都被释放
- cancel 后 source / destination live 可见性恢复到 begin 前
- low budget 路径不做 PixelDouble capture，直接 static cut
- destination prepare 失败时释放已获取的 source snapshot

构建：

```bash
cmake -S Examples/ui/vivid/page_transition_demo -B cmake-build-vivid-page-transition-demo-codex -G Ninja
cmake --build cmake-build-vivid-page-transition-demo-codex -j 22
cmake-build-vivid-page-transition-demo-codex/vivid-page-transition-demo
```
