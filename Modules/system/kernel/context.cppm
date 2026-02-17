module;

// Optional/experimental module: current execution context helpers.
export module kernel.context;

import kernel.eda;
import kernel.evt;

export namespace kernel {
    struct CurrentContext {
        TaskId task{};
        Event event{};
        bool valid{false};
    };

    inline CurrentContext current_context{};

    inline void set_current(TaskId task, Event event) noexcept {
        current_context.task = task;
        current_context.event = event;
        current_context.valid = true;
    }

    inline void clear_current() noexcept {
        current_context.valid = false;
    }

    [[nodiscard]] inline bool has_current() noexcept {
        return current_context.valid;
    }

    [[nodiscard]] inline TaskId current_task() noexcept {
        return current_context.task;
    }

    [[nodiscard]] inline Event current_event() noexcept {
        return current_context.event;
    }
}
