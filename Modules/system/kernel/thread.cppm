module;

export module kernel.thread;

import kernel.evt;
import kernel.eda;
import kernel.ssu;
import util.core;

export namespace kernel {
    struct ThreadControl {
        bool done{false};
        void finish() noexcept { done = true; }
        void reset() noexcept { done = false; }
        [[nodiscard]] bool is_done() const noexcept { return done; }
    };

    template <typename Context>
    using ThreadStep = void (*)(Context&, ThreadControl&, Event);

    template <typename Context, ThreadStep<Context> StepFn, Priority Prio>
    struct ThreadTask {
        static constexpr Priority priority{Prio};
        static constexpr EventMask mask{0xFFFF'FFFFu};
        Context context{};
        ThreadControl control{};

        static consteval kernel::ssu::Meta ssu_meta() noexcept {
            return {
                .domain = kernel::ssu::ExecutionDomain::task_only,
                .trigger = kernel::ssu::TriggerKind::event,
                .budget = kernel::ssu::BudgetKind::single_step,
                .blocking = kernel::ssu::BlockingKind::non_blocking,
                .name = "kernel.thread_task",
            };
        }

        void on_start() noexcept {
            control.reset();
        }

        void on_stop() noexcept {
            control.finish();
        }

        void on_event(Event evt) {
            // terminate is always delivered even when done
            if (control.done && evt.id != EventId::terminate) {
                return;
            }
            if (evt.id == EventId::init) {
                control.reset();
            }
            StepFn(context, control, evt);
        }
    };
}
