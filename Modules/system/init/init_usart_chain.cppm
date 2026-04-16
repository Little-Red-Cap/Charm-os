module;

export module charm.system.init_usart;

import init.node;
import init.plan;
import io.reactor;
import platform.irq;
import hal_uart;
import hal_uart.node;
import driver.usart_channel;
import util.core;

export namespace charm::system {
    template <typename RegistryT, util::usize RxCap, util::usize TxCap>
    struct UsartInitChain {
        platform::IrqBinding irq{};
        hal::UartBinding uart_binding;
        driver::usart::ChannelBinding<RegistryT, RxCap, TxCap> channel_binding;

        UsartInitChain(RegistryT& registry,
                       io::Reactor& reactor,
                       hal::UartIoHandle uart,
                       const hal::UartConfig& cfg,
                       const char* uart_cap = "io.uart1",
                       const char* hal_cap = "hal.uart1") noexcept
            : irq(),
              uart_binding(uart, cfg, hal_cap, "platform.irq"),
              channel_binding(registry, reactor, uart, uart_cap, hal_cap) {
        }

        constexpr auto plan() const noexcept {
            return init::compose(
                init::as_plan(irq),
                init::as_plan(uart_binding),
                init::as_plan(channel_binding));
        }

    };
}
