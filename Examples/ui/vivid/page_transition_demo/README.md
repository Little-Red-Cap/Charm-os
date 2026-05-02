# page_transition_demo

这个示例验证 Vivid 的双页 `PageTransitionRunner` 最小事务语义。

阶段性运行形态矩阵见 `docs/ui/vivid_motion_runtime_v0.md` 的 “PageTransition v0 运行形态矩阵”。

它不追求动画效果，而是验证这些收尾律：

- normal commit 后 source snapshot / destination snapshot 都被释放
- fade_slide recipe 在 PixelDouble 路径中同时驱动 transform / opacity
- Cheap profile 下 fade_slide 会量化 motion time 与 opacity
- Static / None profile 下 fade_slide 不会绕过 profile 降级与拒绝规则
- cancel 后 source / destination live 可见性恢复到 begin 前
- Static profile 是主动 static cut 运行形态，不依赖预算失败
- None profile 拒绝转场事务，不调用 prepare，不改变 page truth
- CommandSnapshot admission 当前不做双页 replay，显式降级为 static cut
- PixelSingle + fade_slide 只捕获 source snapshot，destination 保持 live
- PixelSingle cancel 后 source snapshot 释放，page truth 恢复到 begin 前
- PixelSingle active transition 上再次 begin 时先释放旧 source snapshot，再启动新事务
- destination prepare 失败时释放已获取的 source snapshot
- source / destination capture 失败时恢复 page truth 并释放已获取 snapshot
- active transition 上再次 begin 时先 abort 旧事务，再启动新事务

构建：

```bash
cmake -S Examples/ui/vivid/page_transition_demo -B cmake-build-vivid-page-transition-demo-codex -G Ninja
cmake --build cmake-build-vivid-page-transition-demo-codex -j 22
cmake-build-vivid-page-transition-demo-codex/vivid-page-transition-demo
```
