module;

#include <array>
#include <span>

export module charm.system.init_input;

import init.node;
import init.graph;
import input.service;
import input.service.node;
import input.pump;
import hal_input;
import charm.system.clock;
import kernel.eda;
import util.core;
import util.error;

export namespace charm::system {
    struct InputInitCfg {
        input::ServiceCfg service{};
        charm::system::ClockTick period_ms{16};
        util::usize budget{8};
    };

    struct InputInitCaps {
        const char* service_cap{"input.service"};
        const char* pump_cap{"input.pump"};
        const char* clock_cap{"system.clock"};
        const char* eda_cap{"kernel.eda"};
    };

    template <typename RegistryT>
    struct InputInitChain {
        input::InputService service;
        input::ServiceBinding service_binding;
        input::InputPumpBinding pump_binding;
        std::array<const init::Node*, 2> nodes{};

        InputInitChain(const hal::RawInputSource& source,
                       input::InputPumpTask& pump,
                       input::ScheduleFn schedule_fn,
                       void* schedule_ctx,
                       kernel::TaskId pump_id,
                       input::SinkFn sink_fn = nullptr,
                       void* sink_ctx = nullptr,
                       InputInitCfg cfg = {},
                       InputInitCaps caps = {},
                       init::Phase phase = init::Phase::core,
                       util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : service(source, cfg.service),
              service_binding(service, caps.service_cap, caps.clock_cap, phase, runlevel_mask),
              pump_binding(pump,
                           service,
                           schedule_fn,
                           schedule_ctx,
                           pump_id,
                           sink_fn,
                           sink_ctx,
                           cfg.period_ms,
                           cfg.budget,
                           caps.pump_cap,
                           caps.eda_cap,
                           caps.service_cap,
                           caps.clock_cap,
                           phase,
                           runlevel_mask) {
            nodes = {&service_binding.node, &pump_binding.node};
        }

        std::span<const init::Node* const> node_span() const noexcept {
            return std::span<const init::Node* const>(nodes.data(), nodes.size());
        }

        template <util::usize MaxNodes, util::usize MaxCaps>
        util::Result<void> build(init::Graph<MaxNodes, MaxCaps>& graph,
                                 util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all),
                                 init::Phase max_phase = init::Phase::app) noexcept {
            return graph.build(node_span(), runlevel_mask, max_phase);
        }
    };
}
