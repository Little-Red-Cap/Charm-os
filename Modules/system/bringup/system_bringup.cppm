module;

#include <array>
#include <span>

export module charm.system.bringup;

import charm.system.init_core;
import charm.system.reactor_pump;
import charm.system.init_usart;
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
              util::usize MaxEndpoints,
              util::usize RxCap,
              util::usize TxCap>
    class BringupMinimal {
    public:
        BringupMinimal(const platform::board::UartDesc& uart,
                       ReactorPumpTask& pump_task,
                       PostFn post_fn,
                       void* post_ctx,
                       kernel::TaskId pump_id,
                       util::usize budget = 8) noexcept
            : uart_(uart),
              core_(pump_task, post_fn, post_ctx, pump_id, budget),
              board_(core_.registry, core_.reactor,
                     uart.handle, uart.config, uart.io_cap, uart.hal_cap) {}

        util::Result<void> start(util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all),
                                 init::Phase max_phase = init::Phase::app,
                                 std::span<const init::Node* const> extra_nodes = {}) noexcept {
            const auto core_nodes = core_.node_span();
            const auto board_nodes = board_.node_span();
            const auto total = core_nodes.size() + board_nodes.size() + extra_nodes.size();
            if (total > MaxNodes) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
            std::array<const init::Node*, MaxNodes> nodes{};
            util::usize idx = 0;
            for (auto* node : core_nodes) {
                nodes[idx++] = node;
            }
            for (auto* node : board_nodes) {
                nodes[idx++] = node;
            }
            for (auto* node : extra_nodes) {
                nodes[idx++] = node;
            }
            auto r = graph_.build(std::span<const init::Node* const>(nodes.data(), idx),
                                  runlevel_mask, max_phase);
            if (!r) return r;
            r = graph_.start();
            if (!r) return r;
            if (!core_.registry.open_channel(uart_.io_cap)) {
                return util::unexpected(util::Errc::noent);
            }
            return {};
        }

        init::Graph<MaxNodes, MaxCaps>& graph() noexcept { return graph_; }
        io::Registry<MaxEndpoints>& registry() noexcept { return core_.registry; }
        io::Reactor& reactor() noexcept { return core_.reactor; }

    private:
        platform::board::UartDesc uart_{};
        CoreSystemChain<MaxEndpoints> core_;
        init::Graph<MaxNodes, MaxCaps> graph_{};
        UsartInitChain<io::Registry<MaxEndpoints>, RxCap, TxCap> board_;
    };
}
