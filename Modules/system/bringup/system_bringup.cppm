module;

#include <array>
#include <optional>
#include <span>

export module charm.system.bringup;

import charm.system.init_core;
import charm.system.init_block;
import charm.system.init_input;
import charm.system.init_i2c;
import charm.system.init_spi;
import charm.system.reactor_pump;
import charm.system.init_usart;
import charm.system.clock;
import block.registry;
import hal_input;
import init.graph;
import init.materialize;
import init.node;
import init.plan;
import io.registry;
import io.reactor;
import io.channel;
import io.channel.node;
import input.pump;
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
        struct InputBringupDesc {
            const platform::board::InputDesc* desc{nullptr};
            input::InputPumpTask* pump{nullptr};
            input::ScheduleFn schedule{nullptr};
            void* schedule_ctx{nullptr};
            input::PostFn post_more{nullptr};
            void* post_ctx{nullptr};
            kernel::TaskId pump_id{};
            input::SinkFn sink{nullptr};
            void* sink_ctx{nullptr};
            InputInitCfg cfg{};
        };

        static InputBringupDesc make_input_desc(const platform::board::InputDesc& desc,
                                                input::InputPumpTask& pump,
                                                input::ScheduleFn schedule,
                                                void* schedule_ctx,
                                                input::PostFn post_more,
                                                void* post_ctx,
                                                kernel::TaskId pump_id,
                                                input::SinkFn sink = nullptr,
                                                void* sink_ctx = nullptr,
                                                InputInitCfg cfg = {}) noexcept {
            InputBringupDesc out{};
            out.desc = &desc;
            out.pump = &pump;
            out.schedule = schedule;
            out.schedule_ctx = schedule_ctx;
            out.post_more = post_more;
            out.post_ctx = post_ctx;
            out.pump_id = pump_id;
            out.sink = sink;
            out.sink_ctx = sink_ctx;
            out.cfg = cfg;
            return out;
        }

        template <typename Host>
        static InputBringupDesc make_input_desc(const platform::board::InputDesc& desc,
                                                Host& host,
                                                input::SinkFn sink = nullptr,
                                                void* sink_ctx = nullptr,
                                                InputInitCfg cfg = {}) noexcept {
            return make_input_desc(desc,
                                   host.input_pump(),
                                   host.schedule_fn(),
                                   host.schedule_ctx(),
                                   host.post_demand_fn(),
                                   host.post_ctx(),
                                   host.input_pump_id(),
                                   sink,
                                   sink_ctx,
                                   cfg);
        }

        template <typename Host>
        BringupMinimal(const platform::board::BoardCaps& caps,
                       Host& host,
                       util::usize budget = 8,
                       input::SinkFn sink = nullptr,
                       void* sink_ctx = nullptr,
                       InputInitCfg cfg = {}) noexcept
            : BringupMinimal(caps.uart1,
                             caps.clock,
                             caps.input,
                             caps.spi1,
                             caps.i2c1,
                             caps.can0,
                             caps.sdmmc0,
                             caps.flash0,
                             host.pump(),
                             host.post_io_ready_fn(),
                             host.post_demand_fn(),
                             host.post_ctx(),
                             host.pump_id(),
                             budget,
                             caps.input.driver
                                 ? make_input_desc(caps.input, host, sink, sink_ctx, cfg)
                                 : InputBringupDesc{}) {}

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
                       InputBringupDesc input = {}) noexcept
            : uart_(uart),
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
            if (!input.desc) {
                input.desc = &input_desc_;
            }
            input_required_ = (input.desc && input.desc->driver);
            if (input.desc && input.pump && input.schedule) {
                static const hal::RawInputDriver kNullDriver{};
                const auto* driver = input.desc->driver ? input.desc->driver : &kNullDriver;
                InputInitCaps caps{
                    input.desc->service_cap,
                    input.desc->router_cap,
                    input.desc->pump_cap,
                    "system.clock",
                    "kernel.eda"
                };
                input_.emplace(hal::RawInputSource{*driver},
                               core_.clock,
                               *input.pump,
                               input.schedule,
                               input.schedule_ctx,
                               input.post_more,
                               input.post_ctx,
                               input.pump_id,
                               input.sink,
                               input.sink_ctx,
                               input.cfg,
                               caps);
            }
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
        }

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
                       InputBringupDesc input = {}) noexcept
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
                             input) {}

        util::Result<void> start(util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all),
                                 init::Phase max_phase = init::Phase::app,
                                 std::span<const init::Node* const> extra_nodes = {}) noexcept {
            return start_plan(init::legacy_nodes(extra_nodes), runlevel_mask, max_phase);
        }

        template <typename ExtraPlan>
        util::Result<void> start_plan(const ExtraPlan& extra_plan,
                                      util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all),
                                      init::Phase max_phase = init::Phase::app) noexcept {
            if (input_required_ && !input_) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            const auto spi_nodes = spi_
                ? spi_->node_span()
                : std::span<const init::Node* const>{};
            const auto i2c_nodes = i2c_
                ? i2c_->node_span()
                : std::span<const init::Node* const>{};
            const auto sdmmc_nodes = sdmmc_
                ? sdmmc_->node_span()
                : std::span<const init::Node* const>{};
            const auto flash_nodes = flash_
                ? flash_->node_span()
                : std::span<const init::Node* const>{};
            const auto input_nodes = input_
                ? input_->node_span()
                : std::span<const init::Node* const>{};

            std::array<const init::Node*, 1> can_nodes_storage{};
            util::usize can_nodes_count = 0;
            if (can_channel_) {
                can_nodes_storage[0] = &can_channel_->node;
                can_nodes_count = 1;
            }
            const auto can_nodes = std::span<const init::Node* const>(
                can_nodes_storage.data(), can_nodes_count);

            const auto bringup_plan = init::phase_limit(
                init::runlevel(
                    init::compose(
                        init::legacy(core_),
                        init::legacy(board_),
                        init::legacy_nodes(spi_nodes),
                        init::legacy_nodes(i2c_nodes),
                        init::legacy_nodes(sdmmc_nodes),
                        init::legacy_nodes(flash_nodes),
                        init::legacy_nodes(input_nodes),
                        init::legacy_nodes(can_nodes),
                        extra_plan),
                    runlevel_mask),
                max_phase);
            auto materialized = init::materialize<MaxNodes, MaxCaps>(bringup_plan);
            if (!materialized) {
                return util::unexpected(materialized.error());
            }
            auto r = graph_.build(materialized->node_ptr_span(),
                                  materialized->build_runlevel_mask(),
                                  materialized->build_max_phase());
            if (!r) return r;
            auto r_start = graph_.start();
            if (!r_start) return r_start;
            auto* ch = core_.registry.open_channel(uart_.io_cap);
            if (!ch) {
                return util::unexpected(util::Errc::noent);
            }
            if (!core_.registry.find_channel("io.console0")) {
                io::EndpointDesc console_desc{
                    "io.console0",
                    io::cap_id("io.console0"),
                    io::EndpointKind::channel,
                    io::EndpointCaps::duplex
                };
                auto r_console = core_.registry.register_channel(
                    console_desc, *ch, &core_.reactor);
                if (!r_console) {
                    return util::unexpected(r_console.error());
                }
            }
            return {};
        }

        init::Graph<MaxNodes, MaxCaps>& graph() noexcept { return graph_; }
        io::Registry<MaxEndpoints>& registry() noexcept { return core_.registry; }
        block::Registry<MaxEndpoints>& block_registry() noexcept { return core_.block_registry; }
        io::Reactor& reactor() noexcept { return core_.reactor; }
        charm::system::Clock& clock() noexcept { return core_.clock; }

    private:
        platform::board::UartDesc uart_{};
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
        platform::board::SdmmcDesc sdmmc_desc_{};
        platform::board::SpiFlashDesc flash_desc_{};
        std::optional<SdmmcInitChain<block::Registry<MaxEndpoints>>> sdmmc_{};
        std::optional<SpiFlashInitChain<block::Registry<MaxEndpoints>>> flash_{};
    };

}


