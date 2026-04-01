module;

export module kernel.thread_blocking;

import kernel.evt;
import kernel.eda;
import kernel.ssu;
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

    template <typename Context, ThreadHandler<Context> Handler, Priority Prio,
        EventMask UnblockMask = (event_mask(EventId::sync)
            | event_mask(EventId::init)
            | event_mask(EventId::terminate))>
    struct ThreadBlockingTask {
        static constexpr Priority priority{Prio};
        static constexpr EventMask mask{0xFFFF'FFFFu};
        static constexpr EventMask unblock_mask{UnblockMask};
        Context context{};
        ThreadBlockingControl control{};
        ThreadState<Context> state{};

        static consteval kernel::ssu::Meta ssu_meta() noexcept {
            return {
                .domain = kernel::ssu::ExecutionDomain::task_only,
                .trigger = kernel::ssu::TriggerKind::event,
                .budget = kernel::ssu::BudgetKind::single_step,
                .blocking = kernel::ssu::BlockingKind::may_block,
                .name = "kernel.thread_blocking_task",
            };
        }

        void on_start() noexcept {
            control.resume();
        }

        void on_stop() noexcept {
            control.block();
        }

        void on_event(Event evt) {
            state.ctx = &context;
            state.control = &control;
            // Allow a configurable subset of events while blocked.
            if (control.blocked && (event_mask(evt.id) & unblock_mask) == 0) {
                return;
            }
            Handler(state, evt);
        }
    };
}
