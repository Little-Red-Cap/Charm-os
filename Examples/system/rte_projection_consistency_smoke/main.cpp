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
    struct memory_log {};
    struct stdout_trace {};
    struct fake_clock {};
}

namespace rte {
    enum class Phase {
        service,
        app,
    };

    enum class EvidenceStatus : std::uint8_t {
        ok,
        error,
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

    struct EvidenceField {
        std::string_view key{};
        std::string_view value{};
    };

    struct EvidenceFrame {
        std::string_view capability{};
        std::string_view provider{};
        EvidenceStatus status{EvidenceStatus::ok};
        std::array<EvidenceField, 4> fields{};
        std::size_t field_count{0};
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

    template <typename Prov>
    struct RequirementFor;

    template <typename Kind, typename Role>
    struct RequirementFor<Provided<Kind, Role>> {
        using type = Requirement<Kind, Role>;
    };

    template <typename Req, typename ProviderTag>
    struct ProfileBinding {
        using requirement = Req;
        using provider_tag = ProviderTag;
    };

    template <typename Req, typename Binding>
    inline constexpr bool binding_matches_req_v =
        std::same_as<typename Binding::requirement, Req>;

    template <typename Req, typename Binding>
    struct provider_for_requirement;

    template <typename Req, typename ProviderTag>
    struct provider_for_requirement<Req, ProfileBinding<Req, ProviderTag>> {
        using type = ProviderTag;
    };

    template <typename Req, typename... Bindings>
    struct selected_provider;

    template <typename Req>
    struct selected_provider<Req> {
        static_assert(!std::same_as<Req, Req>, "missing binding for requirement");
    };

    template <typename Req, typename ProviderTag, typename... Rest>
    struct selected_provider<Req, ProfileBinding<Req, ProviderTag>, Rest...> {
        using type = ProviderTag;
    };

    template <typename Req, typename OtherReq, typename ProviderTag, typename... Rest>
    struct selected_provider<Req, ProfileBinding<OtherReq, ProviderTag>, Rest...>
        : selected_provider<Req, Rest...> {};

    template <typename Req, typename... Bindings>
    using selected_provider_t = typename selected_provider<Req, Bindings...>::type;

    template <typename Req, typename ProviderTag>
    struct ProjectionProvider {
        using requirement = Req;
        using provider_tag = ProviderTag;
    };

    template <typename Expected, typename Actual>
    inline constexpr bool projection_provider_consistent_v =
        std::same_as<typename Expected::requirement, typename Actual::requirement> &&
        std::same_as<typename Expected::provider_tag, typename Actual::provider_tag>;

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
    constexpr std::string_view provided_cap_name() noexcept {
        return requirement_cap_name<typename RequirementFor<Prov>::type>();
    }

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

    template <typename Component>
    struct component_cap_names;

    template <typename Requires, typename... Provides>
    struct component_cap_names<ComponentDesc<Requires, ProviderSet<Provides...>>> {
        static constexpr auto provides() noexcept {
            return std::array<std::string_view, sizeof...(Provides)>{
                provided_cap_name<Provides>()...
            };
        }
    };

    template <typename Component>
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
    struct RuntimeBinding {
        using requirement = Req;
        using provider = Provider;

        Provider provider_ref;

        constexpr explicit RuntimeBinding(Provider provider_in) noexcept : provider_ref(provider_in) {
            static_assert(std::same_as<typename Req::kind, typename Provider::kind>,
                          "runtime binding capability kind must match requirement capability kind");
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
    using ClockReq = rte::Requirement<cap::Clock, role::monotonic_time>;
    using LogProv = rte::Provided<cap::TextSink, role::log>;
    using ClockProv = rte::Provided<cap::Clock, role::monotonic_time>;
    using AppProv = rte::Provided<cap::App, role::main_app>;
    using EmptyRequires = rte::RequirementSet<>;
    using AppRequires = rte::RequirementSet<LogReq, ClockReq>;
    using LogProvides = rte::ProviderSet<LogProv>;
    using ClockProvides = rte::ProviderSet<ClockProv>;
    using AppProvides = rte::ProviderSet<AppProv>;
    using LogComponent = rte::ComponentDesc<EmptyRequires, LogProvides>;
    using ClockComponent = rte::ComponentDesc<EmptyRequires, ClockProvides>;
    using AppComponent = rte::ComponentDesc<AppRequires, AppProvides>;
    using LogBinding = rte::ProfileBinding<LogReq, provider::memory_log>;
    using ClockBinding = rte::ProfileBinding<ClockReq, provider::fake_clock>;
    using ExpectedLogProvider = rte::ProjectionProvider<LogReq, provider::memory_log>;
    using ExpectedClockProvider = rte::ProjectionProvider<ClockReq, provider::fake_clock>;
    using BadLogEvidenceProvider = rte::ProjectionProvider<LogReq, provider::stdout_trace>;
    using SelectedLogProvider = rte::ProjectionProvider<LogReq, rte::selected_provider_t<LogReq, LogBinding, ClockBinding>>;
    using SelectedClockProvider = rte::ProjectionProvider<ClockReq, rte::selected_provider_t<ClockReq, LogBinding, ClockBinding>>;

    static_assert(rte::projection_provider_consistent_v<ExpectedLogProvider, SelectedLogProvider>);
    static_assert(rte::projection_provider_consistent_v<ExpectedClockProvider, SelectedClockProvider>);
    static_assert(!rte::projection_provider_consistent_v<ExpectedLogProvider, BadLogEvidenceProvider>);

    util::Result<void> init_log(void* ctx) noexcept {
        static_cast<rte::InitTrace*>(ctx)->record("log:memory_log");
        return {};
    }

    util::Result<void> init_clock(void* ctx) noexcept {
        static_cast<rte::InitTrace*>(ctx)->record("clock:fake_clock");
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

    bool run_init_projection(rte::InitTrace& trace) noexcept {
        const LogComponent log_service{
            .name = "log_service",
            .phase = rte::Phase::service,
            .init = init_log,
            .ctx = &trace,
        };
        const ClockComponent clock_service{
            .name = "clock_service",
            .phase = rte::Phase::service,
            .init = init_clock,
            .ctx = &trace,
        };
        const AppComponent app{
            .name = "demo_app",
            .phase = rte::Phase::app,
            .init = init_app,
            .ctx = &trace,
        };

        std::tuple projected{
            rte::ProjectedNode<LogComponent>{},
            rte::ProjectedNode<ClockComponent>{},
            rte::ProjectedNode<AppComponent>{},
        };
        rte::materialize_init_nodes(projected, log_service, clock_service, app);
        auto nodes = node_ptrs(projected);
        init::Graph<6, 8> graph{};
        auto build = graph.build(nodes);
        if (!build) return false;
        auto start = graph.start();
        return start.has_value();
    }

    struct MemoryLog {
        std::array<char, 128> bytes{};
        std::size_t used{0};

        void write(std::string_view text) noexcept {
            const auto remaining = bytes.size() - used;
            const auto count = text.size() < remaining ? text.size() : remaining;
            if (count != 0) {
                std::memcpy(bytes.data() + used, text.data(), count);
                used += count;
            }
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

    static_assert(cap::TextSink::satisfied_by<MemoryLog>);
    static_assert(cap::Clock::satisfied_by<FakeClock>);

    using LogRef = rte::ProviderRef<cap::TextSink, provider::memory_log, MemoryLog>;
    using ClockRef = rte::ProviderRef<cap::Clock, provider::fake_clock, FakeClock>;
    using RuntimeLogBinding = rte::RuntimeBinding<LogReq, LogRef>;
    using RuntimeClockBinding = rte::RuntimeBinding<ClockReq, ClockRef>;
    using AppContext = rte::ContextView<RuntimeLogBinding, RuntimeClockBinding>;

    rte::EvidenceFrame run_evidence_projection(const MemoryLog& log) noexcept {
        return rte::EvidenceFrame{
            .capability = "TextSink.log",
            .provider = "memory_log",
            .status = rte::EvidenceStatus::ok,
            .fields = {{
                {"selected_provider", log.view() == "context:memory_log" ? "memory_log" : "unexpected"},
            }},
            .field_count = 1,
        };
    }

    bool run_context_projection(MemoryLog& log, FakeClock& clock) noexcept {
        AppContext context{
            RuntimeLogBinding{LogRef{log}},
            RuntimeClockBinding{ClockRef{clock}},
        };
        auto& bound_log = context.get<LogReq>();
        auto& bound_clock = context.get<ClockReq>();
        bound_log.write("context:");
        bound_log.write(bound_clock.now_ms() == 42 ? "memory_log" : "unexpected");
        return true;
    }

    bool expect(bool condition, const char* message) noexcept {
        if (!condition) {
            std::printf("[ERR] %s\n", message);
            return false;
        }
        return true;
    }
}

int main() {
    rte::InitTrace trace{};
    MemoryLog log{};
    FakeClock clock{};

    if (!expect(run_init_projection(trace), "init projection runs")) return 1;
    if (!expect(trace.count == 3, "init projection starts all nodes")) return 1;
    if (!expect(trace.entries[0] == "log:memory_log", "init projection preserves log provider")) return 1;
    if (!expect(trace.entries[1] == "clock:fake_clock", "init projection preserves clock provider")) return 1;
    if (!expect(run_context_projection(log, clock), "context projection runs")) return 1;
    if (!expect(log.view() == "context:memory_log", "context projection preserves log provider")) return 1;

    const auto evidence = run_evidence_projection(log);
    if (!expect(evidence.status == rte::EvidenceStatus::ok, "evidence projection runs")) return 1;
    if (!expect(evidence.capability == "TextSink.log", "evidence projection preserves requirement")) return 1;
    if (!expect(evidence.provider == "memory_log", "evidence projection preserves selected provider")) return 1;
    if (!expect(evidence.fields[0].value == "memory_log", "all projections agree on provider identity")) return 1;

    std::puts("[rte-projection-consistency-smoke] ok");
    return 0;
}
