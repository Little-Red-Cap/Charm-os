module;

#include <array>
#include <span>

export module charm.system.init_usart;

import init.graph;
import io.registry;
import io.reactor;
import platform.irq;
import hal_uart.node;
import driver.usart_channel;
import util.core;

export namespace charm::system {
    template <util::usize MaxEndpoints, util::usize RxCap, util::usize TxCap>
    struct UsartInitChain {
        using RegistryT = io::Registry<MaxEndpoints>;

        RegistryT registry{};
        io::Reactor reactor{};
        platform::IrqBinding irq{};
        io::RegistryBinding<RegistryT> registry_binding;
        io::ReactorBinding reactor_binding;
        hal::UartBinding uart_binding;
        driver::usart::ChannelBinding<RegistryT, RxCap, TxCap> channel_binding;
        std::array<const init::Node*, 5> nodes{};

        UsartInitChain(hal::UartIoHandle uart,
                       const hal::UartConfig& cfg,
                       const char* uart_cap = "io.uart1",
                       const char* hal_cap = "hal.uart1") noexcept
            : registry(),
              reactor(),
              irq(),
              registry_binding(registry),
              reactor_binding(reactor),
              uart_binding(uart, cfg, hal_cap, "platform.irq"),
              channel_binding(registry, reactor, uart, uart_cap, hal_cap) {
            nodes = {
                &irq.node,
                &registry_binding.node,
                &reactor_binding.node,
                &uart_binding.node,
                &channel_binding.node
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
