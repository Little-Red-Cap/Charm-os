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
    struct monotonic_time {};
    struct main_app {};
}

namespace provider {
    struct host_log {};
    struct host_clock {};
}

namespace spine {
    enum class Phase {
        service,
        app,
    };

    enum class EvidenceStatus : std::uint8_t {
        ok,
        error,
    };

    struct EvidenceField {
        std::string_view key{};
        std::string_view value{};
    };

    template <std::size_t FieldCount>
    struct EvidenceFrame {
        std::string_view component{};
        std::string_view capability{};
        std::string_view provider{};
        EvidenceStatus status{EvidenceStatus::ok};
        std::array<EvidenceField, FieldCount> fields{};
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
    constexpr std::string_view capability_name() noexcept {
        if constexpr (std::same_as<typename Req::kind, cap::TextSink> &&
                      std::same_as<typename Req::role, role::log>) {
            return "TextSink.log";
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
                capability_name<typename RequirementFor<Provides>::type>()...
            };
        }
    };

    template <typename Component>
    struct component_required_cap_names;

    template <typename Provides, typename... Requires>
    struct component_required_cap_names<ComponentDesc<RequirementSet<Requires...>, Provides>> {
        static constexpr auto required() noexcept {
            return std::array<std::string_view, sizeof...(Requires)>{
                capability_name<Requires>()...
            };
        }
    };

    constexpr init::Phase project_phase(Phase phase) noexcept {
        return phase == Phase::app ? init::Phase::app : init::Phase::service;
    }

    template <std::size_t Count>
    constexpr std::array<init::CapId, Count> cap_ids(
        const std::array<std::string_view, Count>& names) noexcept {
        std::array<init::CapId, Count> ids{};
        for (std::size_t i = 0; i < Count; ++i) {
            ids[i] = init::cap_id(names[i]);
        }
        return ids;
    }

    template <typename Component>
    struct InitProjectionNode {
        static constexpr auto provide_names = component_cap_names<Component>::provides();
        static constexpr auto require_names = component_required_cap_names<Component>::required();

        std::array<init::CapId, provide_names.size()> provides{cap_ids(provide_names)};
        std::array<init::CapId, require_names.size()> required_caps{cap_ids(require_names)};
        init::Node node{};

        void materialize(const Component& component) noexcept {
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
    auto init_projection(const Components&... components) noexcept {
        (void)sizeof...(components);
        return std::tuple<InitProjectionNode<Components>...>{};
    }

    template <typename Tuple, typename... Components>
    void materialize_init_projection(Tuple& projected, const Components&... components) noexcept {
        std::apply([&](auto&... item) {
            (item.materialize(components), ...);
        }, projected);
    }

    template <typename Kind, typename ProviderTag, typename Impl>
    struct ProviderRef {
        using kind = Kind;
        using provider = ProviderTag;
        using impl_type = Impl;

        Impl* impl{nullptr};

        explicit ProviderRef(Impl& value) noexcept : impl(&value) {
            static_assert(Kind::template satisfied_by<Impl>,
                          "provider implementation does not satisfy capability kind");
        }

        [[nodiscard]] Impl& get() const noexcept {
            return *impl;
        }
    };

    template <typename Req, typename Provider>
    struct ProfileBinding {
        using requirement = Req;
        using provider = Provider;

        Provider provider_ref;

        explicit ProfileBinding(Provider provider_in) noexcept : provider_ref(provider_in) {
            static_assert(std::same_as<typename Req::kind, typename Provider::kind>,
                          "binding capability kind must match requirement capability kind");
        }

        [[nodiscard]] auto& get() noexcept {
            return provider_ref.get();
        }
    };

    template <typename Req, typename... Bindings>
    inline constexpr bool has_requirement_v =
        (... || std::same_as<typename Bindings::requirement, Req>);

    template <typename... Bindings>
    class ContextView {
    public:
        explicit ContextView(Bindings... bindings) noexcept
            : bindings_(bindings...) {}

        template <typename Req>
            requires has_requirement_v<Req, Bindings...>
        [[nodiscard]] auto& get() noexcept {
            return binding_for<Req>().get();
        }

    private:
        using Tuple = std::tuple<Bindings...>;

        template <typename Req, std::size_t Index = 0>
        [[nodiscard]] auto& binding_for() noexcept {
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
    using LogReq = spine::Requirement<cap::TextSink, role::log>;
    using ClockReq = spine::Requirement<cap::Clock, role::monotonic_time>;
    using LogProv = spine::Provided<cap::TextSink, role::log>;
    using ClockProv = spine::Provided<cap::Clock, role::monotonic_time>;
    using AppProv = spine::Provided<cap::App, role::main_app>;

    using EmptyRequires = spine::RequirementSet<>;
    using AppRequires = spine::RequirementSet<LogReq, ClockReq>;
    using LogProvides = spine::ProviderSet<LogProv>;
    using ClockProvides = spine::ProviderSet<ClockProv>;
    using AppProvides = spine::ProviderSet<AppProv>;

    using LogComponent = spine::ComponentDesc<EmptyRequires, LogProvides>;
    using ClockComponent = spine::ComponentDesc<EmptyRequires, ClockProvides>;
    using AppComponent = spine::ComponentDesc<AppRequires, AppProvides>;

    static_assert(spine::requirements_satisfied_by_v<AppComponent, LogComponent, ClockComponent>);
    static_assert(!spine::requirements_satisfied_by_v<AppComponent, LogComponent>);

    spine::EvidenceFrame<2> g_log_evidence{};
    spine::EvidenceFrame<2> g_clock_evidence{};

    util::Result<void> init_log(void* ctx) noexcept {
        static_cast<spine::InitTrace*>(ctx)->record("log");
        g_log_evidence = spine::EvidenceFrame<2>{
            .component = "log_service",
            .capability = "TextSink.log",
            .provider = "host_log",
            .status = spine::EvidenceStatus::ok,
            .fields = {{
                {"transport", "memory"},
                {"projection", "context"},
            }},
        };
        return {};
    }

    util::Result<void> init_clock(void* ctx) noexcept {
        static_cast<spine::InitTrace*>(ctx)->record("clock");
        g_clock_evidence = spine::EvidenceFrame<2>{
            .component = "clock_service",
            .capability = "Clock.monotonic_time",
            .provider = "host_clock",
            .status = spine::EvidenceStatus::ok,
            .fields = {{
                {"source", "fake"},
                {"tick", "42"},
            }},
        };
        return {};
    }

    util::Result<void> init_app(void* ctx) noexcept {
        static_cast<spine::InitTrace*>(ctx)->record("app");
        return {};
    }

    template <typename Tuple>
    auto node_ptrs(Tuple& projected) noexcept {
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

    struct FakeClock {
        [[nodiscard]] std::uint32_t now_ms() noexcept {
            return 42;
        }
    };

    using LogProvider = spine::ProviderRef<cap::TextSink, provider::host_log, MemoryLog>;
    using ClockProvider = spine::ProviderRef<cap::Clock, provider::host_clock, FakeClock>;
    using LogBinding = spine::ProfileBinding<LogReq, LogProvider>;
    using ClockBinding = spine::ProfileBinding<ClockReq, ClockProvider>;
    using AppContext = spine::ContextView<LogBinding, ClockBinding>;

    void app_tick(AppContext& context) noexcept {
        auto& log = context.get<LogReq>();
        auto& clock = context.get<ClockReq>();
        log.write("Charm Spine tick=");
        log.write(clock.now_ms() == 42 ? "42" : "unexpected");
    }

    bool expect(bool condition, const char* message) noexcept {
        if (!condition) {
            std::printf("[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    bool evidence_ok() noexcept {
        return g_log_evidence.component == "log_service" &&
               g_log_evidence.capability == "TextSink.log" &&
               g_log_evidence.provider == "host_log" &&
               g_log_evidence.fields[0].key == "transport" &&
               g_log_evidence.fields[0].value == "memory" &&
               g_clock_evidence.component == "clock_service" &&
               g_clock_evidence.capability == "Clock.monotonic_time" &&
               g_clock_evidence.provider == "host_clock" &&
               g_clock_evidence.fields[1].key == "tick" &&
               g_clock_evidence.fields[1].value == "42";
    }
}

int main() {
    spine::InitTrace trace{};

    const LogComponent log_service{
        .name = "log_service",
        .phase = spine::Phase::service,
        .init = init_log,
        .ctx = &trace,
    };
    const ClockComponent clock_service{
        .name = "clock_service",
        .phase = spine::Phase::service,
        .init = init_clock,
        .ctx = &trace,
    };
    const AppComponent app{
        .name = "demo_app",
        .phase = spine::Phase::app,
        .init = init_app,
        .ctx = &trace,
    };

    auto projected = spine::init_projection(log_service, clock_service, app);
    spine::materialize_init_projection(projected, log_service, clock_service, app);
    auto nodes = node_ptrs(projected);
    init::Graph<6, 8> graph{};
    auto build = graph.build(nodes);
    if (!expect(build.has_value(), "component topology materializes init projection")) return 1;
    auto start = graph.start();
    if (!expect(start.has_value(), "init projection starts")) return 1;
    if (!expect(trace.count == 3, "all components initialized")) return 1;
    if (!expect(trace.entries[0] == "log", "log provider initializes first")) return 1;
    if (!expect(trace.entries[1] == "clock", "clock provider initializes before app")) return 1;
    if (!expect(trace.entries[2] == "app", "app initializes last")) return 1;

    MemoryLog log{};
    FakeClock clock{};
    AppContext context{
        LogBinding{LogProvider{log}},
        ClockBinding{ClockProvider{clock}},
    };
    app_tick(context);
    if (!expect(log.view() == "Charm Spine tick=42", "profile materializes app ContextView")) return 1;
    if (!expect(log.writes == 2, "app only writes through bound context")) return 1;
    if (!expect(evidence_ok(), "providers produce structured evidence")) return 1;

    std::puts("[charm-spine-smoke] ok");
    return 0;
}
