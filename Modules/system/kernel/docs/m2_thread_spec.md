# M2 Thread/Blocking Spec (Draft)

## 1. ThreadTask
- `ThreadControl::finish()` stops further tick processing.
- `on_start()` resets control state (if provided by task).
- `init` event is delivered once at startup.
- `tick` drives step function.
- `terminate` is always delivered (even if finished), used for cleanup.

## 2. ThreadBlockingTask
- `block()` suppresses non-sync events.
- `resume()` allows events.
- `sync` event always passes through (used for unblocking).
- `init` always passes through.
- `terminate` always passes through.

## 3. Recommended Usage
- Use ThreadTask for cooperative step-driven logic.
- Use ThreadBlockingTask when a task must wait on sync/IPC.

## 4. Demo
- See `Draft/Examples/windows/main_m2.cpp`

