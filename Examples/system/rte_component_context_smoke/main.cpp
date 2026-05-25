#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <tuple>
#include <type_traits>

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
    struct host_stdout_log {};
    struct memory_log {};
    struct fake_clock {};
}

namespace rte {
    enum class Phase : std::uint8_t {
        service,
        app,
    };

    enum class EvidenceStatus : std::uint8_t {
        ok,
        error,
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

    template <typename Kind, typename ProviderTag, typename Impl>
    struct ProviderRef {
        using kind = Kind;
        using provider = ProviderTag;
        using impl_type = Impl;

        Impl* impl{nullptr};

        constexpr explicit ProviderRef(Impl& value) noexcept : impl(&value) {
            static_assert(Kind::template satisfied_by<Impl>, "provider implementation does not satisfy capability kind");
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

    struct EvidenceField {
        std::string_view key{};
        std::string_view value{};
    };

    template <std::size_t FieldCount>
    struct EvidenceFrame {
        std::string_view capability{};
        std::string_view provider{};
        EvidenceStatus status{EvidenceStatus::ok};
        std::array<EvidenceField, FieldCount> fields{};
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
        Phase phase{Phase::app};
        Requires required{};
        Provides provided{};
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
}

namespace {
    using LogReq = rte::Requirement<cap::TextSink, role::log>;
    using DebugTraceReq = rte::Requirement<cap::TextSink, role::debug_trace>;
    using ClockReq = rte::Requirement<cap::Clock, role::monotonic_time>;
    using LogProv = rte::Provided<cap::TextSink, role::log>;
    using ClockProv = rte::Provided<cap::Clock, role::monotonic_time>;
    using AppProv = rte::Provided<cap::App, role::main_app>;
    using DemoRequires = rte::RequirementSet<LogReq, ClockReq>;
    using EmptyRequires = rte::RequirementSet<>;
    using LogProvides = rte::ProviderSet<LogProv>;
    using ClockProvides = rte::ProviderSet<ClockProv>;
    using AppProvides = rte::ProviderSet<AppProv>;

    using LogServiceComponent = rte::ComponentDesc<EmptyRequires, LogProvides>;
    using ClockServiceComponent = rte::ComponentDesc<EmptyRequires, ClockProvides>;
    using DemoComponent = rte::ComponentDesc<DemoRequires, AppProvides>;

    constexpr LogServiceComponent kLogServiceComponent{
        .name = "rte.log_service",
        .phase = rte::Phase::service,
    };

    constexpr ClockServiceComponent kClockServiceComponent{
        .name = "rte.clock_service",
        .phase = rte::Phase::service,
    };

    constexpr DemoComponent kDemoComponent{
        .name = "rte.demo_app",
        .phase = rte::Phase::app,
    };

    static_assert(rte::requirements_satisfied_by_v<DemoComponent,
                                                   LogServiceComponent,
                                                   ClockServiceComponent>);
    static_assert(!rte::requirements_satisfied_by_v<DemoComponent, LogServiceComponent>);

    constexpr std::array<std::string_view, 3> kInitProjection{
        kLogServiceComponent.name,
        kClockServiceComponent.name,
        kDemoComponent.name,
    };

    struct MemoryLog {
        std::array<char, 128> bytes{};
        std::size_t used{0};
        std::uint32_t writes{0};

        void write(std::string_view text) noexcept {
            const auto remaining = bytes.size() - used;
            const auto count = (text.size() < remaining) ? text.size() : remaining;
            if (count != 0) {
                std::memcpy(bytes.data() + used, text.data(), count);
                used += count;
            }
            ++writes;
        }

        [[nodiscard]] std::string_view view() const noexcept {
            return {bytes.data(), used};
        }

        [[nodiscard]] rte::EvidenceFrame<2> evidence() const noexcept {
            return rte::EvidenceFrame<2>{
                .capability = "TextSink.log",
                .provider = "memory_log",
                .status = rte::EvidenceStatus::ok,
                .fields = {{
                    {"writes", writes == 2 ? "2" : "unexpected"},
                    {"buffer", view() == "tick=42" ? "tick=42" : "unexpected"},
                }},
            };
        }
    };

    struct StdoutLog {
        std::uint32_t writes{0};

        void write(std::string_view text) noexcept {
            ++writes;
            std::fwrite(text.data(), 1, text.size(), stdout);
        }
    };

    struct FakeClock {
        std::uint32_t now{42};

        [[nodiscard]] std::uint32_t now_ms() noexcept {
            return now;
        }
    };

    static_assert(cap::TextSink::satisfied_by<MemoryLog>);
    static_assert(cap::TextSink::satisfied_by<StdoutLog>);
    static_assert(cap::Clock::satisfied_by<FakeClock>);

    using MemoryLogProvider = rte::ProviderRef<cap::TextSink, provider::memory_log, MemoryLog>;
    using StdoutLogProvider = rte::ProviderRef<cap::TextSink, provider::host_stdout_log, StdoutLog>;
    using FakeClockProvider = rte::ProviderRef<cap::Clock, provider::fake_clock, FakeClock>;
    using LogBinding = rte::ProfileBinding<LogReq, MemoryLogProvider>;
    using ClockBinding = rte::ProfileBinding<ClockReq, FakeClockProvider>;
    using DemoContext = rte::ContextView<LogBinding, ClockBinding>;

    static_assert(!rte::has_requirement_v<DebugTraceReq, LogBinding, ClockBinding>);

    template <typename Context>
    void demo_app_tick(Context& ctx) noexcept {
        auto& log = ctx.template get<LogReq>();
        auto& clock = ctx.template get<ClockReq>();
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

    [[nodiscard]] bool evidence_ok(const rte::EvidenceFrame<2>& frame) noexcept {
        return frame.capability == "TextSink.log" &&
               frame.provider == "memory_log" &&
               frame.status == rte::EvidenceStatus::ok &&
               frame.fields[0].key == "writes" &&
               frame.fields[0].value == "2" &&
               frame.fields[1].key == "buffer" &&
               frame.fields[1].value == "tick=42";
    }
}

int main() {
    static_assert(kDemoComponent.name == "rte.demo_app");

    MemoryLog memory_log{};
    StdoutLog stdout_log{};
    FakeClock fake_clock{};

    [[maybe_unused]] StdoutLogProvider unused_stdout{stdout_log};
    DemoContext context{
        LogBinding{MemoryLogProvider{memory_log}},
        ClockBinding{FakeClockProvider{fake_clock}},
    };

    demo_app_tick(context);

    if (!expect(kInitProjection[0] == "rte.log_service", "init projection starts with log service")) return 1;
    if (!expect(kInitProjection[1] == "rte.clock_service", "init projection includes clock service")) return 1;
    if (!expect(kInitProjection[2] == "rte.demo_app", "init projection ends with dependent app")) return 1;
    if (!expect(memory_log.view() == "tick=42", "bound log provider receives app output")) return 1;
    if (!expect(memory_log.writes == 2, "bound log provider records two writes")) return 1;
    if (!expect(stdout_log.writes == 0, "unbound TextSink provider is not implicitly selected")) return 1;
    if (!expect(evidence_ok(memory_log.evidence()), "provider evidence remains structured")) return 1;

    std::puts("[rte-component-context-smoke] ok");
    return 0;
}
