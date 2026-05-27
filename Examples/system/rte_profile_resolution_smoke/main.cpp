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
    struct spare_log {};
    struct stale_log {};
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
    struct ComponentSet {};

    template <typename... Providers>
    struct ProviderList {};

    template <typename... Bindings>
    struct BindingList {};

    template <typename Component, typename Providers, typename Bindings>
    struct ProfileResolution {
        using component = Component;
        using providers = Providers;
        using bindings = Bindings;
    };

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

    template <typename Binding, typename Component>
    inline constexpr bool binding_targets_component_requirement_v =
        set_contains_v<typename Binding::requirement, typename Component::required_set>;

    template <typename Bindings, typename Component>
    struct all_bindings_target_component_requirements;

    template <typename... Bindings, typename Component>
    struct all_bindings_target_component_requirements<BindingList<Bindings...>, Component>
        : std::bool_constant<(... && binding_targets_component_requirement_v<Bindings, Component>)> {};

    template <typename Bindings, typename Component>
    inline constexpr bool all_bindings_target_component_requirements_v =
        all_bindings_target_component_requirements<Bindings, Component>::value;

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

    template <typename Component, typename Bindings>
    inline constexpr bool component_requirements_bound_once_v =
        all_requirements_bound_once<typename Component::required_set, Bindings>::value;

    template <typename ProviderTag, typename Provider>
    inline constexpr bool provider_has_tag_v = std::same_as<ProviderTag, typename Provider::tag>;

    template <typename First, typename... Rest>
    inline constexpr bool provider_tag_unique_from_rest_v =
        (... && !provider_has_tag_v<typename First::tag, Rest>);

    template <typename Providers>
    struct provider_tags_unique;

    template <>
    struct provider_tags_unique<ProviderList<>> : std::true_type {};

    template <typename First, typename... Rest>
    struct provider_tags_unique<ProviderList<First, Rest...>>
        : std::bool_constant<provider_tag_unique_from_rest_v<First, Rest...> &&
                             provider_tags_unique<ProviderList<Rest...>>::value> {};

    template <typename Providers>
    inline constexpr bool provider_tags_unique_v = provider_tags_unique<Providers>::value;

    template <typename Prov, typename Provider>
    inline constexpr bool provider_declares_token_v =
        set_contains_v<Prov, typename Provider::provided_set>;

    template <typename Prov, typename... Providers>
    inline constexpr bool token_unique_from_rest_v =
        (... && !provider_declares_token_v<Prov, Providers>);

    template <typename Provides, typename RestProviders>
    struct provider_tokens_unique_from_rest;

    template <typename... Provs, typename... RestProviders>
    struct provider_tokens_unique_from_rest<ProviderSet<Provs...>, ProviderList<RestProviders...>>
        : std::bool_constant<(... && token_unique_from_rest_v<Provs, RestProviders...>)> {};

    template <typename Providers>
    struct provider_tokens_unique;

    template <>
    struct provider_tokens_unique<ProviderList<>> : std::true_type {};

    template <typename First, typename... Rest>
    struct provider_tokens_unique<ProviderList<First, Rest...>>
        : std::bool_constant<provider_tokens_unique_from_rest<typename First::provided_set,
                                                              ProviderList<Rest...>>::value &&
                             provider_tokens_unique<ProviderList<Rest...>>::value> {};

    template <typename Providers>
    inline constexpr bool provider_tokens_unique_v = provider_tokens_unique<Providers>::value;

    template <typename Resolution>
    inline constexpr bool profile_resolved_v =
        provider_tags_unique_v<typename Resolution::providers> &&
        provider_tokens_unique_v<typename Resolution::providers> &&
        component_requirements_bound_once_v<typename Resolution::component,
                                            typename Resolution::bindings> &&
        all_bindings_target_component_requirements_v<typename Resolution::bindings,
                                                     typename Resolution::component> &&
        all_bindings_valid_v<typename Resolution::bindings,
                             typename Resolution::providers>;

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
    using SpareLogProviderDesc = rte::ProviderDesc<provider::spare_log, LogProvides>;
    using StaleLogProviderDesc = rte::ProviderDesc<provider::stale_log, LogProvides>;

    using LogBinding = rte::ProfileBinding<LogReq, provider::memory_log>;
    using ClockBinding = rte::ProfileBinding<ClockReq, provider::fake_clock>;
    using DebugTraceBinding = rte::ProfileBinding<DebugTraceReq, provider::stdout_trace>;
    using WrongRoleBinding = rte::ProfileBinding<LogReq, provider::stdout_trace>;
    using DuplicateLogBinding = rte::ProfileBinding<LogReq, provider::spare_log>;
    using StaleLogBinding = rte::ProfileBinding<LogReq, provider::stale_log>;

    using GoodProfile = rte::ProfileResolution<
        AppComponent,
        rte::ProviderList<MemoryLogProviderDesc, StdoutTraceProviderDesc, FakeClockProviderDesc>,
        rte::BindingList<LogBinding, ClockBinding>>;

    using MissingClockProfile = rte::ProfileResolution<
        AppComponent,
        rte::ProviderList<MemoryLogProviderDesc, FakeClockProviderDesc>,
        rte::BindingList<LogBinding>>;

    using DuplicateBindingProfile = rte::ProfileResolution<
        AppComponent,
        rte::ProviderList<MemoryLogProviderDesc, SpareLogProviderDesc, FakeClockProviderDesc>,
        rte::BindingList<LogBinding, DuplicateLogBinding, ClockBinding>>;

    using WrongRoleProfile = rte::ProfileResolution<
        AppComponent,
        rte::ProviderList<MemoryLogProviderDesc, StdoutTraceProviderDesc, FakeClockProviderDesc>,
        rte::BindingList<WrongRoleBinding, ClockBinding>>;

    using DuplicateProviderTokenProfile = rte::ProfileResolution<
        AppComponent,
        rte::ProviderList<MemoryLogProviderDesc, SpareLogProviderDesc, FakeClockProviderDesc>,
        rte::BindingList<LogBinding, ClockBinding>>;

    using StaleBindingProfile = rte::ProfileResolution<
        AppComponent,
        rte::ProviderList<MemoryLogProviderDesc, FakeClockProviderDesc>,
        rte::BindingList<StaleLogBinding, ClockBinding>>;

    using ExtraBindingProfile = rte::ProfileResolution<
        AppComponent,
        rte::ProviderList<MemoryLogProviderDesc, StdoutTraceProviderDesc, FakeClockProviderDesc>,
        rte::BindingList<LogBinding, ClockBinding, DebugTraceBinding>>;

    static_assert(rte::profile_resolved_v<GoodProfile>);
    static_assert(!rte::profile_resolved_v<MissingClockProfile>);
    static_assert(!rte::profile_resolved_v<DuplicateBindingProfile>);
    static_assert(!rte::profile_resolved_v<WrongRoleProfile>);
    static_assert(!rte::profile_resolved_v<DuplicateProviderTokenProfile>);
    static_assert(!rte::profile_resolved_v<StaleBindingProfile>);
    static_assert(!rte::profile_resolved_v<ExtraBindingProfile>);
    static_assert(!rte::has_requirement_v<DebugTraceReq,
                                          rte::RuntimeBinding<LogReq, rte::ProviderRef<cap::TextSink, provider::memory_log, struct MemoryLog>>,
                                          rte::RuntimeBinding<ClockReq, rte::ProviderRef<cap::Clock, provider::fake_clock, struct FakeClock>>>);

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

        [[nodiscard]] rte::EvidenceFrame evidence() const noexcept {
            return rte::EvidenceFrame{
                .capability = "TextSink.log",
                .provider = "memory_log",
                .status = rte::EvidenceStatus::ok,
                .fields = {{
                    {"writes", writes == 2 ? "2" : "unexpected"},
                    {"buffer", view() == "tick=42" ? "tick=42" : "unexpected"},
                }},
                .field_count = 2,
            };
        }
    };

    struct TraceSink {
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

        [[nodiscard]] rte::EvidenceFrame evidence() const noexcept {
            return rte::EvidenceFrame{
                .capability = "Clock.monotonic_time",
                .provider = "fake_clock",
                .status = rte::EvidenceStatus::ok,
                .fields = {{
                    {"now_ms", now == 42 ? "42" : "unexpected"},
                }},
                .field_count = 1,
            };
        }
    };

    static_assert(cap::TextSink::satisfied_by<MemoryLog>);
    static_assert(cap::TextSink::satisfied_by<TraceSink>);
    static_assert(cap::Clock::satisfied_by<FakeClock>);

    using MemoryLogRef = rte::ProviderRef<cap::TextSink, provider::memory_log, MemoryLog>;
    using FakeClockRef = rte::ProviderRef<cap::Clock, provider::fake_clock, FakeClock>;
    using RuntimeLogBinding = rte::RuntimeBinding<LogReq, MemoryLogRef>;
    using RuntimeClockBinding = rte::RuntimeBinding<ClockReq, FakeClockRef>;
    using AppContext = rte::ContextView<RuntimeLogBinding, RuntimeClockBinding>;

    static_assert(!rte::has_requirement_v<DebugTraceReq, RuntimeLogBinding, RuntimeClockBinding>);

    void app_tick(AppContext& context) noexcept {
        auto& log = context.get<LogReq>();
        auto& clock = context.get<ClockReq>();
        log.write("tick=");
        log.write(clock.now_ms() == 42 ? "42" : "unexpected");
    }

    bool expect(bool condition, const char* message) noexcept {
        if (!condition) {
            std::printf("[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    bool run_init_projection_from_resolved_profile() noexcept {
        static_assert(rte::profile_resolved_v<GoodProfile>);

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

        auto projected = rte::project_to_init_nodes(log_service, debug_trace_service, clock_service, app);
        rte::materialize_init_nodes(projected, log_service, debug_trace_service, clock_service, app);
        auto nodes = node_ptrs(projected);
        init::Graph<8, 8> graph{};
        auto build = graph.build(nodes);
        if (!expect(build.has_value(), "resolved profile projects to init graph")) return false;
        auto start = graph.start();
        if (!expect(start.has_value(), "resolved profile init graph starts")) return false;
        if (!expect(trace.count == 4, "resolved profile starts selected components")) return false;
        if (!expect(trace.entries[0] == "log", "log provider initializes first")) return false;
        if (!expect(trace.entries[1] == "debug_trace", "debug trace provider remains independent")) return false;
        if (!expect(trace.entries[2] == "clock", "clock provider initializes before app")) return false;
        if (!expect(trace.entries[3] == "app", "app initializes after bound requirements")) return false;
        return true;
    }

    bool run_context_and_evidence_from_resolved_profile() noexcept {
        MemoryLog log{};
        TraceSink trace{};
        FakeClock clock{};

        AppContext context{
            RuntimeLogBinding{MemoryLogRef{log}},
            RuntimeClockBinding{FakeClockRef{clock}},
        };

        app_tick(context);

        const auto log_evidence = log.evidence();
        const auto clock_evidence = clock.evidence();
        if (!expect(log.view() == "tick=42", "resolved binding selects memory log")) return false;
        if (!expect(log.writes == 2, "resolved log binding receives app writes")) return false;
        if (!expect(trace.writes == 0, "unbound TextSink provider is not implicitly selected")) return false;
        if (!expect(log_evidence.capability == "TextSink.log", "log evidence keeps bound requirement role")) return false;
        if (!expect(log_evidence.provider == "memory_log", "log evidence keeps selected provider")) return false;
        if (!expect(log_evidence.fields[1].value == "tick=42", "log evidence remains structured")) return false;
        if (!expect(clock_evidence.capability == "Clock.monotonic_time", "clock evidence keeps requirement role")) return false;
        if (!expect(clock_evidence.provider == "fake_clock", "clock evidence keeps selected provider")) return false;
        if (!expect(clock_evidence.fields[0].value == "42", "clock evidence remains structured")) return false;
        return true;
    }
}

int main() {
    if (!run_init_projection_from_resolved_profile()) return 1;
    if (!run_context_and_evidence_from_resolved_profile()) return 1;

    std::puts("[rte-profile-resolution-smoke] ok");
    return 0;
}
