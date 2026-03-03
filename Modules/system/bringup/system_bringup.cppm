module;

export module charm.system.bringup;

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
        explicit BringupMinimal(const platform::board::UartDesc& uart) noexcept
            : uart_(uart),
              chain_(uart.handle, uart.config, uart.io_cap, uart.hal_cap) {}

        util::Result<void> start(util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all),
                                 init::Phase max_phase = init::Phase::app) noexcept {
            auto r = chain_.build(graph_, runlevel_mask, max_phase);
            if (!r) return r;
            r = graph_.start();
            if (!r) return r;
            if (!chain_.registry.open_channel(uart_.io_cap)) {
                return util::unexpected(util::Errc::noent);
            }
            return {};
        }

        init::Graph<MaxNodes, MaxCaps>& graph() noexcept { return graph_; }
        io::Registry<MaxEndpoints>& registry() noexcept { return chain_.registry; }
        io::Reactor& reactor() noexcept { return chain_.reactor; }

    private:
        platform::board::UartDesc uart_{};
        init::Graph<MaxNodes, MaxCaps> graph_{};
        UsartInitChain<MaxEndpoints, RxCap, TxCap> chain_;
    };
}
