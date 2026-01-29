module;

export module kernel.thread;

import kernel.evt;
import kernel.eda;
import util.core;

export namespace kernel {
    struct ThreadControl {
        bool done{false};
        void finish() noexcept { done = true; }
    };

    template <typename Context>
    using ThreadStep = void (*)(Context&, ThreadControl&, Event);

    template <typename Context, ThreadStep<Context> StepFn, Priority Prio>
    struct ThreadTask {
        static constexpr Priority priority{Prio};
        Context context{};
        ThreadControl control{};

        void on_event(Event evt) {
            if (control.done) {
                return;
            }
            StepFn(context, control, evt);
        }
    };
}
