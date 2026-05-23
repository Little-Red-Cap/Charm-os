#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>
#include <tuple>
#include <type_traits>

import init.graph;
import init.node;
import util.core;
import util.error;

namespace cap {
    struct TextSink {
        template <typename T>
        static constexpr bool satisfied_by = requires(T& sink, std::string_view text) {
            { sink.write(text) } noexcept -> std::same_as<void>;
        };
    };

    struct Clock {
        template <typename T>
        static constexpr bool satisfied_by = requires(T& clock) {
            { clock.now_ms() } noexcept -> std::same_as<std::uint32_t>;
        };
    };

    struct App {};
}

namespace role {
    struct log {};
    struct debug_trace {};
    struct monotonic_time {};
    struct main_app {};
}

namespace provider {
    struct host_memory_log {};
    struct host_stdout_trace {};
    struct host_clock {};
    struct mcu_uart_log {};
    struct mcu_itm_trace {};
    struct mcu_systick {};
}

namespace rte {
    enum class Phase {
        service,
        app,
    };

    using InitFn = util::Result<void> (*)(void*) noexcept;

    struct InitTrace {
        std::array<std::string_view, 8> entries{};
        std::size_t count{0};

        void record(std::string_view name) noexcept {
            if (count < entries.size()) {
                entries[count++] = name;
            }
        }
    };

    template <typename Kind, typename Role>
    struct Requirement {
        using kind = Kind;
        using role = Role;
    };

    template <typename Kind, typename Role>
    struct Provided {
        using kind = Kind;
        using role = Role;
    };

    template <typename... Req>
    struct RequirementSet {};

    template <typename... Prov>
    struct ProviderSet {};

    template <typename Needle, typename Set>
    struct set_contains;

    template <typename Needle, template <typename...> typename Set, typename... Items>
    struct set_contains<Needle, Set<Items...>>
        : std::bool_constant<(... || std::same_as<Needle, Items>)> {};

    template <typename Needle, typename Set>
    inline constexpr bool set_contains_v = set_contains<Needle, Set>::value;

    template <typename Req>
    struct ProvidedFor;

    template <typename Kind, typename Role>
    struct ProvidedFor<Requirement<Kind, Role>> {
        using type = Provided<Kind, Role>;
    };

    template <typename Requires, typename Provides>
    struct ComponentDesc {
        using required_set = Requires;
        using provided_set = Provides;

        std::string_view name{};
        Phase phase{Phase::service};
        Requires required{};
        Provides provided{};
        InitFn init{nullptr};
        void* ctx{nullptr};
    };

    template <typename Req, typename Component>
    inline constexpr bool component_provides_v =
        set_contains_v<typename ProvidedFor<Req>::type, typename Component::provided_set>;

    template <typename Req, typename... Components>
    inline constexpr bool provided_by_any_v =
        (... || component_provides_v<Req, Components>);

    template <typename RequiredSet, typename... Components>
    struct requirements_satisfied_by;

    template <typename... Reqs, typename... Components>
    struct requirements_satisfied_by<RequirementSet<Reqs...>, Components...>
        : std::bool_constant<(... && provided_by_any_v<Reqs, Components...>)> {};

    template <typename Component, typename... Components>
    inline constexpr bool requirements_satisfied_by_v =
        requirements_satisfied_by<typename Component::required_set, Components...>::value;

    template <typename Req>
    constexpr std::string_view requirement_cap_name() noexcept {
        if constexpr (std::same_as<typename Req::kind, cap::TextSink> &&
                      std::same_as<typename Req::role, role::log>) {
            return "TextSink.log";
        } else if constexpr (std::same_as<typename Req::kind, cap::TextSink> &&
                             std::same_as<typename Req::role, role::debug_trace>) {
            return "TextSink.debug_trace";
        } else if constexpr (std::same_as<typename Req::kind, cap::Clock> &&
                             std::same_as<typename Req::role, role::monotonic_time>) {
            return "Clock.monotonic_time";
        } else if constexpr (std::same_as<typename Req::kind, cap::App> &&
                             std::same_as<typename Req::role, role::main_app>) {
            return "App.main_app";
        } else {
            return "unknown";
        }
    }

    template <typename Prov>
    struct RequirementFor;

    template <typename Kind, typename Role>
    struct RequirementFor<Provided<Kind, Role>> {
        using type = Requirement<Kind, Role>;
    };

    template <typename Component>
    struct component_cap_names;

    template <typename Requires, typename... Provides>
    struct component_cap_names<ComponentDesc<Requires, ProviderSet<Provides...>>> {
        static constexpr auto provides() noexcept {
            return std::array<std::string_view, sizeof...(Provides)>{
                requirement_cap_name<typename RequirementFor<Provides>::type>()...
            };
        }
    };

    template <typename Provides, typename... Requires>
    struct component_required_cap_names;

    template <typename Provides, typename... Requires>
    struct component_required_cap_names<ComponentDesc<RequirementSet<Requires...>, Provides>> {
        static constexpr auto required() noexcept {
            return std::array<std::string_view, sizeof...(Requires)>{
                requirement_cap_name<Requires>()...
            };
        }
    };

    constexpr init::Phase project_phase(Phase phase) noexcept {
        return phase == Phase::app ? init::Phase::app : init::Phase::service;
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

    template <typename Component>
    struct ProjectedNode {
        using component_type = Component;

        static constexpr auto provide_names = component_cap_names<Component>::provides();
        static constexpr auto require_names = component_required_cap_names<Component>::required();

        std::array<init::CapId, provide_names.size()> provides{project_cap_ids(provide_names)};
        std::array<init::CapId, require_names.size()> required_caps{project_cap_ids(require_names)};
        init::Node node{};

        constexpr void materialize_node(const Component& component) noexcept {
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

    template <typename... Components>
    auto project_to_init_nodes(const Components&... components) noexcept {
        (void)sizeof...(components);
        return std::tuple<ProjectedNode<Components>...>{};
    }

    template <typename Tuple, typename... Components>
    void materialize_init_nodes(Tuple& projected, const Components&... components) noexcept {
        std::apply([&](auto&... item) {
            (item.materialize_node(components), ...);
        }, projected);
    }

    template <typename Kind, typename ProviderTag, typename Impl>
    struct ProviderRef {
        using kind = Kind;
        using provider = ProviderTag;
        using impl_type = Impl;

        Impl* impl{nullptr};

        constexpr explicit ProviderRef(Impl& value) noexcept : impl(&value) {
            static_assert(Kind::template satisfied_by<Impl>,
                          "provider implementation does not satisfy capability kind");
        }

        [[nodiscard]] constexpr Impl& get() const noexcept {
            return *impl;
        }
    };

    template <typename Req, typename Provider>
    struct ProfileBinding {
        using requirement = Req;
        using provider = Provider;

        Provider provider_ref;

        constexpr explicit ProfileBinding(Provider provider_in) noexcept : provider_ref(provider_in) {
            static_assert(std::same_as<typename Req::kind, typename Provider::kind>,
                          "binding capability kind must match requirement capability kind");
        }

        [[nodiscard]] constexpr auto& get() noexcept {
            return provider_ref.get();
        }
    };

    template <typename Req, typename... Bindings>
    inline constexpr bool has_requirement_v =
        (... || std::same_as<typename Bindings::requirement, Req>);

    template <typename... Bindings>
    class ContextView {
    public:
        constexpr explicit ContextView(Bindings... bindings) noexcept
            : bindings_(bindings...) {}

        template <typename Req>
            requires has_requirement_v<Req, Bindings...>
        [[nodiscard]] constexpr auto& get() noexcept {
            return binding_for<Req>().get();
        }

    private:
        using Tuple = std::tuple<Bindings...>;

        template <typename Req, std::size_t Index = 0>
        [[nodiscard]] constexpr auto& binding_for() noexcept {
            static_assert(Index < sizeof...(Bindings), "missing binding for requirement");
            using Binding = std::tuple_element_t<Index, Tuple>;
            if constexpr (std::same_as<typename Binding::requirement, Req>) {
                return std::get<Index>(bindings_);
            } else {
                return binding_for<Req, Index + 1>();
            }
        }

        Tuple bindings_;
    };
}

namespace {
    using LogReq = rte::Requirement<cap::TextSink, role::log>;
    using DebugTraceReq = rte::Requirement<cap::TextSink, role::debug_trace>;
    using ClockReq = rte::Requirement<cap::Clock, role::monotonic_time>;
    using LogProv = rte::Provided<cap::TextSink, role::log>;
    using DebugTraceProv = rte::Provided<cap::TextSink, role::debug_trace>;
    using ClockProv = rte::Provided<cap::Clock, role::monotonic_time>;
    using AppProv = rte::Provided<cap::App, role::main_app>;

    using EmptyRequires = rte::RequirementSet<>;
    using AppRequires = rte::RequirementSet<LogReq, ClockReq>;
    using LogProvides = rte::ProviderSet<LogProv>;
    using DebugTraceProvides = rte::ProviderSet<DebugTraceProv>;
    using ClockProvides = rte::ProviderSet<ClockProv>;
    using AppProvides = rte::ProviderSet<AppProv>;

    using LogService = rte::ComponentDesc<EmptyRequires, LogProvides>;
    using DebugTraceService = rte::ComponentDesc<EmptyRequires, DebugTraceProvides>;
    using ClockService = rte::ComponentDesc<EmptyRequires, ClockProvides>;
    using AppComponent = rte::ComponentDesc<AppRequires, AppProvides>;

    util::Result<void> init_log(void* ctx) noexcept {
        static_cast<rte::InitTrace*>(ctx)->record("log");
        return {};
    }

    util::Result<void> init_debug_trace(void* ctx) noexcept {
        static_cast<rte::InitTrace*>(ctx)->record("debug_trace");
        return {};
    }

    util::Result<void> init_clock(void* ctx) noexcept {
        static_cast<rte::InitTrace*>(ctx)->record("clock");
        return {};
    }

    util::Result<void> init_app(void* ctx) noexcept {
        static_cast<rte::InitTrace*>(ctx)->record("app");
        return {};
    }

    constexpr LogService kLogService{
        .name = "log_service",
        .phase = rte::Phase::service,
    };

    constexpr DebugTraceService kDebugTraceService{
        .name = "debug_trace_service",
        .phase = rte::Phase::service,
    };

    constexpr ClockService kClockService{
        .name = "clock_service",
        .phase = rte::Phase::service,
    };

    constexpr AppComponent kAppComponent{
        .name = "demo_app",
        .phase = rte::Phase::app,
    };

    static_assert(rte::requirements_satisfied_by_v<AppComponent, LogService, ClockService>);
    static_assert(rte::requirements_satisfied_by_v<AppComponent, LogService, DebugTraceService, ClockService>);
    static_assert(!rte::requirements_satisfied_by_v<AppComponent, DebugTraceService, ClockService>);

    template <typename Tuple>
    constexpr auto node_ptrs(Tuple& projected) noexcept {
        return std::apply([](auto&... item) {
            return std::array<const init::Node*, sizeof...(item)>{&item.node...};
        }, projected);
    }

    struct MemoryLog {
        std::array<char, 128> bytes{};
        std::size_t used{0};
        std::uint32_t writes{0};

        void write(std::string_view text) noexcept {
            const auto remaining = bytes.size() - used;
            const auto count = text.size() < remaining ? text.size() : remaining;
            if (count != 0) {
                std::memcpy(bytes.data() + used, text.data(), count);
                used += count;
            }
            ++writes;
        }

        [[nodiscard]] std::string_view view() const noexcept {
            return {bytes.data(), used};
        }
    };

    struct TraceLog {
        std::uint32_t writes{0};

        void write(std::string_view) noexcept {
            ++writes;
        }
    };

    struct FakeClock {
        std::uint32_t now{42};

        [[nodiscard]] std::uint32_t now_ms() noexcept {
            return now;
        }
    };

    struct McuUartLog {
        std::array<char, 128> bytes{};
        std::size_t used{0};
        std::uint32_t writes{0};

        void write(std::string_view text) noexcept {
            const auto remaining = bytes.size() - used;
            const auto count = text.size() < remaining ? text.size() : remaining;
            if (count != 0) {
                std::memcpy(bytes.data() + used, text.data(), count);
                used += count;
            }
            ++writes;
        }

        [[nodiscard]] std::string_view view() const noexcept {
            return {bytes.data(), used};
        }
    };

    struct SystickClock {
        std::uint32_t tick{9000};

        [[nodiscard]] std::uint32_t now_ms() noexcept {
            return tick;
        }
    };

    static_assert(cap::TextSink::satisfied_by<MemoryLog>);
    static_assert(cap::TextSink::satisfied_by<TraceLog>);
    static_assert(cap::TextSink::satisfied_by<McuUartLog>);
    static_assert(cap::Clock::satisfied_by<FakeClock>);
    static_assert(cap::Clock::satisfied_by<SystickClock>);

    using HostLogProvider = rte::ProviderRef<cap::TextSink, provider::host_memory_log, MemoryLog>;
    using HostTraceProvider = rte::ProviderRef<cap::TextSink, provider::host_stdout_trace, TraceLog>;
    using HostClockProvider = rte::ProviderRef<cap::Clock, provider::host_clock, FakeClock>;
    using McuLogProvider = rte::ProviderRef<cap::TextSink, provider::mcu_uart_log, McuUartLog>;
    using McuClockProvider = rte::ProviderRef<cap::Clock, provider::mcu_systick, SystickClock>;

    using HostLogBinding = rte::ProfileBinding<LogReq, HostLogProvider>;
    using HostClockBinding = rte::ProfileBinding<ClockReq, HostClockProvider>;
    using McuLogBinding = rte::ProfileBinding<LogReq, McuLogProvider>;
    using McuClockBinding = rte::ProfileBinding<ClockReq, McuClockProvider>;
    using HostContext = rte::ContextView<HostLogBinding, HostClockBinding>;
    using McuContext = rte::ContextView<McuLogBinding, McuClockBinding>;

    static_assert(!rte::has_requirement_v<DebugTraceReq, HostLogBinding, HostClockBinding>);

    template <typename Context>
    void app_tick(Context& context) noexcept {
        auto& log = context.template get<LogReq>();
        auto& clock = context.template get<ClockReq>();
        log.write("tick=");
        log.write(clock.now_ms() == 42 ? "42" : "mcu");
    }

    bool expect(bool condition, const char* message) noexcept {
        if (!condition) {
            std::printf("[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    bool run_init_projection() noexcept {
        rte::InitTrace trace{};
        const LogService log_service{
            .name = kLogService.name,
            .phase = kLogService.phase,
            .init = init_log,
            .ctx = &trace,
        };
        const DebugTraceService debug_trace_service{
            .name = kDebugTraceService.name,
            .phase = kDebugTraceService.phase,
            .init = init_debug_trace,
            .ctx = &trace,
        };
        const ClockService clock_service{
            .name = kClockService.name,
            .phase = kClockService.phase,
            .init = init_clock,
            .ctx = &trace,
        };
        const AppComponent app{
            .name = kAppComponent.name,
            .phase = kAppComponent.phase,
            .init = init_app,
            .ctx = &trace,
        };

        auto projected = rte::project_to_init_nodes(log_service, debug_trace_service, clock_service, app);
        rte::materialize_init_nodes(projected, log_service, debug_trace_service, clock_service, app);
        auto nodes = node_ptrs(projected);
        init::Graph<8, 8> graph{};
        auto build = graph.build(nodes);
        if (!expect(build.has_value(), "profile topology projects to init graph")) return false;
        auto start = graph.start();
        if (!expect(start.has_value(), "projected init graph starts")) return false;
        if (!expect(trace.count == 4, "projected init graph starts four components")) return false;
        if (!expect(trace.entries[0] == "log", "log starts first")) return false;
        if (!expect(trace.entries[1] == "debug_trace", "debug trace starts independently")) return false;
        if (!expect(trace.entries[2] == "clock", "clock starts before app")) return false;
        if (!expect(trace.entries[3] == "app", "app starts after required providers")) return false;
        return true;
    }

    bool run_context_projection() noexcept {
        MemoryLog host_log{};
        TraceLog host_trace{};
        FakeClock host_clock{};
        McuUartLog mcu_log{};
        SystickClock mcu_clock{};

        [[maybe_unused]] HostTraceProvider unbound_trace{host_trace};
        HostContext host_context{
            HostLogBinding{HostLogProvider{host_log}},
            HostClockBinding{HostClockProvider{host_clock}},
        };
        McuContext mcu_context{
            McuLogBinding{McuLogProvider{mcu_log}},
            McuClockBinding{McuClockProvider{mcu_clock}},
        };

        app_tick(host_context);
        app_tick(mcu_context);

        if (!expect(host_log.view() == "tick=42", "host profile materializes app context")) return false;
        if (!expect(mcu_log.view() == "tick=mcu", "mock MCU profile materializes same app context")) return false;
        if (!expect(host_trace.writes == 0, "unbound TextSink provider is not implicitly selected")) return false;
        return true;
    }
}

int main() {
    if (!run_init_projection()) return 1;
    if (!run_context_projection()) return 1;

    std::puts("[rte-profile-materialization-smoke] ok");
    return 0;
}
