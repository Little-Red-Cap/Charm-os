module;

#include <cstddef>

export module charm.system.reactor_pump;

import io.reactor;
import kernel.eda;
import kernel.evt;
import util.core;

export namespace charm::system {
    using PostFn = bool (*)(void* ctx, kernel::TaskId task, kernel::Event evt) noexcept;

    struct ReactorPumpTask {
        static constexpr kernel::Priority priority{0};
        static constexpr kernel::EventMask mask = kernel::event_mask(kernel::EventId::reactor_drain);

        io::Reactor* reactor{nullptr};
        PostFn post{nullptr};
        void* post_ctx{nullptr};
        kernel::TaskId self{};
        util::usize budget{8};

        void bind(io::Reactor& reactor_in,
                  PostFn post_fn,
                  void* ctx,
                  kernel::TaskId task,
                  util::usize budget_in = 8) noexcept {
            reactor = &reactor_in;
            post = post_fn;
            post_ctx = ctx;
            self = task;
            budget = (budget_in == 0) ? 1 : budget_in;
        }

        void on_event(kernel::Event evt) noexcept {
            if (evt.id != kernel::EventId::reactor_drain) {
                return;
            }
            if (!reactor || !post) {
                return;
            }
            const bool more = reactor->drain(budget);
            if (more) {
                (void)post(post_ctx, self, kernel::make_event(kernel::EventId::reactor_drain));
            }
        }
    };

    struct ReactorWaker {
        ReactorPumpTask* pump{nullptr};

        static void wake(void* ctx) noexcept {
            auto* self = static_cast<ReactorWaker*>(ctx);
            if (!self || !self->pump) {
                return;
            }
            auto* pump = self->pump;
            if (!pump->post) {
                return;
            }
            (void)pump->post(pump->post_ctx, pump->self, kernel::make_event(kernel::EventId::reactor_drain));
        }
    };

    template <typename Scheduler>
    inline bool scheduler_post(void* ctx, kernel::TaskId task, kernel::Event evt) noexcept {
        auto* scheduler = static_cast<Scheduler*>(ctx);
        if (!scheduler) {
            return false;
        }
        return scheduler->post(task, evt);
    }
}
