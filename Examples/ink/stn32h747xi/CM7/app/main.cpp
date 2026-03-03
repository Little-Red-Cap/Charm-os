#include <cstddef>
#include <cstdint>
#include <utility>

/*
LED
    StateLED-> PI15

UART
    PA10     ------> USART1_RX
    PA9     ------> USART1_TX

OLED
    PB10     ------> I2C2_SCL
    PB11     ------> I2C2_SDA

Encoder
    PI7     ------> Key
    PI6     ------> TIM8_CH2
    PI5     ------> TIM8_CH1

Debug
    PA14 (JTCK/SWCLK)   ------> DEBUG_JTCK-SWCLK
    PA13 (JTMS/SWDIO)   ------> DEBUG_JTMS-SWDIO
 */

import charm.system.bringup;
import charm.system.reactor_pump;
import driver.usart_channel;
import io.channel;
import io.registry;
import kernel.capabilities;
import kernel.config;
import kernel.eda;
import kernel.evt;
import kernel.scheduler;
import platform.board.stn32h747xi;
import util.core;
import util.error;

extern "C" {
    void HAL_Init(void);
    void SystemClock_Config(void);
    void MX_GPIO_Init(void);
    void MX_DMA_Init(void);
    void MX_I2C2_Init(void);
    void MX_TIM8_Init(void);
    void Error_Handler(void);
}

namespace {
    constexpr util::usize kRxCap = 64;
    constexpr util::usize kTxCap = 64;
    driver::usart::ChannelAdapter<kRxCap, kTxCap>* g_uart_adapter = nullptr;

    struct PumpConfig : kernel::KernelConfig {
        static constexpr std::size_t priority_levels = 1;
        static constexpr std::size_t evtq_capacity = 8;
    };
}

extern "C" void USART1_IRQHandler(void) {
    if (g_uart_adapter) {
        g_uart_adapter->on_irq();
    }
}

int main() {
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_I2C2_Init();
    MX_TIM8_Init();

    auto caps = platform::board::stn32h747xi::make_board_caps();
    using PumpTask = charm::system::ReactorPumpTask;
    using Registry = kernel::TaskRegistry<PumpTask>;

    using PumpCaps = kernel::NoopCapabilities;

    Registry registry{};
    PumpCaps pump_caps{};
    auto created = kernel::make_scheduler<PumpConfig>(registry, pump_caps);
    auto running = kernel::start(std::move(created));
    const auto pump_id = Registry::id_of<PumpTask>();
    auto& pump = registry.get<PumpTask>();

    using SchedulerT = decltype(running);
    auto post_fn = static_cast<charm::system::PostFn>(
        &charm::system::scheduler_post<SchedulerT>
    );
    charm::system::BringupMinimal<8, 16, 8, kRxCap, kTxCap> bringup{
        caps.uart1,
        pump,
        post_fn,
        &running,
        pump_id,
        8
    };

    auto r = bringup.start();
    if (!r) {
        Error_Handler();
    }

    auto* ch = bringup.registry().open_channel("io.uart1");
    if (!ch) {
        Error_Handler();
    }
    g_uart_adapter = static_cast<driver::usart::ChannelAdapter<kRxCap, kTxCap>*>(ch->ctx);

    io::EndpointDesc console_desc{
        "io.console0",
        io::cap_id("io.console0"),
        io::EndpointKind::channel,
        io::EndpointCaps::duplex
    };
    auto& reg = bringup.registry();
    auto* console_ep = reg.find_channel("io.console0");
    auto r_console = console_ep
        ? reg.replace_channel(console_desc, *ch, &bringup.reactor())
        : reg.register_channel(console_desc, *ch, &bringup.reactor());
    if (!r_console) {
        Error_Handler();
    }

    const char msg[] = "bringup ok\n";
    auto wr = ch->write(io::ByteView{
        reinterpret_cast<const util::u8*>(msg),
        sizeof(msg) - 1
    });
    if (!wr && wr.error() != util::Errc::would_block) {
        Error_Handler();
    }

    while (true) {
        (void)running.run_once();
    }
}
