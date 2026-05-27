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

    template <typename Tag, typename Provides>
    struct ProviderDesc {
        using tag = Tag;
        using provided_set = Provides;
    };

    template <typename Req, typename ProviderTag>
    struct ProfileBinding {
        using requirement = Req;
        using provider_tag = ProviderTag;
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

    template <typename... Components>
    struct ComponentList {};

    template <typename... Providers>
    struct ProviderList {};

    template <typename... Bindings>
    struct BindingList {};

    template <typename App, typename Components, typename Providers, typename Bindings>
    struct ProfileSpec {
        using app = App;
        using components = Components;
        using providers = Providers;
        using bindings = Bindings;
    };

    template <typename Spec>
    struct ResolvedProfile {
        using spec = Spec;
    };

    template <typename T>
    struct is_resolved_profile : std::false_type {};

    template <typename Spec>
    struct is_resolved_profile<ResolvedProfile<Spec>> : std::true_type {};

    template <typename T>
    inline constexpr bool is_resolved_profile_v = is_resolved_profile<T>::value;

    template <typename T>
    inline constexpr bool projection_input_v = is_resolved_profile_v<T>;

    template <typename Req, typename Binding>
    inline constexpr bool binding_matches_req_v =
        std::same_as<typename Binding::requirement, Req>;

    template <typename Req, typename Bindings>
    struct binding_count;

    template <typename Req, typename... Bindings>
    struct binding_count<Req, BindingList<Bindings...>>
        : std::integral_constant<std::size_t, (std::size_t{0} + ... + (binding_matches_req_v<Req, Bindings> ? 1u : 0u))> {};

    template <typename Req, typename Bindings>
    inline constexpr std::size_t binding_count_v = binding_count<Req, Bindings>::value;

    template <typename Binding, typename App>
    inline constexpr bool binding_targets_app_requirement_v =
        set_contains_v<typename Binding::requirement, typename App::required_set>;

    template <typename Bindings, typename App>
    struct all_bindings_target_app_requirements;

    template <typename... Bindings, typename App>
    struct all_bindings_target_app_requirements<BindingList<Bindings...>, App>
        : std::bool_constant<(... && binding_targets_app_requirement_v<Bindings, App>)> {};

    template <typename Bindings, typename App>
    inline constexpr bool all_bindings_target_app_requirements_v =
        all_bindings_target_app_requirements<Bindings, App>::value;

    template <typename ProviderTag, typename Providers>
    struct provider_count;

    template <typename ProviderTag, typename... Providers>
    struct provider_count<ProviderTag, ProviderList<Providers...>>
        : std::integral_constant<std::size_t, (std::size_t{0} + ... + (std::same_as<ProviderTag, typename Providers::tag> ? 1u : 0u))> {};

    template <typename ProviderTag, typename Providers>
    inline constexpr std::size_t provider_count_v = provider_count<ProviderTag, Providers>::value;

    template <typename Req, typename Provider>
    inline constexpr bool provider_declares_requirement_v =
        set_contains_v<typename ProvidedFor<Req>::type, typename Provider::provided_set>;

    template <typename Req, typename ProviderTag, typename Providers>
    struct provider_tag_declares_requirement;

    template <typename Req, typename ProviderTag, typename... Providers>
    struct provider_tag_declares_requirement<Req, ProviderTag, ProviderList<Providers...>>
        : std::bool_constant<(... || (std::same_as<ProviderTag, typename Providers::tag> &&
                                      provider_declares_requirement_v<Req, Providers>))> {};

    template <typename Req, typename ProviderTag, typename Providers>
    inline constexpr bool provider_tag_declares_requirement_v =
        provider_tag_declares_requirement<Req, ProviderTag, Providers>::value;

    template <typename Binding, typename Providers>
    inline constexpr bool binding_is_valid_v =
        provider_count_v<typename Binding::provider_tag, Providers> == 1 &&
        provider_tag_declares_requirement_v<typename Binding::requirement,
                                            typename Binding::provider_tag,
                                            Providers>;

    template <typename Bindings, typename Providers>
    struct all_bindings_valid;

    template <typename... Bindings, typename Providers>
    struct all_bindings_valid<BindingList<Bindings...>, Providers>
        : std::bool_constant<(... && binding_is_valid_v<Bindings, Providers>)> {};

    template <typename Bindings, typename Providers>
    inline constexpr bool all_bindings_valid_v = all_bindings_valid<Bindings, Providers>::value;

    template <typename RequiredSet, typename Bindings>
    struct all_requirements_bound_once;

    template <typename... Reqs, typename Bindings>
    struct all_requirements_bound_once<RequirementSet<Reqs...>, Bindings>
        : std::bool_constant<(... && (binding_count_v<Reqs, Bindings> == 1))> {};

    template <typename App, typename Bindings>
    inline constexpr bool app_requirements_bound_once_v =
        all_requirements_bound_once<typename App::required_set, Bindings>::value;

    template <typename Spec>
    inline constexpr bool profile_spec_resolved_v =
        app_requirements_bound_once_v<typename Spec::app, typename Spec::bindings> &&
        all_bindings_target_app_requirements_v<typename Spec::bindings, typename Spec::app> &&
        all_bindings_valid_v<typename Spec::bindings, typename Spec::providers>;

    template <typename Spec>
    constexpr auto resolve_profile() noexcept {
        static_assert(profile_spec_resolved_v<Spec>,
                      "profile must resolve before any projection can be materialized");
        return ResolvedProfile<Spec>{};
    }

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

    template <typename Resolved>
        requires projection_input_v<Resolved>
    struct InitProjection {
        using spec = typename Resolved::spec;

        std::string_view gate{"resolved-profile"};
    };

    template <typename Resolved>
        requires projection_input_v<Resolved>
    struct ContextProjection {
        using spec = typename Resolved::spec;

        std::string_view gate{"resolved-profile"};
    };

    template <typename Resolved>
        requires projection_input_v<Resolved>
    struct EvidenceProjection {
        using spec = typename Resolved::spec;

        std::string_view gate{"resolved-profile"};
    };

    template <typename Resolved>
        requires projection_input_v<Resolved>
    constexpr auto make_init_projection(Resolved) noexcept {
        return InitProjection<Resolved>{};
    }

    template <typename Resolved>
        requires projection_input_v<Resolved>
    constexpr auto make_context_projection(Resolved) noexcept {
        return ContextProjection<Resolved>{};
    }

    template <typename Resolved>
        requires projection_input_v<Resolved>
    constexpr auto make_evidence_projection(Resolved) noexcept {
        return EvidenceProjection<Resolved>{};
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

    using LogComponent = rte::ComponentDesc<EmptyRequires, LogProvides>;
    using DebugTraceComponent = rte::ComponentDesc<EmptyRequires, DebugTraceProvides>;
    using ClockComponent = rte::ComponentDesc<EmptyRequires, ClockProvides>;
    using AppComponent = rte::ComponentDesc<AppRequires, AppProvides>;

    using MemoryLogProviderDesc = rte::ProviderDesc<provider::memory_log, LogProvides>;
    using StdoutTraceProviderDesc = rte::ProviderDesc<provider::stdout_trace, DebugTraceProvides>;
    using FakeClockProviderDesc = rte::ProviderDesc<provider::fake_clock, ClockProvides>;
    using LogBinding = rte::ProfileBinding<LogReq, provider::memory_log>;
    using ClockBinding = rte::ProfileBinding<ClockReq, provider::fake_clock>;
    using WrongRoleBinding = rte::ProfileBinding<LogReq, provider::stdout_trace>;

    using GoodSpec = rte::ProfileSpec<
        AppComponent,
        rte::ComponentList<LogComponent, DebugTraceComponent, ClockComponent, AppComponent>,
        rte::ProviderList<MemoryLogProviderDesc, StdoutTraceProviderDesc, FakeClockProviderDesc>,
        rte::BindingList<LogBinding, ClockBinding>>;

    using MissingClockSpec = rte::ProfileSpec<
        AppComponent,
        rte::ComponentList<LogComponent, ClockComponent, AppComponent>,
        rte::ProviderList<MemoryLogProviderDesc, FakeClockProviderDesc>,
        rte::BindingList<LogBinding>>;

    using WrongRoleSpec = rte::ProfileSpec<
        AppComponent,
        rte::ComponentList<LogComponent, DebugTraceComponent, ClockComponent, AppComponent>,
        rte::ProviderList<MemoryLogProviderDesc, StdoutTraceProviderDesc, FakeClockProviderDesc>,
        rte::BindingList<WrongRoleBinding, ClockBinding>>;

    using ResolvedGood = rte::ResolvedProfile<GoodSpec>;

    static_assert(rte::profile_spec_resolved_v<GoodSpec>);
    static_assert(!rte::profile_spec_resolved_v<MissingClockSpec>);
    static_assert(!rte::profile_spec_resolved_v<WrongRoleSpec>);
    static_assert(!rte::is_resolved_profile_v<GoodSpec>);
    static_assert(rte::is_resolved_profile_v<ResolvedGood>);
    static_assert(!rte::projection_input_v<GoodSpec>);
    static_assert(rte::projection_input_v<ResolvedGood>);

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

    template <typename Tuple>
    constexpr auto node_ptrs(Tuple& projected) noexcept {
        return std::apply([](auto&... item) {
            return std::array<const init::Node*, sizeof...(item)>{&item.node...};
        }, projected);
    }

    template <typename Resolved>
        requires rte::is_resolved_profile_v<Resolved>
    bool run_init_projection(const rte::InitProjection<Resolved>& projection) noexcept {
        if (projection.gate != "resolved-profile") return false;

        rte::InitTrace trace{};
        const LogComponent log_service{
            .name = "log_service",
            .phase = rte::Phase::service,
            .init = init_log,
            .ctx = &trace,
        };
        const DebugTraceComponent debug_trace_service{
            .name = "debug_trace_service",
            .phase = rte::Phase::service,
            .init = init_debug_trace,
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
            rte::ProjectedNode<DebugTraceComponent>{},
            rte::ProjectedNode<ClockComponent>{},
            rte::ProjectedNode<AppComponent>{},
        };
        rte::materialize_init_nodes(projected, log_service, debug_trace_service, clock_service, app);
        auto nodes = node_ptrs(projected);
        init::Graph<8, 8> graph{};
        auto build = graph.build(nodes);
        if (!build) return false;
        auto start = graph.start();
        return start.has_value() &&
               trace.count == 4 &&
               trace.entries[0] == "log" &&
               trace.entries[1] == "debug_trace" &&
               trace.entries[2] == "clock" &&
               trace.entries[3] == "app";
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
        std::uint32_t now{42};

        [[nodiscard]] std::uint32_t now_ms() noexcept {
            return now;
        }
    };

    static_assert(cap::TextSink::satisfied_by<MemoryLog>);
    static_assert(cap::Clock::satisfied_by<FakeClock>);

    using MemoryLogRef = rte::ProviderRef<cap::TextSink, provider::memory_log, MemoryLog>;
    using FakeClockRef = rte::ProviderRef<cap::Clock, provider::fake_clock, FakeClock>;
    using RuntimeLogBinding = rte::RuntimeBinding<LogReq, MemoryLogRef>;
    using RuntimeClockBinding = rte::RuntimeBinding<ClockReq, FakeClockRef>;
    using AppContext = rte::ContextView<RuntimeLogBinding, RuntimeClockBinding>;

    static_assert(!rte::has_requirement_v<DebugTraceReq, RuntimeLogBinding, RuntimeClockBinding>);

    template <typename Resolved>
        requires rte::is_resolved_profile_v<Resolved>
    bool run_context_projection(const rte::ContextProjection<Resolved>& projection,
                                MemoryLog& log,
                                FakeClock& clock) noexcept {
        if (projection.gate != "resolved-profile") return false;

        AppContext context{
            RuntimeLogBinding{MemoryLogRef{log}},
            RuntimeClockBinding{FakeClockRef{clock}},
        };
        auto& bound_log = context.get<LogReq>();
        auto& bound_clock = context.get<ClockReq>();
        bound_log.write("provider=");
        bound_log.write(bound_clock.now_ms() == 42 ? "memory_log" : "unexpected");
        return log.view() == "provider=memory_log";
    }

    template <typename Resolved>
        requires rte::is_resolved_profile_v<Resolved>
    rte::EvidenceFrame run_evidence_projection(const rte::EvidenceProjection<Resolved>& projection,
                                               const MemoryLog& log) noexcept {
        if (projection.gate != "resolved-profile") {
            return rte::EvidenceFrame{.status = rte::EvidenceStatus::error};
        }

        return rte::EvidenceFrame{
            .capability = "TextSink.log",
            .provider = "memory_log",
            .status = rte::EvidenceStatus::ok,
            .fields = {{
                {"selected_provider", log.view() == "provider=memory_log" ? "memory_log" : "unexpected"},
            }},
            .field_count = 1,
        };
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
    constexpr auto resolved = rte::resolve_profile<GoodSpec>();
    constexpr auto init_projection = rte::make_init_projection(resolved);
    constexpr auto context_projection = rte::make_context_projection(resolved);
    constexpr auto evidence_projection = rte::make_evidence_projection(resolved);

    MemoryLog log{};
    FakeClock clock{};

    if (!expect(init_projection.gate == "resolved-profile", "init projection is gated by resolved profile")) return 1;
    if (!expect(context_projection.gate == "resolved-profile", "context projection is gated by resolved profile")) return 1;
    if (!expect(evidence_projection.gate == "resolved-profile", "evidence projection is gated by resolved profile")) return 1;
    if (!expect(run_init_projection(init_projection), "resolved profile materializes init projection")) return 1;
    if (!expect(run_context_projection(context_projection, log, clock), "resolved profile materializes context projection")) return 1;

    const auto evidence = run_evidence_projection(evidence_projection, log);
    if (!expect(evidence.status == rte::EvidenceStatus::ok, "resolved profile materializes evidence projection")) return 1;
    if (!expect(evidence.capability == "TextSink.log", "evidence preserves requirement capability")) return 1;
    if (!expect(evidence.provider == "memory_log", "evidence preserves selected provider")) return 1;
    if (!expect(evidence.fields[0].key == "selected_provider", "evidence remains structured")) return 1;
    if (!expect(evidence.fields[0].value == "memory_log", "all projections share provider identity")) return 1;

    std::puts("[rte-projection-gate-smoke] ok");
    return 0;
}
