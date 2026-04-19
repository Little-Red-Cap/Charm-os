# signal_state_closure_demo

这个示例把 Charm signal/state v0 第一版闭环里最关键的几层放到同一条最小路径里：

- `service.signal` 做同域同步边沿广播
- `service.state` 表达当前真相与变化通知
- `init.connection` 把长期 wiring 继续留在 `materialize / observe` 可见面
- `service.deferred_signal` 把跨上下文动作显式送进 `AppHost` 的 task-local `demand` poster

它不是 system compiler 自动生成 binding 的演示。

当前 v0 还没有把：

- `init.connection`
- `materialized connection`
- runtime `poster`

直接编译成一条自动执行链。

这个示例要冻结的是更克制的事实：

- 静态 direct / deferred wiring 可以被 graph 看见
- 同域内该用 `emit()` / `state.set()` 的地方可以同步完成
- 跨 task 的部分必须显式 `post()`
- 运行时 worker 不会被 direct call 偷偷拉起

示例里覆盖的语义点：

- `knob_delta.emit(5)` 会同步更新本地 `volume state`
- `volume.changed()` 会显式 `post()` 到 worker 的 `demand` lane
- `host.dispatch_batch(...)` 之前 worker 不应被调用
- `knob_delta.emit(0)` 仍是边沿事件，但不会制造新的状态变化
- `init.connection` 的 direct / deferred 两条关系在 materialized graph 中都可见

构建：

```bash
cmake -S Examples/system/signal_state_closure_demo -B Examples/system/signal_state_closure_demo/build -G Ninja
cmake --build Examples/system/signal_state_closure_demo/build
```
