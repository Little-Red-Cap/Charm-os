# scene_state_demo

这个示例专门用最小 SoA `SceneBuilder / SceneAccess` 代码，演示
Vivid scene/runtime 这一层应如何显式表达跨控件状态推进。

它刻意不走 object-level widget `observe_*`，而是把链路完整摊开：

- `scene.dispatch_event(...)`
- controller 读取 `access.input_event(...)`
- controller 更新 app-state
- controller 再通过 `SceneAccess` 回写其他 handle

这个示例当前冻结的语义点是：

- `SceneAccess` 是句柄驱动的 runtime 写入口，不是 object-level `observe_*` 镜像
- SoA 跨控件关系应显式写在 controller / app-state 中，而不是偷藏进 kernel
- `input_event_count()` 只描述当前一次 dispatch 的输入结果，不是累计事件日志

示例里用到的关系很朴素：

- 点击 checkbox 改变 `armed` 真相
- 点击 button 改变 `level` 真相
- controller 根据 app-state 显式回写 label / switch / progress

构建：

```bash
cmake -S Examples/ui/vivid/scene_state_demo -B cmake-build-vivid-scene-state-demo -G Ninja
cmake --build cmake-build-vivid-scene-state-demo
```
