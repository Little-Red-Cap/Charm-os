module;

#include <array>
#include <span>

export module charm.system.bringup.block;

import charm.system.init_core;
import charm.system.clock;
import charm.system.reactor_pump;
import block.registry;
import init.graph;
import init.node;
import io.registry;
import io.reactor;
import platform.board;
import util.core;
import util.error;

export namespace charm::system {
    template <util::usize MaxNodes,
              util::usize MaxCaps,
              util::usize MaxEndpoints>
    class BringupBlock {
    public:
        template <typename Host>
        BringupBlock(const platform::board::BlockCaps& caps,
                     Host& host,
                     util::usize budget = 8) noexcept
            : caps_(caps),
              core_(charm::system::ClockOps{caps.clock.now_ms, caps.clock.now_us},
                    caps.clock.ctx,
                    host.pump(),
                    host.post_fn(),
                    host.post_ctx(),
                    host.pump_id(),
                    budget) {}

        util::Result<void> start(util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all),
                                 init::Phase max_phase = init::Phase::app,
                                 std::span<const init::Node* const> extra_nodes = {}) noexcept {
            const auto core_nodes = core_.node_span();
            const auto total = core_nodes.size() + extra_nodes.size();
            if (total > MaxNodes) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
            std::array<const init::Node*, MaxNodes> nodes{};
            util::usize idx = 0;
            for (util::usize i = 0; i < core_nodes.size(); ++i) {
                nodes[idx++] = core_nodes[i];
            }
            for (util::usize i = 0; i < extra_nodes.size(); ++i) {
                nodes[idx++] = extra_nodes[i];
            }
            auto r = graph_.build(std::span<const init::Node* const>(nodes.data(), idx),
                                  runlevel_mask, max_phase);
            if (!r) return r;
            return graph_.start();
        }

        init::Graph<MaxNodes, MaxCaps>& graph() noexcept { return graph_; }
        io::Registry<MaxEndpoints>& registry() noexcept { return core_.registry; }
        block::Registry<MaxEndpoints>& block_registry() noexcept { return core_.block_registry; }
        io::Reactor& reactor() noexcept { return core_.reactor; }
        charm::system::Clock& clock() noexcept { return core_.clock; }

    private:
        platform::board::BlockCaps caps_{};
        CoreSystemChain<MaxEndpoints> core_;
        init::Graph<MaxNodes, MaxCaps> graph_{};
    };
}
