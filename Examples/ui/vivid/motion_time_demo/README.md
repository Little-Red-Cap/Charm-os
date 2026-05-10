# motion_time_demo

这个示例验证 Vivid `Managed UI Time v0` 的最小语义。

它刻意不依赖 SDL、不构建页面，也不接 Player 转场，只检查 runtime 如何按 `MotionTier` 托管时间，以及如何把采样结果投影为 `LayerTransform`。

- `Rich60Fps` 保留连续 elapsed time。
- `Cheap30Fps` 量化到约 33ms 的采样步长。
- `StaticCut` 直接采样 end state。
- `EinkDissolve` 在最终刷新前保持起点，结束时采样终点。
- `None` 表示没有 motion，直接落到最终状态且不需要采样。
- `sample_layer_motion()` 验证时间采样、`LayerProfile` opacity 降级和 layer transform 插值的最小接线。
- `sample_motion_recipe()` 验证 `fade` / `slide` / `fade_slide` / `cut` 到 `LayerMotionSpec` 的最小翻译。
- `MotionTransitionRunner` 验证 begin / sample / finish / cancel 的最小生命周期。
- `MotionTransitionTrace` 验证 sample / compose / finish / cancel 的最小证据账本。
- `make_motion_compose_spec()` 验证 transition frame 到 `LayerComposeSpec` 的纯函数桥。
- `dry_run_motion_compose()` 验证 transition frame 经 `SnapshotStore` 形成 compose plan 和 budget 证据。
- `decide_motion_compose_profile()` 验证 dry-run budget 到 effective profile / fallback reason 的裁决桥。
- `execute_motion_compose()` 验证 transition frame 经 `Scene` 执行 pixel snapshot compose 的最小路径。
- `PageMotionTransition` 验证 `PageLayer` freeze / transitioning / execute / thaw 的最小路径。
- 最终 `causal_chain` 将托管时间、recipe、compose、budget、trace 与 page motion 证据收束为 `motion_time.managed` 判决，并在最终行显式输出 `time_ok` / `recipe_ok` / `compose_ok` / `budget_ok` / `trace_ok` / `page_motion_ok`。

示例 stdout 遵守 `docs/ui/vivid_evidence_stdout_law.md`：统一为 `[mt] run=motion_time_demo phase=begin/end` 与 `[mt] case=...` 的 summary 形式，并由 CTest 约束最终 `result=ok cases=13`。

构建：

```bash
cmake -S Examples/ui/vivid/motion_time_demo -B cmake-build-vivid-motion-time-demo-codex -G Ninja
cmake --build cmake-build-vivid-motion-time-demo-codex -j 22
ctest --test-dir cmake-build-vivid-motion-time-demo-codex --output-on-failure
cmake-build-vivid-motion-time-demo-codex/vivid-motion-time-demo
```
