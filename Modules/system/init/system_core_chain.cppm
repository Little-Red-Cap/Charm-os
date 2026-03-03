module;

#include <array>
#include <span>

export module charm.system.init_core;

import init.node;
import init.graph;
import io.registry;
import io.reactor;
import kernel.eda;
import kernel.eda.node;
import charm.system.reactor_pump;
import util.core;
import util.error;

export namespace charm::system {
    template <util::usize MaxEndpoints>
    struct CoreSystemChain {
        using RegistryT = io::Registry<MaxEndpoints>;

        RegistryT registry{};
        io::Reactor reactor{};
        io::RegistryBinding<RegistryT> registry_binding;
        io::ReactorBinding reactor_binding;
        kernel::EdaBinding eda_binding;
        charm::system::ReactorPumpBinding pump_binding;
        std::array<const init::Node*, 4> nodes{};

        CoreSystemChain(ReactorPumpTask& pump_task,
                        PostFn post_fn,
                        void* post_ctx,
                        kernel::TaskId pump_id,
                        util::usize budget = 8) noexcept
            : registry(),
              reactor(),
              registry_binding(registry),
              reactor_binding(reactor),
              eda_binding(),
              pump_binding(pump_task, reactor, post_fn, post_ctx, pump_id, budget) {
            nodes = {
                &registry_binding.node,
                &reactor_binding.node,
                &eda_binding.node,
                &pump_binding.node
            };
        }

        std::span<const init::Node* const> node_span() const noexcept {
            return std::span<const init::Node* const>(nodes.data(), nodes.size());
        }
    };
}
