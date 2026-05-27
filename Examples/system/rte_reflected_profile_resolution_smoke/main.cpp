#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <meta>
#include <string_view>
#include <tuple>
#include <type_traits>

#if !defined(__cpp_impl_reflection)
#error "__cpp_impl_reflection is required"
#endif

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
    struct spare_log {};
    struct stdout_trace {};
    struct fake_clock {};
    struct stale_log {};
}

namespace reflected_spec {
    struct demo_app {
        int requirements;
        int provides;
    };

    struct memory_log {
        int provides;
    };

    struct spare_log {
        int provides;
    };

    struct stdout_trace {
        int provides;
    };

    struct fake_clock {
        int provides;
    };

    struct stale_log {
        int provides;
    };

    struct profile {
        int bindings;
    };
}

namespace rte {
    template <auto SpecRef, auto KindRef, auto RoleRef>
    struct Requirement {
        static constexpr auto spec_ref = SpecRef;
        static constexpr auto kind_ref = KindRef;
        static constexpr auto role_ref = RoleRef;
        using kind = typename [:KindRef:];
        using role = typename [:RoleRef:];
    };

    template <auto SpecRef, auto KindRef, auto RoleRef>
    struct Provided {
        static constexpr auto spec_ref = SpecRef;
        static constexpr auto kind_ref = KindRef;
        static constexpr auto role_ref = RoleRef;
        using kind = typename [:KindRef:];
        using role = typename [:RoleRef:];
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

    template <auto SpecRef, auto KindRef, auto RoleRef>
    struct ProvidedFor<Requirement<SpecRef, KindRef, RoleRef>> {
        using type = Provided<SpecRef, KindRef, RoleRef>;
    };

    template <typename Req, typename Prov>
    struct provided_satisfies_requirement : std::false_type {};

    template <auto ReqSpecRef, auto ReqKindRef, auto ReqRoleRef,
              auto ProvSpecRef, auto ProvKindRef, auto ProvRoleRef>
    struct provided_satisfies_requirement<Requirement<ReqSpecRef, ReqKindRef, ReqRoleRef>,
                                          Provided<ProvSpecRef, ProvKindRef, ProvRoleRef>>
        : std::bool_constant<ReqKindRef == ProvKindRef && ReqRoleRef == ProvRoleRef> {};

    template <typename Req, typename Prov>
    inline constexpr bool provided_satisfies_requirement_v =
        provided_satisfies_requirement<Req, Prov>::value;

    template <auto SpecRef, typename Requires, typename Provides>
    struct ComponentDesc {
        static constexpr auto spec_ref = SpecRef;
        using required_set = Requires;
        using provided_set = Provides;
    };

    template <auto SpecRef, typename Provides>
    struct ProviderDesc {
        static constexpr auto spec_ref = SpecRef;
        using provider_tag = typename [:SpecRef:];
        using provided_set = Provides;
    };

    template <typename Req, auto ProviderRef>
    struct ProfileBinding {
        using requirement = Req;
        using provider_tag = typename [:ProviderRef:];
        static constexpr auto provider_ref = ProviderRef;
    };

    template <typename... Providers>
    struct ProviderList {};

    template <typename... Bindings>
    struct BindingList {};

    template <auto SpecRef, typename Component, typename Providers, typename Bindings>
    struct ProfileResolution {
        static constexpr auto spec_ref = SpecRef;
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

    template <typename ProviderTag, typename Providers>
    struct provider_count;

    template <typename ProviderTag, typename... Providers>
    struct provider_count<ProviderTag, ProviderList<Providers...>>
        : std::integral_constant<std::size_t,
              (std::size_t{0} + ... + (std::same_as<ProviderTag, typename Providers::provider_tag> ? 1u : 0u))> {};

    template <typename ProviderTag, typename Providers>
    inline constexpr std::size_t provider_count_v = provider_count<ProviderTag, Providers>::value;

    template <typename Req, typename ProvidedSet>
    struct provided_set_satisfies_requirement;

    template <typename Req, typename... Provs>
    struct provided_set_satisfies_requirement<Req, ProviderSet<Provs...>>
        : std::bool_constant<(... || provided_satisfies_requirement_v<Req, Provs>)> {};

    template <typename Req, typename ProvidedSet>
    inline constexpr bool provided_set_satisfies_requirement_v =
        provided_set_satisfies_requirement<Req, ProvidedSet>::value;

    template <typename Req, typename ProviderTag, typename Providers>
    struct provider_tag_declares_requirement;

    template <typename Req, typename ProviderTag, typename... Providers>
    struct provider_tag_declares_requirement<Req, ProviderTag, ProviderList<Providers...>>
        : std::bool_constant<(... || (std::same_as<ProviderTag, typename Providers::provider_tag> &&
                                      provided_set_satisfies_requirement_v<Req, typename Providers::provided_set>))> {};

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

    template <typename Binding, typename Component>
    inline constexpr bool binding_targets_component_requirement_v =
        set_contains_v<typename Binding::requirement, typename Component::required_set>;

    template <typename Bindings, typename Component>
    struct all_bindings_target_component;

    template <typename... Bindings, typename Component>
    struct all_bindings_target_component<BindingList<Bindings...>, Component>
        : std::bool_constant<(... && binding_targets_component_requirement_v<Bindings, Component>)> {};

    template <typename Bindings, typename Component>
    inline constexpr bool all_bindings_target_component_v =
        all_bindings_target_component<Bindings, Component>::value;

    template <typename ProviderTag, typename Rest>
    struct provider_tag_unique_from_rest;

    template <typename ProviderTag, typename... Rest>
    struct provider_tag_unique_from_rest<ProviderTag, ProviderList<Rest...>>
        : std::bool_constant<(... && !std::same_as<ProviderTag, typename Rest::provider_tag>)> {};

    template <typename Providers>
    struct provider_tags_unique;

    template <>
    struct provider_tags_unique<ProviderList<>> : std::true_type {};

    template <typename First, typename... Rest>
    struct provider_tags_unique<ProviderList<First, Rest...>>
        : std::bool_constant<provider_tag_unique_from_rest<typename First::provider_tag,
                                                           ProviderList<Rest...>>::value &&
                             provider_tags_unique<ProviderList<Rest...>>::value> {};

    template <typename Providers>
    inline constexpr bool provider_tags_unique_v = provider_tags_unique<Providers>::value;

    template <typename Left, typename Right>
    struct provided_tokens_equivalent : std::false_type {};

    template <auto LeftSpecRef, auto LeftKindRef, auto LeftRoleRef,
              auto RightSpecRef, auto RightKindRef, auto RightRoleRef>
    struct provided_tokens_equivalent<Provided<LeftSpecRef, LeftKindRef, LeftRoleRef>,
                                      Provided<RightSpecRef, RightKindRef, RightRoleRef>>
        : std::bool_constant<LeftKindRef == RightKindRef && LeftRoleRef == RightRoleRef> {};

    template <typename Left, typename Right>
    inline constexpr bool provided_tokens_equivalent_v =
        provided_tokens_equivalent<Left, Right>::value;

    template <typename Prov, typename Provider>
    struct provider_declares_token;

    template <typename Prov, typename... ProviderProvs>
    struct provider_declares_token<Prov, ProviderSet<ProviderProvs...>>
        : std::bool_constant<(... || provided_tokens_equivalent_v<Prov, ProviderProvs>)> {};

    template <typename Prov, typename Provider>
    inline constexpr bool provider_declares_token_v =
        provider_declares_token<Prov, typename Provider::provided_set>::value;

    template <typename Prov, typename... RestProviders>
    inline constexpr bool token_unique_from_rest_v =
        (... && !provider_declares_token_v<Prov, RestProviders>);

    template <typename ProvidedSet, typename RestProviders>
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
        all_bindings_target_component_v<typename Resolution::bindings,
                                        typename Resolution::component> &&
        all_bindings_valid_v<typename Resolution::bindings,
                             typename Resolution::providers>;

    template <typename T>
    consteval bool reflected_shape_has_fields(auto spec_ref, std::initializer_list<std::string_view> names) {
        (void)sizeof(T);
        const auto fields = std::meta::nonstatic_data_members_of(spec_ref, std::meta::access_context::unchecked());
        if (fields.size() != names.size()) {
            return false;
        }

        std::size_t index = 0;
        for (const auto name : names) {
            if (std::meta::identifier_of(fields[index]) != name) {
                return false;
            }
            ++index;
        }
        return true;
    }

    template <typename Component>
    consteval bool component_shape_ok() {
        return reflected_shape_has_fields<Component>(Component::spec_ref, {"requirements", "provides"});
    }

    template <typename Provider>
    consteval bool provider_shape_ok() {
        return reflected_shape_has_fields<Provider>(Provider::spec_ref, {"provides"});
    }

    template <typename Resolution>
    consteval bool profile_shape_ok() {
        return reflected_shape_has_fields<Resolution>(Resolution::spec_ref, {"bindings"});
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
    using LogReq = rte::Requirement<^^reflected_spec::demo_app, ^^cap::TextSink, ^^role::log>;
    using ClockReq = rte::Requirement<^^reflected_spec::demo_app, ^^cap::Clock, ^^role::monotonic_time>;
    using DebugTraceReq = rte::Requirement<^^reflected_spec::demo_app, ^^cap::TextSink, ^^role::debug_trace>;
    using AppReq = rte::Requirement<^^reflected_spec::demo_app, ^^cap::App, ^^role::main_app>;

    using LogProv = rte::Provided<^^reflected_spec::memory_log, ^^cap::TextSink, ^^role::log>;
    using SpareLogProv = rte::Provided<^^reflected_spec::spare_log, ^^cap::TextSink, ^^role::log>;
    using TraceProv = rte::Provided<^^reflected_spec::stdout_trace, ^^cap::TextSink, ^^role::debug_trace>;
    using ClockProv = rte::Provided<^^reflected_spec::fake_clock, ^^cap::Clock, ^^role::monotonic_time>;

    using AppRequires = rte::RequirementSet<LogReq, ClockReq>;
    using AppProvides = rte::ProviderSet<>;
    using LogProvides = rte::ProviderSet<LogProv>;
    using SpareLogProvides = rte::ProviderSet<SpareLogProv>;
    using TraceProvides = rte::ProviderSet<TraceProv>;
    using ClockProvides = rte::ProviderSet<ClockProv>;

    using AppComponent = rte::ComponentDesc<^^reflected_spec::demo_app, AppRequires, AppProvides>;
    using MemoryLogProviderDesc = rte::ProviderDesc<^^reflected_spec::memory_log, LogProvides>;
    using SpareLogProviderDesc = rte::ProviderDesc<^^reflected_spec::spare_log, SpareLogProvides>;
    using StdoutTraceProviderDesc = rte::ProviderDesc<^^reflected_spec::stdout_trace, TraceProvides>;
    using FakeClockProviderDesc = rte::ProviderDesc<^^reflected_spec::fake_clock, ClockProvides>;

    using LogBinding = rte::ProfileBinding<LogReq, ^^reflected_spec::memory_log>;
    using ClockBinding = rte::ProfileBinding<ClockReq, ^^reflected_spec::fake_clock>;
    using DebugTraceBinding = rte::ProfileBinding<DebugTraceReq, ^^reflected_spec::stdout_trace>;
    using WrongRoleBinding = rte::ProfileBinding<LogReq, ^^reflected_spec::stdout_trace>;
    using DuplicateLogBinding = rte::ProfileBinding<LogReq, ^^reflected_spec::spare_log>;
    using StaleLogBinding = rte::ProfileBinding<LogReq, ^^reflected_spec::stale_log>;

    using GoodProfile = rte::ProfileResolution<
        ^^reflected_spec::profile,
        AppComponent,
        rte::ProviderList<MemoryLogProviderDesc, StdoutTraceProviderDesc, FakeClockProviderDesc>,
        rte::BindingList<LogBinding, ClockBinding>>;

    using MissingClockProfile = rte::ProfileResolution<
        ^^reflected_spec::profile,
        AppComponent,
        rte::ProviderList<MemoryLogProviderDesc, FakeClockProviderDesc>,
        rte::BindingList<LogBinding>>;

    using DuplicateBindingProfile = rte::ProfileResolution<
        ^^reflected_spec::profile,
        AppComponent,
        rte::ProviderList<MemoryLogProviderDesc, SpareLogProviderDesc, FakeClockProviderDesc>,
        rte::BindingList<LogBinding, DuplicateLogBinding, ClockBinding>>;

    using WrongRoleProfile = rte::ProfileResolution<
        ^^reflected_spec::profile,
        AppComponent,
        rte::ProviderList<MemoryLogProviderDesc, StdoutTraceProviderDesc, FakeClockProviderDesc>,
        rte::BindingList<WrongRoleBinding, ClockBinding>>;

    using DuplicateProviderTokenProfile = rte::ProfileResolution<
        ^^reflected_spec::profile,
        AppComponent,
        rte::ProviderList<MemoryLogProviderDesc, SpareLogProviderDesc, FakeClockProviderDesc>,
        rte::BindingList<LogBinding, ClockBinding>>;

    using StaleBindingProfile = rte::ProfileResolution<
        ^^reflected_spec::profile,
        AppComponent,
        rte::ProviderList<MemoryLogProviderDesc, FakeClockProviderDesc>,
        rte::BindingList<StaleLogBinding, ClockBinding>>;

    using ExtraBindingProfile = rte::ProfileResolution<
        ^^reflected_spec::profile,
        AppComponent,
        rte::ProviderList<MemoryLogProviderDesc, StdoutTraceProviderDesc, FakeClockProviderDesc>,
        rte::BindingList<LogBinding, ClockBinding, DebugTraceBinding>>;

    static_assert(std::meta::identifier_of(AppComponent::spec_ref) == std::string_view{"demo_app"});
    static_assert(rte::component_shape_ok<AppComponent>());
    static_assert(rte::provider_shape_ok<MemoryLogProviderDesc>());
    static_assert(rte::provider_shape_ok<FakeClockProviderDesc>());
    static_assert(rte::profile_shape_ok<GoodProfile>());
    static_assert(rte::profile_resolved_v<GoodProfile>);
    static_assert(!rte::profile_resolved_v<MissingClockProfile>);
    static_assert(!rte::profile_resolved_v<DuplicateBindingProfile>);
    static_assert(!rte::profile_resolved_v<WrongRoleProfile>);
    static_assert(!rte::profile_resolved_v<DuplicateProviderTokenProfile>);
    static_assert(!rte::profile_resolved_v<StaleBindingProfile>);
    static_assert(!rte::profile_resolved_v<ExtraBindingProfile>);

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
    };

    static_assert(cap::TextSink::satisfied_by<MemoryLog>);
    static_assert(cap::TextSink::satisfied_by<TraceSink>);
    static_assert(cap::Clock::satisfied_by<FakeClock>);

    using MemoryLogRef = rte::ProviderRef<cap::TextSink, provider::memory_log, MemoryLog>;
    using TraceRef = rte::ProviderRef<cap::TextSink, provider::stdout_trace, TraceSink>;
    using FakeClockRef = rte::ProviderRef<cap::Clock, provider::fake_clock, FakeClock>;
    using RuntimeLogBinding = rte::RuntimeBinding<LogReq, MemoryLogRef>;
    using RuntimeClockBinding = rte::RuntimeBinding<ClockReq, FakeClockRef>;
    using AppContext = rte::ContextView<RuntimeLogBinding, RuntimeClockBinding>;

    static_assert(!rte::has_requirement_v<DebugTraceReq, RuntimeLogBinding, RuntimeClockBinding>);
    static_assert(!rte::has_requirement_v<AppReq, RuntimeLogBinding, RuntimeClockBinding>);

    void app_tick(AppContext& context) noexcept {
        auto& log = context.get<LogReq>();
        auto& clock = context.get<ClockReq>();
        log.write("tick=");
        log.write(clock.now_ms() == 42 ? "42" : "unexpected");
    }

    [[nodiscard]] bool expect(bool condition, const char* message) noexcept {
        if (!condition) {
            std::printf("[ERR] %s\n", message);
            return false;
        }
        return true;
    }
}

int main() {
    MemoryLog log{};
    TraceSink trace{};
    FakeClock clock{};

    [[maybe_unused]] TraceRef unbound_trace{trace};
    AppContext context{
        RuntimeLogBinding{MemoryLogRef{log}},
        RuntimeClockBinding{FakeClockRef{clock}},
    };

    app_tick(context);

    if (!expect(log.view() == "tick=42", "accepted reflected profile materializes ContextView")) return 1;
    if (!expect(log.writes == 2, "bound provider receives app writes")) return 1;
    if (!expect(trace.writes == 0, "unbound provider is not implicitly selected")) return 1;

    std::puts("[rte-reflected-profile-resolution-smoke] ok");
    return 0;
}
