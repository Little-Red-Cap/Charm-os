module;

#include <cstddef>
#include <span>
#include <array>
#include <string_view>

export module charm.system.reactor_pump;

import init.node;
import io.reactor;
import kernel.eda;
import kernel.evt;
import kernel.ssu;
import util.core;
import util.error;

export namespace charm::system {
    using PostFn = bool (*)(void* ctx, kernel::TaskId task, kernel::Event evt) noexcept;

    struct ReactorPumpTask {
        static constexpr kernel::Priority priority{0};
        static constexpr kernel::EventMask mask = kernel::event_mask(kernel::EventId::reactor_drain);

        io::Reactor* reactor{nullptr};
        PostFn post{nullptr};
        PostFn post_more{nullptr};
        void* post_ctx{nullptr};
        kernel::TaskId self{};
        util::usize budget{8};

        static consteval kernel::ssu::Meta ssu_meta() noexcept {
            return kernel::ssu::Meta{
                .domain = kernel::ssu::ExecutionDomain::task_only,
                .trigger = kernel::ssu::TriggerKind::io_ready,
                .budget = kernel::ssu::BudgetKind::budgeted,
                .blocking = kernel::ssu::BlockingKind::non_blocking,
                .name = "system.reactor_pump",
            };
        }

        void bind(io::Reactor& reactor_in,
                  PostFn post_fn,
                  PostFn post_more_fn,
                  void* ctx,
                  kernel::TaskId task,
                  util::usize budget_in = 8) noexcept {
            reactor = &reactor_in;
            post = post_fn;
            post_more = post_more_fn ? post_more_fn : post_fn;
            post_ctx = ctx;
            self = task;
            budget = (budget_in == 0) ? 1 : budget_in;
        }

        void on_event(kernel::Event evt) noexcept {
            if (evt.id != kernel::EventId::reactor_drain) {
                return;
            }
            if (!reactor || !post || !post_more) {
                return;
            }
            const bool more = reactor->drain(budget);
            if (more) {
                (void)post_more(post_ctx, self, kernel::make_event(kernel::EventId::reactor_drain));
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

    template <typename Scheduler>
    inline bool scheduler_post_io_ready(void* ctx, kernel::TaskId task, kernel::Event evt) noexcept {
        auto* scheduler = static_cast<Scheduler*>(ctx);
        if (!scheduler) {
            return false;
        }
        return scheduler->post_io_ready(task, evt);
    }

    template <typename Scheduler>
    inline bool scheduler_post_demand(void* ctx, kernel::TaskId task, kernel::Event evt) noexcept {
        auto* scheduler = static_cast<Scheduler*>(ctx);
        if (!scheduler) {
            return false;
        }
        return scheduler->post_demand(task, evt);
    }


    struct ReactorPumpBinding {
        ReactorPumpTask* pump{nullptr};
        io::Reactor* reactor{nullptr};
        PostFn post{nullptr};
        PostFn post_more{nullptr};
        void* post_ctx{nullptr};
        kernel::TaskId self{};
        util::usize budget{8};
        const char* eda_cap_name{"kernel.eda"};
        const char* reactor_cap_name{"io.reactor"};

        static consteval kernel::ssu::Meta ssu_meta() noexcept {
            return kernel::ssu::Meta{
                .domain = kernel::ssu::ExecutionDomain::task_only,
                .trigger = kernel::ssu::TriggerKind::io_ready,
                .budget = kernel::ssu::BudgetKind::budgeted,
                .blocking = kernel::ssu::BlockingKind::non_blocking,
                .name = "system.reactor_pump",
            };
        }
        ReactorWaker waker{};
        std::array<init::CapId, 1> provides{};
        std::array<init::CapId, 2> requires_caps{};
        init::Node node{};

        ReactorPumpBinding(ReactorPumpTask& task,
                           io::Reactor& reactor_in,
                           PostFn post_fn,
                           PostFn post_more_fn,
                           void* post_ctx_in,
                           kernel::TaskId self_id,
                           util::usize budget_in = 8,
                           const char* cap_name = "system.reactor_pump",
                           const char* eda_cap_name = "kernel.eda",
                           const char* reactor_cap_name = "io.reactor",
                           init::Phase phase = init::Phase::core,
                           util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : pump(&task),
              reactor(&reactor_in),
              post(post_fn),
              post_more(post_more_fn ? post_more_fn : post_fn),
              post_ctx(post_ctx_in),
              self(self_id),
              budget((budget_in == 0) ? 1 : budget_in),
              eda_cap_name(eda_cap_name),
              reactor_cap_name(reactor_cap_name),
              waker{&task} {
            provides[0] = init::cap_id(cap_name);
            requires_caps[0] = init::cap_id(eda_cap_name);
            requires_caps[1] = init::cap_id(reactor_cap_name);
            node = init::Node{
                cap_name,
                phase,
                runlevel_mask,
                std::span<const init::CapId>(provides.data(), provides.size()),
                std::span<const init::CapId>(requires_caps.data(), requires_caps.size()),
                &ReactorPumpBinding::init_trampoline,
                nullptr,
                this
            };
        }

        constexpr std::string_view capability_name(init::CapId id) const noexcept {
            if (id == provides[0]) {
                return node.name;
            }
            if (id == requires_caps[0]) {
                return std::string_view{eda_cap_name ? eda_cap_name : ""};
            }
            if (id == requires_caps[1]) {
                return std::string_view{reactor_cap_name ? reactor_cap_name : ""};
            }
            return {};
        }

        static util::Result<void> init_trampoline(void* ctx) noexcept {
            auto* self = static_cast<ReactorPumpBinding*>(ctx);
            if (!self || !self->pump || !self->reactor || !self->post) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            self->pump->bind(*self->reactor, self->post, self->post_more, self->post_ctx, self->self, self->budget);
            self->waker.pump = self->pump;
            self->reactor->set_waker(&ReactorWaker::wake, &self->waker);
            return {};
        }
    };
}



