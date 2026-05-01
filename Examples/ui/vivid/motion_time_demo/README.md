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

构建：

```bash
cmake -S Examples/ui/vivid/motion_time_demo -B Examples/ui/vivid/motion_time_demo/build -G Ninja
cmake --build Examples/ui/vivid/motion_time_demo/build
Examples/ui/vivid/motion_time_demo/build/vivid-motion-time-demo
```
