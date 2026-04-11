module;

#include <span>

export module charm.system.bringup.block;

import charm.system.init_core;
import charm.system.clock;
import charm.system.reactor_pump;
import block.registry;
import init.graph;
import init.materialize;
import init.node;
import init.plan;
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
                    host.post_io_ready_fn(),
                    host.post_demand_fn(),
                    host.post_ctx(),
                    host.pump_id(),
                    budget) {}

        util::Result<void> start(util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all),
                                 init::Phase max_phase = init::Phase::app) noexcept {
            return start_plan(init::compose(), runlevel_mask, max_phase);
        }

        [[deprecated("use start_plan(...) instead of passing extra Node spans")]]
        util::Result<void> start(util::u32 runlevel_mask,
                                 init::Phase max_phase,
                                 std::span<const init::Node* const> extra_nodes) noexcept {
            return start_plan(init::compat_nodes(extra_nodes), runlevel_mask, max_phase);
        }

        template <typename ExtraPlan>
        util::Result<void> start_plan(const ExtraPlan& extra_plan,
                                      util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all),
                                      init::Phase max_phase = init::Phase::app) noexcept {
            const auto bringup_plan = init::phase_limit(
                init::runlevel(
                    init::compose(
                        core_.plan(),
                        extra_plan),
                    runlevel_mask),
                max_phase);
            return init::start_graph(graph_, bringup_plan);
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

