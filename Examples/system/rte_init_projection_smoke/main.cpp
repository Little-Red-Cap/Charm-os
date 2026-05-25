#include <array>
#include <cstddef>
#include <cstdio>
#include <span>
#include <string_view>
#include <tuple>

import init.graph;
import init.node;
import util.core;
import util.error;

namespace rte {
    enum class Phase {
        service,
        app,
    };

    struct InitTrace {
        std::array<std::string_view, 8> entries{};
        std::size_t count{0};

        void record(std::string_view name) noexcept {
            if (count < entries.size()) {
                entries[count++] = name;
            }
        }
    };

    using InitFn = util::Result<void> (*)(void*) noexcept;

    template <std::size_t ProvideCount, std::size_t RequireCount>
    struct ComponentDesc {
        std::string_view name{};
        Phase phase{Phase::service};
        std::array<std::string_view, ProvideCount> provides{};
        std::array<std::string_view, RequireCount> required_caps{};
        InitFn init{nullptr};
        void* ctx{nullptr};
    };

    constexpr init::Phase project_phase(Phase phase) noexcept {
        switch (phase) {
        case Phase::service:
            return init::Phase::service;
        case Phase::app:
            return init::Phase::app;
        }
        return init::Phase::app;
    }

    template <std::size_t Count>
    constexpr std::array<init::CapId, Count> project_cap_ids(
        const std::array<std::string_view, Count>& names) noexcept {
        std::array<init::CapId, Count> ids{};
        for (std::size_t i = 0; i < Count; ++i) {
            ids[i] = init::cap_id(names[i]);
        }
        return ids;
    }

    template <std::size_t ProvideCount, std::size_t RequireCount>
    struct ProjectedNode {
        std::array<init::CapId, ProvideCount> provides{};
        std::array<init::CapId, RequireCount> required_caps{};
        init::Node node{};

        constexpr void materialize_node(const ComponentDesc<ProvideCount, RequireCount>& component) noexcept {
            node = init::Node{
                component.name,
                project_phase(component.phase),
                static_cast<util::u32>(init::Runlevel::all),
                std::span<const init::CapId>(provides.data(), provides.size()),
                std::span<const init::CapId>(required_caps.data(), required_caps.size()),
                component.init,
                nullptr,
                component.ctx,
            };
        }
    };

    template <std::size_t ProvideCount, std::size_t RequireCount>
    constexpr auto project_to_init_node(
        const ComponentDesc<ProvideCount, RequireCount>& component) noexcept {
        ProjectedNode<ProvideCount, RequireCount> projected{
            .provides = project_cap_ids(component.provides),
            .required_caps = project_cap_ids(component.required_caps),
        };
        return projected;
    }

    template <typename... Components>
    auto project_to_init_nodes(const Components&... components) noexcept {
        return std::tuple{project_to_init_node(components)...};
    }

    template <typename Tuple, typename... Components>
    void materialize_init_nodes(Tuple& projected, const Components&... components) noexcept {
        std::apply([&](auto&... item) {
            (item.materialize_node(components), ...);
        }, projected);
    }
}

namespace {
    using EmptyRequires = std::array<std::string_view, 0>;

    constexpr std::array kPowerProvides{std::string_view{"power"}};
    constexpr std::array kDisplayProvides{std::string_view{"display"}};
    constexpr std::array kDisplayRequires{std::string_view{"power"}};
    constexpr std::array kAppProvides{std::string_view{"app"}};
    constexpr std::array kAppRequires{std::string_view{"display"}};
    constexpr std::array kPowerAppRequires{std::string_view{"app"}};
    constexpr std::array kMissingDisplayRequires{std::string_view{"display"}};

    util::Result<void> init_power(void* ctx) noexcept {
        static_cast<rte::InitTrace*>(ctx)->record("power");
        return {};
    }

    util::Result<void> init_display(void* ctx) noexcept {
        static_cast<rte::InitTrace*>(ctx)->record("display");
        return {};
    }

    util::Result<void> init_app(void* ctx) noexcept {
        static_cast<rte::InitTrace*>(ctx)->record("app");
        return {};
    }

    template <typename Tuple>
    constexpr auto node_ptrs(Tuple& projected) noexcept {
        return std::apply([](auto&... item) {
            return std::array<const init::Node*, sizeof...(item)>{&item.node...};
        }, projected);
    }

    bool expect(bool condition, const char* message) noexcept {
        if (!condition) {
            std::printf("[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    void print_error(util::Errc error) noexcept {
        std::printf("[ERR] init graph error=%d\n", static_cast<int>(error));
    }

    bool test_positive_projection() noexcept {
        rte::InitTrace trace{};

        const rte::ComponentDesc power{
            .name = "power",
            .phase = rte::Phase::service,
            .provides = kPowerProvides,
            .required_caps = EmptyRequires{},
            .init = init_power,
            .ctx = &trace,
        };
        const rte::ComponentDesc display{
            .name = "display",
            .phase = rte::Phase::service,
            .provides = kDisplayProvides,
            .required_caps = kDisplayRequires,
            .init = init_display,
            .ctx = &trace,
        };
        const rte::ComponentDesc app{
            .name = "app",
            .phase = rte::Phase::app,
            .provides = kAppProvides,
            .required_caps = kAppRequires,
            .init = init_app,
            .ctx = &trace,
        };

        auto projected = rte::project_to_init_nodes(power, display, app);
        rte::materialize_init_nodes(projected, power, display, app);
        auto nodes = node_ptrs(projected);
        init::Graph<6, 8> graph{};
        auto build = graph.build(nodes);
        if (!build.has_value()) print_error(build.error());
        if (!expect(build.has_value(), "positive projection builds init graph")) return false;
        if (!expect(graph.size() == 3, "positive projection includes three nodes")) return false;
        if (!expect(graph.ordered() == 3, "positive projection topo sorts three nodes")) return false;

        auto start = graph.start();
        if (!expect(start.has_value(), "positive projection starts init graph")) return false;
        if (!expect(trace.count == 3, "positive projection invokes three init callbacks")) return false;
        if (!expect(trace.entries[0] == "power", "power initializes first")) return false;
        if (!expect(trace.entries[1] == "display", "display initializes after power")) return false;
        if (!expect(trace.entries[2] == "app", "app initializes last")) return false;
        return true;
    }

    bool test_duplicate_provider() noexcept {
        const rte::ComponentDesc power_a{
            .name = "power_a",
            .phase = rte::Phase::service,
            .provides = kPowerProvides,
            .required_caps = EmptyRequires{},
        };
        const rte::ComponentDesc power_b{
            .name = "power_b",
            .phase = rte::Phase::service,
            .provides = kPowerProvides,
            .required_caps = EmptyRequires{},
        };

        auto projected = rte::project_to_init_nodes(power_a, power_b);
        rte::materialize_init_nodes(projected, power_a, power_b);
        auto nodes = node_ptrs(projected);
        init::Graph<4, 4> graph{};
        auto build = graph.build(nodes);
        return expect(!build.has_value() && build.error() == util::Errc::exist,
                      "duplicate provider is rejected by init graph");
    }

    bool test_missing_requirement() noexcept {
        const rte::ComponentDesc app{
            .name = "app",
            .phase = rte::Phase::app,
            .provides = kAppProvides,
            .required_caps = kMissingDisplayRequires,
        };

        auto projected = rte::project_to_init_nodes(app);
        rte::materialize_init_nodes(projected, app);
        auto nodes = node_ptrs(projected);
        init::Graph<4, 4> graph{};
        auto build = graph.build(nodes);
        return expect(!build.has_value() && build.error() == util::Errc::noent,
                      "missing requirement is rejected by init graph");
    }

    bool test_phase_inversion() noexcept {
        const rte::ComponentDesc power{
            .name = "power",
            .phase = rte::Phase::service,
            .provides = kPowerProvides,
            .required_caps = kPowerAppRequires,
        };
        const rte::ComponentDesc app{
            .name = "app",
            .phase = rte::Phase::app,
            .provides = kAppProvides,
            .required_caps = EmptyRequires{},
        };

        auto projected = rte::project_to_init_nodes(power, app);
        rte::materialize_init_nodes(projected, power, app);
        auto nodes = node_ptrs(projected);
        init::Graph<4, 4> graph{};
        auto build = graph.build(nodes);
        return expect(!build.has_value() && build.error() == util::Errc::bad_state,
                      "phase inversion is rejected by init graph");
    }
}

int main() {
    if (!test_positive_projection()) return 1;
    if (!test_duplicate_provider()) return 1;
    if (!test_missing_requirement()) return 1;
    if (!test_phase_inversion()) return 1;

    std::puts("[rte-init-projection-smoke] ok");
    return 0;
}
