# M2 API Freeze (Thread/Blocking)

> `status`: `archived`。历史 freeze 不约束当前 API。

## Frozen Types
- `kernel::ThreadControl`
- `kernel::ThreadTask<Context, StepFn, Priority>`
- `kernel::ThreadBlockingTask<Context, Handler, Priority, UnblockMask>`
- `kernel::ThreadBlockingControl`
- `kernel::ThreadState<Context>`

## Frozen Behaviors
- `ThreadTask`:
  - `on_start()` resets control (done=false)
  - `finish()` stops future tick handling
  - `terminate` always delivered
- `ThreadBlockingTask`:
  - `block()` suppresses non-sync events
  - `resume()` allows events
  - `UnblockMask` defines which events pass through while blocked (default: sync/init/terminate)

## Frozen Events
- `EventId::init`
- `EventId::tick`
- `EventId::sync`
- `EventId::terminate`

## Demo Reference
- `Draft/Examples/windows/main_m2.cpp`

