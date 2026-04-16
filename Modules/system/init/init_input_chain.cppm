module;

export module charm.system.init_input;

import init.node;
import init.plan;
import input.service;
import input.service.node;
import input.router;
import input.router.node;
import input.pump;
import hal_input;
import charm.system.clock;
import kernel.eda;
import util.core;

export namespace charm::system {
    struct InputInitCfg {
        input::ServiceCfg service{};
        charm::system::ClockTick period_ms{16};
        util::usize budget{8};
    };

    struct InputInitCaps {
        const char* service_cap{"input.service"};
        const char* router_cap{"input.router"};
        const char* pump_cap{"input.pump"};
        const char* clock_cap{"system.clock"};
        const char* eda_cap{"kernel.eda"};
    };

    template <typename RegistryT>
    struct InputInitChain {
        input::Router router;
        input::InputService service;
        input::RouterBinding router_binding;
        input::ServiceBinding service_binding;
        input::InputPumpBinding pump_binding;

        InputInitChain(const hal::RawInputSource& source,
                       charm::system::Clock& clock,
                       input::InputPumpTask& pump,
                       input::ScheduleFn schedule_fn,
                       void* schedule_ctx,
                       input::PostFn post_more_fn,
                       void* post_ctx,
                       kernel::TaskId pump_id,
                       input::SinkFn sink_fn = nullptr,
                       void* sink_ctx = nullptr,
                       InputInitCfg cfg = {},
                       InputInitCaps caps = {},
                       init::Phase phase = init::Phase::core,
                       util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : router(),
              service(source, clock, cfg.service),
              router_binding(router, caps.router_cap, phase, runlevel_mask),
              service_binding(service, clock, caps.service_cap, caps.clock_cap, phase, runlevel_mask),
              pump_binding(pump,
                           service,
                           clock,
                           schedule_fn,
                           schedule_ctx,
                           post_more_fn,
                           post_ctx,
                           pump_id,
                           sink_fn ? sink_fn : &input::Router::sink_trampoline,
                           sink_fn ? sink_ctx : static_cast<void*>(&router),
                           cfg.period_ms,
                           cfg.budget,
                           caps.pump_cap,
                           caps.eda_cap,
                           caps.service_cap,
                           caps.clock_cap,
                           caps.router_cap,
                           phase,
                           runlevel_mask) {
        }

        constexpr auto plan() const noexcept {
            return init::compose(
                init::as_plan(service_binding),
                init::as_plan(router_binding),
                init::as_plan(pump_binding));
        }

        input::Router& router_ref() noexcept { return router; }

    };
}
