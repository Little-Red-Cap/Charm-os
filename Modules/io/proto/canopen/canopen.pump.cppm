module;

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

export module canopen.pump;

import charm.system.clock;
import charm.system.schedule_ref;
import init.binding;
import canopen.sdo_service;
import canopen.nmt_service;
import kernel.eda;
import kernel.evt;
import kernel.poster;
import kernel.ssu;
import util.core;
import util.error;

export namespace canopen {
    using ScheduleFn = charm::system::ScheduleFn;
    using PostFn = kernel::PostFn;

    struct CanopenPumpPorts {
        charm::system::ScheduleRef schedule{};
        kernel::PostRef post_more{};
    };

    struct CanopenPumpTask {
        static constexpr kernel::Priority priority{0};
        static constexpr kernel::EventMask mask =
            kernel::event_mask(kernel::EventId::init) |
            kernel::event_mask(kernel::EventId::canopen_pump);

        SdoService* sdo{nullptr};
        NmtService* nmt{nullptr};
        charm::system::ClockRef clock{};
        ScheduleFn schedule{nullptr};
        void* schedule_ctx{nullptr};
        PostFn post_more{nullptr};
        void* post_ctx{nullptr};
        kernel::TaskId self{};
        charm::system::ClockTick period_ms{10};
        bool started{false};

        static consteval kernel::ssu::Meta ssu_meta() noexcept {
            return kernel::ssu::Meta{
                .domain = kernel::ssu::ExecutionDomain::task_only,
                .trigger = kernel::ssu::TriggerKind::timer,
                .budget = kernel::ssu::BudgetKind::single_step,
                .blocking = kernel::ssu::BlockingKind::non_blocking,
                .name = "canopen.pump",
            };
        }

        void bind(SdoService* sdo_in,
                  NmtService* nmt_in,
                  charm::system::Clock& clock_in,
                  ScheduleFn schedule_fn,
                  void* schedule_ctx_in,
                  PostFn post_more_fn,
                  void* post_ctx_in,
                  kernel::TaskId task_id,
                  charm::system::ClockTick period_ms_in = 10) noexcept {
            sdo = sdo_in;
            nmt = nmt_in;
            clock.reset(clock_in);
            schedule = schedule_fn;
            schedule_ctx = schedule_ctx_in;
            post_more = post_more_fn;
            post_ctx = post_ctx_in;
            self = task_id;
            period_ms = (period_ms_in == 0) ? 1 : period_ms_in;
            started = false;
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
            if (evt.id != kernel::EventId::canopen_pump) {
                return;
            }
            if (!clock.valid()) {
                return;
            }
            const auto now_tick = clock.now_ms();
            const auto now_ms = static_cast<std::uint32_t>(now_tick);
            if (nmt) {
                (void)nmt->poll_time(now_ms);
            }
            if (sdo) {
                (void)sdo->poll_time(now_ms);
            }
            const bool more = (nmt && nmt->has_pending()) || (sdo && sdo->has_pending());
            if (more && post_more) {
                (void)post_more(post_ctx, self, kernel::make_event(kernel::EventId::canopen_pump));
                return;
            }
            schedule_next(now_tick);
        }

    private:
        void schedule_next(charm::system::ClockTick now_ms) noexcept {
            if (!schedule) {
                return;
            }
            const auto due = now_ms + period_ms;
            (void)schedule(schedule_ctx, self, kernel::make_event(kernel::EventId::canopen_pump), due);
        }
    };

    template <typename Scheduler>
    [[nodiscard]] inline CanopenPumpPorts pump_ports_from_scheduler(Scheduler& scheduler) noexcept {
        return CanopenPumpPorts{
            .schedule = charm::system::ScheduleRef::raw(&charm::system::scheduler_schedule_at<Scheduler>, &scheduler),
            .post_more = kernel::PostRef::raw(&kernel::scheduler_post_demand<Scheduler>, &scheduler),
        };
    }

    struct CanopenPumpBinding {
        CanopenPumpTask* pump{nullptr};
        SdoService* sdo{nullptr};
        NmtService* nmt{nullptr};
        charm::system::Clock* clock{nullptr};
        CanopenPumpPorts ports{};
        kernel::TaskId self{};
        charm::system::ClockTick period_ms{10};
        const char* eda_cap_name{"kernel.eda"};
        const char* sdo_cap_name{"canopen.sdo"};
        const char* nmt_cap_name{"canopen.nmt"};
        const char* clock_cap_name{"system.clock"};
        std::array<init::CapId, 1> provides{};
        std::array<init::CapId, 4> requires_caps{};
        util::usize requires_count{0};
        init::Node node{};

        CanopenPumpBinding(CanopenPumpTask& task,
                           charm::system::Clock& clock_in,
                           CanopenPumpPorts ports_in,
                           kernel::TaskId task_id,
                           SdoService* sdo_in = nullptr,
                           NmtService* nmt_in = nullptr,
                           charm::system::ClockTick period_ms_in = 10,
                           const char* cap_name = "canopen.pump",
                           const char* eda_cap_name = "kernel.eda",
                           const char* sdo_cap_name = "canopen.sdo",
                           const char* nmt_cap_name = "canopen.nmt",
                           const char* clock_cap_name = "system.clock",
                           init::Phase phase = init::Phase::service,
                           util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : pump(&task),
              sdo(sdo_in),
              nmt(nmt_in),
              clock(&clock_in),
              ports(ports_in),
              self(task_id),
              period_ms((period_ms_in == 0) ? 1 : period_ms_in),
              eda_cap_name(eda_cap_name),
              sdo_cap_name(sdo_cap_name),
              nmt_cap_name(nmt_cap_name),
              clock_cap_name(clock_cap_name) {
            provides = init::capability_ids(cap_name);
            requires_caps[0] = init::cap_id_or_zero(init::capability_name_view(eda_cap_name));
            requires_caps[1] = init::cap_id_or_zero(init::capability_name_view(clock_cap_name));
            requires_count = 2;
            if (sdo) {
                requires_caps[requires_count++] = init::cap_id_or_zero(init::capability_name_view(sdo_cap_name));
            }
            if (nmt) {
                requires_caps[requires_count++] = init::cap_id_or_zero(init::capability_name_view(nmt_cap_name));
            }
            node = init::make_binding_node(init::capability_name_view(cap_name),
                                           phase,
                                           runlevel_mask,
                                           std::span<const init::CapId>(provides.data(), provides.size()),
                                           std::span<const init::CapId>(requires_caps.data(), requires_count),
                                           &CanopenPumpBinding::init_trampoline,
                                           nullptr,
                                           this);
        }

        constexpr std::string_view capability_name(init::CapId id) const noexcept {
            const std::array<std::string_view, 1> provide_names{node.name};
            std::array<std::string_view, 4> require_names{};
            require_names[0] = init::capability_name_view(eda_cap_name);
            require_names[1] = init::capability_name_view(clock_cap_name);
            util::usize require_name_count = 2;
            if (sdo) {
                require_names[require_name_count++] = init::capability_name_view(sdo_cap_name);
            }
            if (nmt) {
                require_names[require_name_count++] = init::capability_name_view(nmt_cap_name);
            }
            return init::lookup_capability_name(id,
                                                std::span<const init::CapId>(provides.data(), provides.size()),
                                                std::span<const std::string_view>(provide_names.data(), provide_names.size()),
                                                std::span<const init::CapId>(requires_caps.data(), requires_count),
                                                std::span<const std::string_view>(require_names.data(), require_name_count));
        }

        static util::Result<void> init_trampoline(void* ctx) noexcept {
            auto* self = static_cast<CanopenPumpBinding*>(ctx);
            if (!self || !self->pump || !self->clock) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (!self->sdo && !self->nmt) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            self->pump->bind(self->sdo,
                             self->nmt,
                             *self->clock,
                             self->ports.schedule.fn(),
                             self->ports.schedule.ctx(),
                             self->ports.post_more.fn(),
                             self->ports.post_more.ctx(),
                             self->self,
                             self->period_ms);
            self->pump->start();
            return {};
        }
    };
}
