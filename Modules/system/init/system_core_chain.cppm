module;

#include <array>
#include <span>

export module charm.system.init_core;

import block.registry;
import init.graph;
import init.node;
import init.plan;
import io.registry;
import io.reactor;
import charm.system.clock;
import kernel.eda;
import kernel.eda.node;
import charm.system.reactor_pump;
import util.core;
import util.error;

export namespace charm::system {
    template <util::usize MaxEndpoints>
    struct CoreSystemChain {
        using RegistryT = io::Registry<MaxEndpoints>;
        using BlockRegistryT = block::Registry<MaxEndpoints>;

        RegistryT registry{};
        BlockRegistryT block_registry{};
        io::Reactor reactor{};
        charm::system::Clock clock{};
        charm::system::ClockBinding clock_binding;
        io::RegistryBinding<RegistryT> registry_binding;
        block::RegistryBinding<BlockRegistryT> block_registry_binding;
        io::ReactorBinding reactor_binding;
        kernel::EdaBinding eda_binding;
        charm::system::ReactorPumpBinding pump_binding;
        std::array<const init::Node*, 6> nodes{};

        CoreSystemChain(const charm::system::ClockOps& clock_ops,
                        void* clock_ctx,
                        ReactorPumpTask& pump_task,
                        PostFn post_fn,
                        PostFn post_more_fn,
                        void* post_ctx,
                        kernel::TaskId pump_id,
                        util::usize budget = 8) noexcept
            : registry(),
              block_registry(),
              reactor(),
              clock(clock_ctx, clock_ops),
              clock_binding(clock),
              registry_binding(registry),
              block_registry_binding(block_registry),
              reactor_binding(reactor),
              eda_binding(),
              pump_binding(pump_task, reactor, post_fn, post_more_fn, post_ctx, pump_id, budget) {
            nodes = {
                &clock_binding.node,
                &registry_binding.node,
                &block_registry_binding.node,
                &reactor_binding.node,
                &eda_binding.node,
                &pump_binding.node
            };
        }

        constexpr auto plan() const noexcept {
            return init::compose(
                init::as_plan(clock_binding),
                init::as_plan(registry_binding),
                init::as_plan(block_registry_binding),
                init::as_plan(reactor_binding),
                init::as_plan(eda_binding),
                init::as_plan(pump_binding));
        }

        [[deprecated("use plan() or build_graph(...) instead of node_span()")]]
        std::span<const init::Node* const> node_span() const noexcept {
            return std::span<const init::Node* const>(nodes.data(), nodes.size());
        }
    };
}
