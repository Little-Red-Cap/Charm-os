module;

#include <array>
#include <span>

export module charm.system.init_i2c;

import init.node;
import init.graph;
import init.materialize;
import init.plan;
import hal_i2c;
import hal_i2c.node;
import util.core;
import util.error;

export namespace charm::system {
    struct I2cInitChain {
        hal::I2cBinding i2c_binding;
        std::array<const init::Node*, 1> nodes{};

        I2cInitChain(hal::I2cIoHandle handle,
                     const hal::I2cConfig& cfg,
                     const char* hal_cap = "hal.i2c1",
                     const char* irq_cap = "platform.irq") noexcept
            : i2c_binding(handle, cfg, hal_cap, irq_cap) {
            nodes = {&i2c_binding.node};
        }

        constexpr auto plan() const noexcept {
            return init::as_plan(i2c_binding);
        }

        template <typename Fn>
        constexpr void for_each_legacy_node(Fn&& fn) const noexcept {
            for (util::usize i = 0; i < nodes.size(); ++i) {
                if (nodes[i]) {
                    fn(*nodes[i]);
                }
            }
        }

        [[deprecated("use plan() or build_graph(...) instead of node_span()")]]
        std::span<const init::Node* const> node_span() const noexcept {
            return std::span<const init::Node* const>(nodes.data(), nodes.size());
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
