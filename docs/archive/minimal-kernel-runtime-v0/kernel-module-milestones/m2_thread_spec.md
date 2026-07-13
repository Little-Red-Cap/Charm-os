# M2 Thread/Blocking Spec (Draft)

> `status`: `archived`。接口行为需以当前 thread module 源码为准。

## 1. ThreadTask
- `ThreadControl::finish()` stops further tick processing.
- `on_start()` resets control state (if provided by task).
- `init` event is delivered once at startup.
- `tick` drives step function.
- `terminate` is always delivered (even if finished), used for cleanup.

## 2. ThreadBlockingTask
- `block()` suppresses events outside `UnblockMask`.
- `resume()` allows all events.
- `UnblockMask` 默认放行 `sync/init/terminate`，可按任务自定义。

## 3. 历史使用意图

- `ThreadTask` 用于 cooperative step-driven logic；
- `ThreadBlockingTask` 用于等待 sync/IPC，并由 `UnblockMask` 保留必要事件。

原 `Draft/Examples/windows/main_m2.cpp` 已不存在，不能作为当前行为证据。

