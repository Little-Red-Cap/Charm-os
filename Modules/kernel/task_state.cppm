module;

export module kernel.task_state;

// Optional/experimental module: task state tracking.
export namespace kernel::state {
    struct Created { };
    struct Ready { };
    struct Running { };
    struct Stopped { };
}

export namespace kernel {
    enum class TaskState : unsigned char {
        ready,
        running,
        stopped,
        terminated
    };
}
