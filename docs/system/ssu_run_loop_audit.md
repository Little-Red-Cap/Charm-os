# SSU RunLoop/Phase 提交来源审计（Phase 2）

## 目标

在不改变现有调度行为的前提下，把 run_loop / phase 路径的 submit 语义做成可审计事实。

## 当前规则

- `add_scheduler_step(...)` 默认标注为 `event-submit`
- `add_reactor_step(...)` 默认标注为 `io-ready-submit`
- 直接 `loop.add_step(...)` 的调用点，如未显式标注，视为 `unclassified`（待映射）

> 说明：`submit_projection` 目前仅用于审计标签，不参与调度行为分流。

## 代码落点

- `Modules/system/loop/system_run_loop.cppm`
  - `LoopStep.submit_projection`
  - `kSubmitProjectionEvent`
  - `kSubmitProjectionIoReady`

## 已知调用点审计

### 1) `Examples/project/player/stn32h747_HQZY/CM7/app/main.cpp`

- `add_reactor_step(..., LoopPhase::io, ...)` -> `io-ready-submit`
- `add_scheduler_step(..., LoopPhase::update, ...)` -> `event-submit`

### 2) `Examples/project/player/win/main.cpp`

- `loop.add_step(LoopPhase::io, ...)` -> `event-submit`
- `loop.add_step(LoopPhase::update, ...)` -> `event-submit`
- `loop.add_step(LoopPhase::render, ...)` -> `event-submit`

## 下一步

1. 对 `win/main.cpp` 的三个 `add_step` 显式补 submit_projection。
2. 形成“新增 run_loop step 必须声明 submit_projection”的评审项。
3. 在 scheduler observability 中评估是否输出 phase+projection 的最小统计（后续）。

