module;

export module charm.system.bringup.console;

import charm.system.clock;
import charm.system.init_usart;
import init.graph;
import init.materialize;
import init.node;
import init.plan;
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
                           caps.console_cap, caps.uart.hal_cap) {}

        util::Result<void> start(util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all),
                                 init::Phase max_phase = init::Phase::core) noexcept {
            return start_plan(init::compose(), runlevel_mask, max_phase);
        }

        constexpr auto plan(util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all),
                            init::Phase max_phase = init::Phase::core) const noexcept {
            return plan(init::compose(), runlevel_mask, max_phase);
        }

        template <typename ExtraPlan>
        constexpr auto plan(const ExtraPlan& extra_plan,
                            util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all),
                            init::Phase max_phase = init::Phase::core) const noexcept {
            return init::phase_limit(
                init::runlevel(
                    init::compose(
                        core_plan(),
                        usart_chain_.plan(),
                        extra_plan),
                    runlevel_mask),
                max_phase);
        }

        template <typename ExtraPlan>
        util::Result<void> start_plan(const ExtraPlan& extra_plan,
                                      util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all),
                                      init::Phase max_phase = init::Phase::core) noexcept {
            const auto bringup_plan = plan(extra_plan, runlevel_mask, max_phase);
            return init::start_graph(graph_, bringup_plan);
        }

        init::Graph<MaxNodes, MaxCaps>& graph() noexcept { return graph_; }
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
        init::Graph<MaxNodes, MaxCaps> graph_{};

        constexpr auto core_plan() const noexcept {
            return init::compose(
                init::as_plan(clock_binding_),
                init::as_plan(registry_binding_),
                init::as_plan(reactor_binding_));
        }
    };
}
