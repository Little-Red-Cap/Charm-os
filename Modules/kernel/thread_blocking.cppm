module;

export module kernel.thread_blocking;

import kernel.evt;
import kernel.eda;
import util.type_state;

export namespace kernel {
    struct RunningState { };
    struct BlockedState { };

    template <typename Context>
    struct ThreadState {
        Context* ctx{nullptr};
    };

    template <typename Context>
    using ThreadHandler = void (*)(ThreadState<Context>&, Event);

    template <typename Context, ThreadHandler<Context> Handler, Priority Prio>
    struct ThreadBlockingTask {
        static constexpr Priority priority{Prio};
        ThreadState<Context> state{};

        void on_event(Event evt) {
            Handler(state, evt);
        }
    };
}
