#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
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
    struct stdout_trace {};
    struct fake_clock {};
}

namespace reflected_spec {
    struct log_service {
        int requirements;
        int provides;
    };

    struct clock_service {
        int requirements;
        int provides;
    };

    struct demo_app {
        int requirements;
        int provides;
    };
}

namespace rte {
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

    template <auto SpecRef, typename Requires, typename Provides>
    struct ComponentDesc {
        static constexpr auto spec_ref = SpecRef;
        using spec_type = typename [:SpecRef:];
        using required_set = Requires;
        using provided_set = Provides;
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

    template <typename Component>
    consteval bool spec_shape_ok() {
        const auto fields = std::meta::nonstatic_data_members_of(Component::spec_ref,
                                                                 std::meta::access_context::unchecked());
        return fields.size() == 2 &&
               std::meta::identifier_of(fields[0]) == std::string_view{"requirements"} &&
               std::meta::identifier_of(fields[1]) == std::string_view{"provides"};
    }

    template <typename Component>
    consteval std::string_view reflected_component_name() {
        return std::meta::identifier_of(Component::spec_ref);
    }

    template <typename Component, typename Req>
    [[nodiscard]] constexpr EvidenceFrame<3> evidence_for(std::string_view provider_name) noexcept {
        return EvidenceFrame<3>{
            .component = reflected_component_name<Component>(),
            .capability = requirement_cap_name<Req>(),
            .provider = provider_name,
            .status = EvidenceStatus::ok,
            .fields = {{
                {"component", reflected_component_name<Component>()},
                {"capability", requirement_cap_name<Req>()},
                {"source", "reflected-spec"},
            }},
        };
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
    using LogReq = rte::Requirement<^^reflected_spec::log_service, ^^cap::TextSink, ^^role::log>;
    using DebugTraceReq = rte::Requirement<^^reflected_spec::log_service, ^^cap::TextSink, ^^role::debug_trace>;
    using ClockReq = rte::Requirement<^^reflected_spec::clock_service, ^^cap::Clock, ^^role::monotonic_time>;
    using AppReq = rte::Requirement<^^reflected_spec::demo_app, ^^cap::App, ^^role::main_app>;
    using LogProv = rte::Provided<^^reflected_spec::log_service, ^^cap::TextSink, ^^role::log>;
    using ClockProv = rte::Provided<^^reflected_spec::clock_service, ^^cap::Clock, ^^role::monotonic_time>;
    using AppProv = rte::Provided<^^reflected_spec::demo_app, ^^cap::App, ^^role::main_app>;

    using EmptyRequires = rte::RequirementSet<>;
    using DemoRequires = rte::RequirementSet<LogReq, ClockReq>;
    using LogProvides = rte::ProviderSet<LogProv>;
    using ClockProvides = rte::ProviderSet<ClockProv>;
    using AppProvides = rte::ProviderSet<AppProv>;

    using LogService = rte::ComponentDesc<^^reflected_spec::log_service, EmptyRequires, LogProvides>;
    using ClockService = rte::ComponentDesc<^^reflected_spec::clock_service, EmptyRequires, ClockProvides>;
    using DemoApp = rte::ComponentDesc<^^reflected_spec::demo_app, DemoRequires, AppProvides>;

    static_assert(std::meta::is_type(LogService::spec_ref));
    static_assert(std::meta::identifier_of(LogService::spec_ref) == std::string_view{"log_service"});
    static_assert(rte::spec_shape_ok<LogService>());
    static_assert(rte::spec_shape_ok<ClockService>());
    static_assert(rte::spec_shape_ok<DemoApp>());
    static_assert(rte::requirements_satisfied_by_v<DemoApp, LogService, ClockService>);
    static_assert(!rte::requirements_satisfied_by_v<DemoApp, ClockService>);

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

    using MemoryLogProvider = rte::ProviderRef<cap::TextSink, provider::memory_log, MemoryLog>;
    using TraceProvider = rte::ProviderRef<cap::TextSink, provider::stdout_trace, TraceSink>;
    using FakeClockProvider = rte::ProviderRef<cap::Clock, provider::fake_clock, FakeClock>;
    using LogBinding = rte::ProfileBinding<LogReq, MemoryLogProvider>;
    using ClockBinding = rte::ProfileBinding<ClockReq, FakeClockProvider>;
    using DemoContext = rte::ContextView<LogBinding, ClockBinding>;

    static_assert(!rte::has_requirement_v<DebugTraceReq, LogBinding, ClockBinding>);
    static_assert(!rte::has_requirement_v<AppReq, LogBinding, ClockBinding>);

    template <typename Context>
    void demo_tick(Context& context) noexcept {
        auto& log = context.template get<LogReq>();
        auto& clock = context.template get<ClockReq>();
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

    [[nodiscard]] bool evidence_ok(const rte::EvidenceFrame<3>& frame) noexcept {
        return frame.component == "log_service" &&
               frame.capability == "TextSink.log" &&
               frame.provider == "memory_log" &&
               frame.status == rte::EvidenceStatus::ok &&
               frame.fields[0].key == "component" &&
               frame.fields[0].value == "log_service" &&
               frame.fields[1].key == "capability" &&
               frame.fields[1].value == "TextSink.log" &&
               frame.fields[2].key == "source" &&
               frame.fields[2].value == "reflected-spec";
    }
}

int main() {
    MemoryLog memory_log{};
    TraceSink trace{};
    FakeClock clock{};

    [[maybe_unused]] TraceProvider unbound_trace{trace};
    DemoContext context{
        LogBinding{MemoryLogProvider{memory_log}},
        ClockBinding{FakeClockProvider{clock}},
    };

    demo_tick(context);
    const auto evidence = rte::evidence_for<LogService, LogReq>("memory_log");

    if (!expect(memory_log.view() == "tick=42", "reflected spec materializes ContextView")) return 1;
    if (!expect(memory_log.writes == 2, "bound provider receives app writes")) return 1;
    if (!expect(trace.writes == 0, "unbound provider is not implicitly selected")) return 1;
    if (!expect(evidence_ok(evidence), "reflected component name feeds structured evidence")) return 1;

    std::puts("[rte-reflected-context-smoke] ok");
    return 0;
}
