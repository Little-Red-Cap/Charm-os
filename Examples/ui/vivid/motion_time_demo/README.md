# motion_time_demo

这个示例验证 Vivid `Managed UI Time v0` 的最小语义。

它刻意不依赖 SDL、不构建页面，也不接 Player 转场，只检查 runtime 如何按 `MotionTier` 托管时间：

- `Rich60Fps` 保留连续 elapsed time。
- `Cheap30Fps` 量化到约 33ms 的采样步长。
- `StaticCut` 直接采样 end state。
- `EinkDissolve` 在最终刷新前保持起点，结束时采样终点。
- `None` 表示没有 motion，直接落到最终状态且不需要采样。

构建：

```bash
cmake -S Examples/ui/vivid/motion_time_demo -B Examples/ui/vivid/motion_time_demo/build -G Ninja
cmake --build Examples/ui/vivid/motion_time_demo/build
Examples/ui/vivid/motion_time_demo/build/vivid-motion-time-demo
```
