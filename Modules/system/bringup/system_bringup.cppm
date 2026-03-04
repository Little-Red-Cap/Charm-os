module;

#include <array>
#include <optional>
#include <span>

export module charm.system.bringup;

import charm.system.init_core;
import charm.system.init_input;
import charm.system.reactor_pump;
import charm.system.init_usart;
import charm.system.clock;
import hal_input;
import init.graph;
import init.node;
import io.registry;
import io.reactor;
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

        BringupMinimal(const platform::board::UartDesc& uart,
                       const platform::board::ClockDesc& clock_desc,
                       const platform::board::InputDesc& input_desc,
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
              input_desc_(input_desc) {
            if (!input.desc) {
                input.desc = &input_desc_;
            }
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
                               *input.pump,
                               input.schedule,
                               input.schedule_ctx,
                               input.pump_id,
                               input.sink,
                               input.sink_ctx,
                               input.cfg,
                               caps);
                input_nodes_ = input_->node_span();
            }
        }

        util::Result<void> start(util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all),
                                 init::Phase max_phase = init::Phase::app,
                                 std::span<const init::Node* const> extra_nodes = {}) noexcept {
            const auto core_nodes = core_.node_span();
            const auto board_nodes = board_.node_span();
            const auto total =
                core_nodes.size() + board_nodes.size() + input_nodes_.size() + extra_nodes.size();
            if (total > MaxNodes) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
            std::array<const init::Node*, MaxNodes> nodes{};
            util::usize idx = 0;
            for (util::usize i = 0; i < core_nodes.size(); ++i) {
                nodes[idx++] = core_nodes[i];
            }
            for (util::usize i = 0; i < board_nodes.size(); ++i) {
                nodes[idx++] = board_nodes[i];
            }
            for (util::usize i = 0; i < input_nodes_.size(); ++i) {
                nodes[idx++] = input_nodes_[i];
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

    private:
        platform::board::UartDesc uart_{};
        CoreSystemChain<MaxEndpoints> core_;
        UsartInitChain<io::Registry<MaxEndpoints>, RxCap, TxCap> board_;
        platform::board::InputDesc input_desc_{};
        init::Graph<MaxNodes, MaxCaps> graph_{};
        std::optional<InputInitChain<io::Registry<MaxEndpoints>>> input_{};
        std::span<const init::Node* const> input_nodes_{};
    };
}
