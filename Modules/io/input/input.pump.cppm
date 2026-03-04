module;

#include <array>
#include <cstdint>
#include <span>

export module input.pump;

import charm.system.clock;
import init.node;
import input.raw_event;
import input.service;
import kernel.eda;
import kernel.evt;
import util.core;
import util.error;

export namespace input {
    using ScheduleFn = bool (*)(void* ctx,
                                kernel::TaskId task,
                                kernel::Event evt,
                                charm::system::ClockTick due) noexcept;

    using SinkFn = bool (*)(void* ctx, const RawInputEvent& ev) noexcept;

    struct InputPumpTask {
        static constexpr kernel::Priority priority{0};
        static constexpr kernel::EventMask mask =
            kernel::event_mask(kernel::EventId::init) |
            kernel::event_mask(kernel::EventId::input_pump);

        InputService* service{nullptr};
        charm::system::ClockRef clock{};
        SinkFn sink{nullptr};
        void* sink_ctx{nullptr};
        ScheduleFn schedule{nullptr};
        void* schedule_ctx{nullptr};
        kernel::TaskId self{};
        charm::system::ClockTick period_ms{16};
        util::usize budget{8};
        bool started{false};

        void bind(InputService& service_in,
                  charm::system::Clock& clock_in,
                  SinkFn sink_fn,
                  void* sink_ctx_in,
                  ScheduleFn schedule_fn,
                  void* schedule_ctx_in,
                  kernel::TaskId task_id,
                  charm::system::ClockTick period_ms_in,
                  util::usize budget_in = 8) noexcept {
            service = &service_in;
            clock.reset(clock_in);
            sink = sink_fn;
            sink_ctx = sink_ctx_in;
            schedule = schedule_fn;
            schedule_ctx = schedule_ctx_in;
            self = task_id;
            period_ms = (period_ms_in == 0) ? 1 : period_ms_in;
            budget = (budget_in == 0) ? 1 : budget_in;
            started = false;
        }

        void set_sink(SinkFn fn, void* ctx) noexcept {
            sink = fn;
            sink_ctx = ctx;
        }

        void start() noexcept {
            if (started) return;
            started = true;
            if (!clock.valid()) return;
            const auto now = clock.now_ms();
            schedule_next(now);
        }

        void on_event(kernel::Event evt) noexcept {
            if (evt.id == kernel::EventId::init) {
                start();
                return;
            }
            if (evt.id != kernel::EventId::input_pump) {
                return;
            }
            if (!service || !clock.valid()) {
                return;
            }
            const auto now_tick = clock.now_ms();
            const auto now_ms = static_cast<std::uint32_t>(now_tick);
            pump_once(now_ms);
            schedule_next(now_tick);
        }

    private:
        void schedule_next(charm::system::ClockTick now_ms) noexcept {
            if (!schedule) {
                return;
            }
            const auto due = now_ms + period_ms;
            (void)schedule(schedule_ctx, self, kernel::make_event(kernel::EventId::input_pump), due);
        }

        void pump_once(std::uint32_t now_ms) noexcept {
            if (!service) return;
            for (util::usize i = 0; i < budget; ++i) {
                auto ev = service->poll_raw_at(now_ms);
                if (!ev) {
                    break;
                }
                if (sink && !sink(sink_ctx, *ev)) {
                    break;
                }
            }
        }
    };

    template <typename Scheduler>
    inline bool scheduler_schedule_at(void* ctx,
                                      kernel::TaskId task,
                                      kernel::Event evt,
                                      charm::system::ClockTick due) noexcept {
        auto* scheduler = static_cast<Scheduler*>(ctx);
        if (!scheduler) {
            return false;
        }
        return scheduler->schedule_at(due, task, evt);
    }

    struct InputPumpBinding {
        InputPumpTask* pump{nullptr};
        InputService* service{nullptr};
        charm::system::Clock* clock{nullptr};
        SinkFn sink{nullptr};
        void* sink_ctx{nullptr};
        ScheduleFn schedule{nullptr};
        void* schedule_ctx{nullptr};
        kernel::TaskId self{};
        charm::system::ClockTick period_ms{16};
        util::usize budget{8};
        std::array<init::CapId, 1> provides{};
        std::array<init::CapId, 4> requires_caps{};
        init::Node node{};

        InputPumpBinding(InputPumpTask& task,
                         InputService& service_in,
                         charm::system::Clock& clock_in,
                         ScheduleFn schedule_fn,
                         void* schedule_ctx_in,
                         kernel::TaskId task_id,
                         SinkFn sink_fn = nullptr,
                         void* sink_ctx_in = nullptr,
                         charm::system::ClockTick period_ms_in = 16,
                         util::usize budget_in = 8,
                         const char* cap_name = "input.pump",
                         const char* eda_cap_name = "kernel.eda",
                         const char* service_cap_name = "input.service",
                         const char* clock_cap_name = "system.clock",
                         const char* router_cap_name = "input.router",
                         init::Phase phase = init::Phase::core,
                         util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : pump(&task),
              service(&service_in),
              clock(&clock_in),
              sink(sink_fn),
              sink_ctx(sink_ctx_in),
              schedule(schedule_fn),
              schedule_ctx(schedule_ctx_in),
              self(task_id),
              period_ms((period_ms_in == 0) ? 1 : period_ms_in),
              budget((budget_in == 0) ? 1 : budget_in) {
            provides[0] = init::cap_id(cap_name);
            requires_caps[0] = init::cap_id(eda_cap_name);
            requires_caps[1] = init::cap_id(service_cap_name);
            requires_caps[2] = init::cap_id(clock_cap_name);
            requires_caps[3] = init::cap_id(router_cap_name);
            node = init::Node{
                cap_name,
                phase,
                runlevel_mask,
                std::span<const init::CapId>(provides.data(), provides.size()),
                std::span<const init::CapId>(requires_caps.data(), requires_caps.size()),
                &InputPumpBinding::init_trampoline,
                nullptr,
                this
            };
        }

        static util::Result<void> init_trampoline(void* ctx) noexcept {
            auto* self = static_cast<InputPumpBinding*>(ctx);
            if (!self || !self->pump || !self->service || !self->clock) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            self->pump->bind(*self->service,
                             *self->clock,
                             self->sink,
                             self->sink_ctx,
                             self->schedule,
                             self->schedule_ctx,
                             self->self,
                             self->period_ms,
                             self->budget);
            self->pump->start();
            return {};
        }
    };
}
