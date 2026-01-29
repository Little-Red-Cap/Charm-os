module;

export module kernel.task_state;

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
        stopped
    };
}
