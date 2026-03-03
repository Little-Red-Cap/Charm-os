module;

#include <array>
#include <span>

export module charm.system.init_usart;

import init.graph;
import init.node;
import io.reactor;
import platform.irq;
import hal_uart;
import hal_uart.node;
import driver.usart_channel;
import util.core;
import util.error;

export namespace charm::system {
    template <typename RegistryT, util::usize RxCap, util::usize TxCap>
    struct UsartInitChain {
        platform::IrqBinding irq{};
        hal::UartBinding uart_binding;
        driver::usart::ChannelBinding<RegistryT, RxCap, TxCap> channel_binding;
        std::array<const init::Node*, 3> nodes{};

        UsartInitChain(RegistryT& registry,
                       io::Reactor& reactor,
                       hal::UartIoHandle uart,
                       const hal::UartConfig& cfg,
                       const char* uart_cap = "io.uart1",
                       const char* hal_cap = "hal.uart1") noexcept
            : irq(),
              uart_binding(uart, cfg, hal_cap, "platform.irq"),
              channel_binding(registry, reactor, uart, uart_cap, hal_cap) {
            nodes = {
                &irq.node,
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
