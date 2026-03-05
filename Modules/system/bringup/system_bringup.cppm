module;

#include <array>
#include <optional>
#include <span>

export module charm.system.bringup;

import charm.system.init_core;
import charm.system.init_input;
import charm.system.init_i2c;
import charm.system.init_spi;
import charm.system.reactor_pump;
import charm.system.init_usart;
import charm.system.clock;
import hal_input;
import init.graph;
import init.node;
import io.registry;
import io.reactor;
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
            kernel::TaskId pump_id{};
            input::SinkFn sink{nullptr};
            void* sink_ctx{nullptr};
            InputInitCfg cfg{};
        };

        static InputBringupDesc make_input_desc(const platform::board::InputDesc& desc,
                                                input::InputPumpTask& pump,
                                                input::ScheduleFn schedule,
                                                void* schedule_ctx,
                                                kernel::TaskId pump_id,
                                                input::SinkFn sink = nullptr,
                                                void* sink_ctx = nullptr,
                                                InputInitCfg cfg = {}) noexcept {
            InputBringupDesc out{};
            out.desc = &desc;
            out.pump = &pump;
            out.schedule = schedule;
            out.schedule_ctx = schedule_ctx;
            out.pump_id = pump_id;
            out.sink = sink;
            out.sink_ctx = sink_ctx;
            out.cfg = cfg;
            return out;
        }

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
                       InputBringupDesc input = {}) noexcept
            : uart_(uart),
              core_(charm::system::ClockOps{clock_desc.now_ms, clock_desc.now_us},
                    clock_desc.ctx,
                    pump_task, post_fn, post_ctx, pump_id, budget),
              board_(core_.registry, core_.reactor,
                     uart.handle, uart.config, uart.io_cap, uart.hal_cap),
              input_desc_(input_desc),
              spi_desc_(spi_desc),
              i2c_desc_(i2c_desc),
              can_desc_(can_desc) {
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
            util::usize board_count = 0;
            const auto board_nodes = board_.node_span();
            for (util::usize i = 0; i < board_nodes.size(); ++i) {
                board_nodes_[board_count++] = board_nodes[i];
            }
            if (spi_) {
                const auto spi_nodes = spi_->node_span();
                for (util::usize i = 0; i < spi_nodes.size(); ++i) {
                    board_nodes_[board_count++] = spi_nodes[i];
                }
            }
            if (i2c_) {
                const auto i2c_nodes = i2c_->node_span();
                for (util::usize i = 0; i < i2c_nodes.size(); ++i) {
                    board_nodes_[board_count++] = i2c_nodes[i];
                }
            }
            if (input_) {
                const auto input_nodes = input_->node_span();
                for (util::usize i = 0; i < input_nodes.size(); ++i) {
                    board_nodes_[board_count++] = input_nodes[i];
                }
            }
            if (can_desc_.channel) {
                io::EndpointDesc desc{
                    can_desc_.io_cap,
                    io::cap_id(can_desc_.io_cap),
                    io::EndpointKind::channel,
                    io::EndpointCaps::duplex
                };
                can_channel_.emplace(core_.registry, *can_desc_.channel, desc);
                board_nodes_[board_count++] = &can_channel_->node;
            }
            board_nodes_span_ = std::span<const init::Node* const>(board_nodes_.data(), board_count);
        }

        util::Result<void> start(util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all),
                                 init::Phase max_phase = init::Phase::app,
                                 std::span<const init::Node* const> extra_nodes = {}) noexcept {
            if (input_required_ && !input_) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            const auto core_nodes = core_.node_span();
            const auto total =
                core_nodes.size() + board_nodes_span_.size() + extra_nodes.size();
            if (total > MaxNodes) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
            std::array<const init::Node*, MaxNodes> nodes{};
            util::usize idx = 0;
            for (util::usize i = 0; i < core_nodes.size(); ++i) {
                nodes[idx++] = core_nodes[i];
            }
            for (util::usize i = 0; i < board_nodes_span_.size(); ++i) {
                nodes[idx++] = board_nodes_span_[i];
            }
            for (util::usize i = 0; i < extra_nodes.size(); ++i) {
                nodes[idx++] = extra_nodes[i];
            }
            auto r = graph_.build(std::span<const init::Node* const>(nodes.data(), idx),
                                  runlevel_mask, max_phase);
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
        io::Reactor& reactor() noexcept { return core_.reactor; }
        charm::system::Clock& clock() noexcept { return core_.clock; }

    private:
        platform::board::UartDesc uart_{};
        CoreSystemChain<MaxEndpoints> core_;
        UsartInitChain<io::Registry<MaxEndpoints>, RxCap, TxCap> board_;
        platform::board::InputDesc input_desc_{};
        init::Graph<MaxNodes, MaxCaps> graph_{};
        std::optional<InputInitChain<io::Registry<MaxEndpoints>>> input_{};
        std::array<const init::Node*, 16> board_nodes_{};
        std::span<const init::Node* const> board_nodes_span_{};
        bool input_required_{false};
        platform::board::SpiDesc spi_desc_{};
        platform::board::I2cDesc i2c_desc_{};
        std::optional<SpiInitChain> spi_{};
        std::optional<I2cInitChain> i2c_{};
        platform::board::CanDesc can_desc_{};
        std::optional<io::ChannelBinding<io::Registry<MaxEndpoints>>> can_channel_{};
    };
}
