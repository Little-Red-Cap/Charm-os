module;

#include <optional>

export module charm.system.bringup.input;

import charm.system.init_core;
import charm.system.init_input;
export import charm.system.bringup.input_support;
import charm.system.clock;
import charm.system.reactor_pump;
import block.registry;
import init.graph;
import init.materialize;
import init.node;
import init.plan;
import io.registry;
import io.reactor;
import input.pump;
import platform.board;
import util.core;
import util.error;

export namespace charm::system {
    template <util::usize MaxNodes,
              util::usize MaxCaps,
              util::usize MaxEndpoints>
    class BringupInput {
    public:
        template <typename Host>
        BringupInput(const platform::board::InputCaps& caps,
                     Host& host,
                     util::usize budget = 8,
                     InputSinkRef sink = {},
                     InputInitCfg cfg = {}) noexcept
            : caps_(caps),
              core_(charm::system::ClockOps{caps.clock.now_ms, caps.clock.now_us},
                    caps.clock.ctx,
                    host.pump(),
                    host.post_io_ready_fn(),
                    host.post_demand_fn(),
                    host.post_ctx(),
                    host.pump_id(),
                    budget) {
            (void)detail::emplace_input_chain_from_host(input_, caps.input, core_.clock, host, sink, cfg);
        }

        util::Result<void> start(util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all),
                                 init::Phase max_phase = init::Phase::app) noexcept {
            return start_plan(init::compose(), runlevel_mask, max_phase);
        }

        constexpr auto plan(util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all),
                            init::Phase max_phase = init::Phase::app) const noexcept {
            return plan(init::compose(), runlevel_mask, max_phase);
        }

        template <typename ExtraPlan>
        constexpr auto plan(const ExtraPlan& extra_plan,
                            util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all),
                            init::Phase max_phase = init::Phase::app) const noexcept {
            return init::phase_limit(
                init::runlevel(
                    init::compose(
                        core_.plan(),
                        init::maybe(input_),
                        extra_plan),
                    runlevel_mask),
                max_phase);
        }

        template <typename ExtraPlan>
        util::Result<void> start_plan(const ExtraPlan& extra_plan,
                                      util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all),
                                      init::Phase max_phase = init::Phase::app) noexcept {
            if (!caps_.input.driver || !input_) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            const auto bringup_plan = plan(extra_plan, runlevel_mask, max_phase);
            return init::start_graph(graph_, bringup_plan);
        }

        init::Graph<MaxNodes, MaxCaps>& graph() noexcept { return graph_; }
        io::Registry<MaxEndpoints>& registry() noexcept { return core_.registry; }
        block::Registry<MaxEndpoints>& block_registry() noexcept { return core_.block_registry; }
        io::Reactor& reactor() noexcept { return core_.reactor; }
        charm::system::Clock& clock() noexcept { return core_.clock; }

    private:
        platform::board::InputCaps caps_{};
        CoreSystemChain<MaxEndpoints> core_;
        std::optional<InputInitChain<io::Registry<MaxEndpoints>>> input_{};
        init::Graph<MaxNodes, MaxCaps> graph_{};
    };
}

