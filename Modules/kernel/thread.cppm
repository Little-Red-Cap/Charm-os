module;

export module kernel.thread;

import kernel.evt;
import kernel.eda;
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
        Context context{};
        ThreadControl control{};

        void on_start() noexcept {
            control.reset();
        }

        void on_stop() noexcept {
            control.finish();
        }

        void on_event(Event evt) {
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
