module;

export module kernel.thread_blocking;

import kernel.evt;
import kernel.eda;
import util.type_state;

export namespace kernel {
    struct RunningState { };
    struct BlockedState { };

    struct ThreadBlockingControl {
        bool blocked{false};

        void block() noexcept {
            blocked = true;
        }

        void resume() noexcept {
            blocked = false;
        }

        [[nodiscard]] bool is_blocked() const noexcept {
            return blocked;
        }
    };

    template <typename Context>
    struct ThreadState {
        Context* ctx{nullptr};
        ThreadBlockingControl* control{nullptr};
    };

    template <typename Context>
    using ThreadHandler = void (*)(ThreadState<Context>&, Event);

    template <typename Context, ThreadHandler<Context> Handler, Priority Prio>
    struct ThreadBlockingTask {
        static constexpr Priority priority{Prio};
        Context context{};
        ThreadBlockingControl control{};
        ThreadState<Context> state{};

        void on_start() noexcept {
            control.resume();
        }

        void on_stop() noexcept {
            control.block();
        }

        void on_event(Event evt) {
            state.ctx = &context;
            state.control = &control;
            if (control.blocked && evt.id != EventId::sync && evt.id != EventId::init && evt.id != EventId::terminate) {
                return;
            }
            Handler(state, evt);
        }
    };
}
