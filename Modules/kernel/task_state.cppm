export module kernel.task_state;

export namespace kernel::state {
    struct Created { };
    struct Ready { };
    struct Running { };
    struct Stopped { };
}
