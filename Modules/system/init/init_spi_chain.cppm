module;

export module charm.system.init_spi;

import init.node;
import init.graph;
import init.materialize;
import init.plan;
import hal_spi;
import hal_spi.node;
import util.core;
import util.error;

export namespace charm::system {
    struct SpiInitChain {
        hal::SpiBinding spi_binding;

        SpiInitChain(hal::SpiIoHandle handle,
                     const hal::SpiConfig& cfg,
                     const char* hal_cap = "hal.spi1",
                     const char* irq_cap = "platform.irq") noexcept
            : spi_binding(handle, cfg, hal_cap, irq_cap) {
        }

        constexpr auto plan() const noexcept {
            return init::as_plan(spi_binding);
        }

        template <typename Fn>
        constexpr void for_each_legacy_node(Fn&& fn) const noexcept {
            fn(spi_binding.node);
        }

        template <util::usize MaxNodes, util::usize MaxCaps>
        util::Result<void> build(init::Graph<MaxNodes, MaxCaps>& graph,
                                 util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all),
                                 init::Phase max_phase = init::Phase::app) noexcept {
            return init::build_graph(
                graph,
                plan().runlevel(runlevel_mask).phase_limit(max_phase));
        }
    };
}
