module;

#include <array>
#include <span>

export module charm.system.init_canopen;

import init.node;
import init.graph;
import canopen.transport_channel.node;
import canopen.sdo;
import canopen.sdo.node;
import canopen.sdo_service;
import canopen.nmt;
import canopen.nmt.node;
import canopen.nmt_service;
import canopen.pump;
import charm.system.clock;
import kernel.eda;
import util.core;
import util.error;

export namespace charm::system {
    struct CanopenInitCaps {
        const char* can_io_cap{"io.can0"};
        const char* registry_cap{"io.registry"};
        const char* transport_cap{"canopen.transport"};
        const char* sdo_cap{"canopen.sdo"};
        const char* nmt_cap{"canopen.nmt"};
        const char* pump_cap{"canopen.pump"};
        const char* clock_cap{"system.clock"};
        const char* eda_cap{"kernel.eda"};
    };

    struct CanopenInitCfg {
        charm::system::ClockTick period_ms{10};
    };

    template <typename RegistryT, typename Scheduler, util::usize RxBufSize = 44>
    struct CanopenInitChain {
        canopen::ChannelTransportBinding<RegistryT, RxBufSize> transport_binding;
        canopen::SdoService sdo_service;
        canopen::NmtService nmt_service;
        canopen::SdoBinding sdo_binding;
        canopen::NmtBinding nmt_binding;
        canopen::CanopenPumpBinding pump_binding;
        std::array<const init::Node*, 4> nodes{};

        CanopenInitChain(RegistryT& registry,
                         canopen::SdoServer& sdo_server,
                         canopen::NmtNode& nmt_node,
                         charm::system::Clock& clock,
                         Scheduler& scheduler,
                         canopen::CanopenPumpTask& pump_task,
                         kernel::TaskId pump_id,
                         canopen::SdoServiceConfig sdo_cfg = {},
                         canopen::NmtServiceConfig nmt_cfg = {},
                         CanopenInitCfg cfg = {},
                         CanopenInitCaps caps = {},
                         init::Phase phase = init::Phase::service,
                         util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : transport_binding(registry, caps.can_io_cap, caps.transport_cap, caps.registry_cap, phase, runlevel_mask),
              sdo_service(sdo_server, transport_binding.transport, sdo_cfg),
              nmt_service(nmt_node, transport_binding.transport, nmt_cfg),
              sdo_binding(sdo_service, caps.sdo_cap, caps.transport_cap, phase, runlevel_mask),
              nmt_binding(nmt_service, caps.nmt_cap, caps.transport_cap, phase, runlevel_mask),
              pump_binding(pump_task,
                           clock,
                           &canopen::scheduler_schedule_at<Scheduler>,
                           &scheduler,
                           pump_id,
                           &sdo_service,
                           &nmt_service,
                           cfg.period_ms,
                           caps.pump_cap,
                           caps.eda_cap,
                           caps.sdo_cap,
                           caps.nmt_cap,
                           caps.clock_cap,
                           phase,
                           runlevel_mask) {
            nodes = {
                &transport_binding.node,
                &sdo_binding.node,
                &nmt_binding.node,
                &pump_binding.node
            };
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
