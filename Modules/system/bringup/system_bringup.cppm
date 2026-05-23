module;

#include <optional>
#include <string_view>

export module charm.system.bringup;

import charm.system.init_core;
import charm.system.init_block;
import charm.system.init_input;
import charm.system.init_i2c;
import charm.system.init_spi;
export import charm.system.bringup.input_support;
import charm.system.reactor_pump;
import charm.system.init_usart;
import charm.system.clock;
import block.registry;
import init.graph;
import init.materialize;
import init.node;
import init.plan;
import io.registry;
import io.reactor;
import io.channel;
import io.channel.node;
import input.pump;
import input.raw_sink;
import kernel.eda;
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
        template <typename Host>
        BringupMinimal(const platform::board::BoardCaps& caps,
                       Host& host,
                       util::usize budget = 8,
                       input::RawSinkRef sink = {},
                       InputInitCfg cfg = {}) noexcept
            : uart_(caps.uart1),
              console_cap_(caps.console_cap),
              core_(charm::system::ClockOps{caps.clock.now_ms, caps.clock.now_us},
                    caps.clock.ctx,
                    host.pump(),
                    host.post_io_ready_fn(),
                    host.post_demand_fn(),
                    host.post_ctx(),
                    host.pump_id(),
                    budget),
              board_(core_.registry, core_.reactor,
                     caps.uart1.handle, caps.uart1.config, caps.uart1.io_cap, caps.uart1.hal_cap),
              input_desc_(caps.input),
              spi_desc_(caps.spi1),
              i2c_desc_(caps.i2c1),
              can_desc_(caps.can0),
              sdmmc_desc_(caps.sdmmc0),
              flash_desc_(caps.flash0) {
            emplace_input_from_host(caps.input, host, sink, cfg);
            emplace_optional_peripherals();
        }

        BringupMinimal(const platform::board::UartDesc& uart,
                       const platform::board::ClockDesc& clock_desc,
                       const platform::board::InputDesc& input_desc,
                       const platform::board::SpiDesc& spi_desc,
                       const platform::board::I2cDesc& i2c_desc,
                       const platform::board::CanDesc& can_desc,
                       const platform::board::SdmmcDesc& sdmmc_desc,
                       const platform::board::SpiFlashDesc& flash_desc,
                       ReactorPumpTask& pump_task,
                       PostFn post_fn,
                       PostFn post_more_fn,
                       void* post_ctx,
                       kernel::TaskId pump_id,
                       util::usize budget = 8,
                       const char* console_cap = "io.console0") noexcept
            : uart_(uart),
              console_cap_(console_cap),
              core_(charm::system::ClockOps{clock_desc.now_ms, clock_desc.now_us},
                    clock_desc.ctx,
                    pump_task, post_fn, post_more_fn, post_ctx, pump_id, budget),
              board_(core_.registry, core_.reactor,
                     uart.handle, uart.config, uart.io_cap, uart.hal_cap),
              input_desc_(input_desc),
              spi_desc_(spi_desc),
              i2c_desc_(i2c_desc),
              can_desc_(can_desc),
              sdmmc_desc_(sdmmc_desc),
              flash_desc_(flash_desc) {
            input_required_ = (input_desc_.driver != nullptr);
            emplace_optional_peripherals();
        }

        BringupMinimal(const platform::board::UartDesc& uart,
                       const platform::board::ClockDesc& clock_desc,
                       const platform::board::InputDesc& input_desc,
                       const platform::board::SpiDesc& spi_desc,
                       const platform::board::I2cDesc& i2c_desc,
                       const platform::board::CanDesc& can_desc,
                       const platform::board::SdmmcDesc& sdmmc_desc,
                       const platform::board::SpiFlashDesc& flash_desc,
                       ReactorPumpTask& pump_task,
                       PostFn post_fn,
                       void* post_ctx,
                       kernel::TaskId pump_id,
                       util::usize budget = 8,
                       const char* console_cap = "io.console0") noexcept
            : BringupMinimal(uart,
                             clock_desc,
                             input_desc,
                             spi_desc,
                             i2c_desc,
                             can_desc,
                             sdmmc_desc,
                             flash_desc,
                             pump_task,
                             post_fn,
                             post_fn,
                             post_ctx,
                             pump_id,
                             budget,
                             console_cap) {}

        BringupMinimal(const platform::board::UartDesc& uart,
                       const platform::board::ClockDesc& clock_desc,
                       const platform::board::InputDesc& input_desc,
                       const platform::board::SpiDesc& spi_desc,
                       const platform::board::I2cDesc& i2c_desc,
                       const platform::board::CanDesc& can_desc,
                       ReactorPumpTask& pump_task,
                       PostFn post_fn,
                       PostFn post_more_fn,
                       void* post_ctx,
                       kernel::TaskId pump_id,
                       util::usize budget = 8,
                       const char* console_cap = "io.console0") noexcept
            : BringupMinimal(uart,
                             clock_desc,
                             input_desc,
                             spi_desc,
                             i2c_desc,
                             can_desc,
                             platform::board::SdmmcDesc{},
                             platform::board::SpiFlashDesc{},
                             pump_task,
                             post_fn,
                             post_more_fn,
                             post_ctx,
                             pump_id,
                             budget,
                             console_cap) {}

        BringupMinimal(const platform::board::UartDesc& uart,
                       const platform::board::ClockDesc& clock_desc,
                       const platform::board::InputDesc& input_desc,
                       const platform::board::SpiDesc& spi_desc,
                       const platform::board::I2cDesc& i2c_desc,
                       const platform::board::CanDesc& can_desc,
                       ReactorPumpTask& pump_task,
                       PostFn post_fn,
                       void* post_ctx,
                       kernel::TaskId pump_id,
                       util::usize budget = 8,
                       const char* console_cap = "io.console0") noexcept
            : BringupMinimal(uart,
                             clock_desc,
                             input_desc,
                             spi_desc,
                             i2c_desc,
                             can_desc,
                             platform::board::SdmmcDesc{},
                             platform::board::SpiFlashDesc{},
                             pump_task,
                             post_fn,
                             post_fn,
                             post_ctx,
                             pump_id,
                             budget,
                             console_cap) {}

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
                        board_.plan(),
                        init::maybe(console_channel_),
                        init::maybe(spi_),
                        init::maybe(i2c_),
                        init::maybe(sdmmc_),
                        init::maybe(flash_),
                        init::maybe(input_),
                        init::maybe(can_channel_),
                        extra_plan),
                    runlevel_mask),
                max_phase);
        }

        template <typename ExtraPlan>
        util::Result<void> start_plan(const ExtraPlan& extra_plan,
                                      util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all),
                                      init::Phase max_phase = init::Phase::app) noexcept {
            if (input_required_ && !input_) {
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
        platform::board::UartDesc uart_{};
        const char* console_cap_{"io.console0"};
        CoreSystemChain<MaxEndpoints> core_;
        UsartInitChain<io::Registry<MaxEndpoints>, RxCap, TxCap> board_;
        platform::board::InputDesc input_desc_{};
        init::Graph<MaxNodes, MaxCaps> graph_{};
        std::optional<InputInitChain<io::Registry<MaxEndpoints>>> input_{};
        bool input_required_{false};
        platform::board::SpiDesc spi_desc_{};
        platform::board::I2cDesc i2c_desc_{};
        std::optional<SpiInitChain> spi_{};
        std::optional<I2cInitChain> i2c_{};
        platform::board::CanDesc can_desc_{};
        std::optional<io::ChannelBinding<io::Registry<MaxEndpoints>>> can_channel_{};
        std::optional<io::ChannelAliasBinding<io::Registry<MaxEndpoints>>> console_channel_{};
        platform::board::SdmmcDesc sdmmc_desc_{};
        platform::board::SpiFlashDesc flash_desc_{};
        std::optional<SdmmcInitChain<block::Registry<MaxEndpoints>>> sdmmc_{};
        std::optional<SpiFlashInitChain<block::Registry<MaxEndpoints>>> flash_{};

        bool needs_console_alias() const noexcept {
            return console_cap_ && console_cap_[0] != '\0' &&
                   std::string_view{uart_.io_cap ? uart_.io_cap : ""}.compare(std::string_view{console_cap_}) != 0;
        }

        template <typename Host>
        void emplace_input_from_host(const platform::board::InputDesc& desc,
                                     Host& host,
                                     input::RawSinkRef sink,
                                     InputInitCfg cfg) noexcept {
            input_required_ = (desc.driver != nullptr);
            (void)detail::emplace_input_chain_from_host(input_, desc, core_.clock, host, sink, cfg);
        }

        void emplace_optional_peripherals() noexcept {
            if (spi_desc_.handle.ops) {
                spi_.emplace(spi_desc_.handle, spi_desc_.config, spi_desc_.hal_cap);
            }
            if (i2c_desc_.handle.ops) {
                i2c_.emplace(i2c_desc_.handle, i2c_desc_.config, i2c_desc_.hal_cap);
            }
            if (sdmmc_desc_.handle.ops) {
                sdmmc_.emplace(core_.block_registry,
                               sdmmc_desc_.handle,
                               sdmmc_desc_.config,
                               sdmmc_desc_.block_cap,
                               sdmmc_desc_.hal_cap);
            }
            if (flash_desc_.handle.ops) {
                flash_.emplace(core_.block_registry,
                               flash_desc_.handle,
                               flash_desc_.config,
                               flash_desc_.block_cap,
                               flash_desc_.hal_cap);
            }
            if (can_desc_.channel) {
                io::EndpointDesc desc{
                    can_desc_.io_cap,
                    io::cap_id(can_desc_.io_cap),
                    io::EndpointKind::channel,
                    io::EndpointCaps::duplex
                };
                can_channel_.emplace(core_.registry, *can_desc_.channel, desc);
            }
            if (needs_console_alias()) {
                console_channel_.emplace(core_.registry, core_.reactor, uart_.io_cap, console_cap_);
            }
        }
    };

}


