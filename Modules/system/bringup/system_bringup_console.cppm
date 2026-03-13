module;

#include <array>
#include <span>

export module charm.system.bringup.console;

import charm.system.clock;
import charm.system.init_usart;
import init.graph;
import init.node;
import io.channel;
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
    class BringupConsole {
    public:
        explicit BringupConsole(const platform::board::ConsoleCaps& caps) noexcept
            : caps_(caps),
              registry_(),
              reactor_(),
              clock_(caps.clock.ctx, ClockOps{caps.clock.now_ms, caps.clock.now_us}),
              clock_binding_(clock_),
              registry_binding_(registry_),
              reactor_binding_(reactor_),
              usart_chain_(registry_, reactor_,
                           caps.uart.handle, caps.uart.config,
                           caps.console_cap, caps.uart.hal_cap) {
            nodes_[0] = &clock_binding_.node;
            nodes_[1] = &registry_binding_.node;
            nodes_[2] = &reactor_binding_.node;
            const auto usart_nodes = usart_chain_.node_span();
            util::usize idx = 3;
            for (util::usize i = 0; i < usart_nodes.size(); ++i) {
                nodes_[idx++] = usart_nodes[i];
            }
            nodes_count_ = idx;
        }

        util::Result<void> start(util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all),
                                 init::Phase max_phase = init::Phase::core) noexcept {
            if (nodes_count_ > MaxNodes) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
            auto r = graph_.build(std::span<const init::Node* const>(nodes_.data(), nodes_count_),
                                  runlevel_mask, max_phase);
            if (!r) return r;
            auto r_start = graph_.start();
            if (!r_start) return r_start;
            return {};
        }

        io::Channel* console_channel() noexcept {
            return registry_.open_channel(caps_.console_cap);
        }

        io::Registry<MaxEndpoints>& registry() noexcept { return registry_; }
        io::Reactor& reactor() noexcept { return reactor_; }
        charm::system::Clock& clock() noexcept { return clock_; }

    private:
        platform::board::ConsoleCaps caps_{};
        io::Registry<MaxEndpoints> registry_{};
        io::Reactor reactor_{};
        charm::system::Clock clock_{};
        charm::system::ClockBinding clock_binding_;
        io::RegistryBinding<io::Registry<MaxEndpoints>> registry_binding_;
        io::ReactorBinding reactor_binding_;
        UsartInitChain<io::Registry<MaxEndpoints>, RxCap, TxCap> usart_chain_;
        std::array<const init::Node*, MaxNodes> nodes_{};
        util::usize nodes_count_{0};
        init::Graph<MaxNodes, MaxCaps> graph_{};
    };
}
