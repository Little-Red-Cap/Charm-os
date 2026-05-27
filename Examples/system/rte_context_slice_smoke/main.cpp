#include <array>
#include <cstddef>
#include <cstdint>
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

    struct Display {
        template <typename T>
        static constexpr bool satisfied_by = requires(T& display, std::uint32_t color) {
            { display.fill(color) } noexcept -> std::same_as<void>;
        };
    };

    struct App {};
}

namespace role {
    struct log {};
    struct debug_trace {};
    struct monotonic_time {};
    struct primary_display {};
    struct ui_app {};
    struct diagnostics_app {};
}

namespace provider {
    struct memory_log {};
    struct memory_trace {};
    struct spare_text_sink {};
    struct fake_clock {};
    struct host_display {};
}

namespace rte {
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
    };

    template <typename Component, typename Bindings>
    struct ContextSlice {
        using component = Component;
        using bindings = Bindings;
    };

    template <typename... Components>
    struct ComponentList {};

    template <typename... Providers>
    struct ProviderList {};

    template <typename... Bindings>
    struct BindingList {};

    template <typename... Slices>
    struct ProfileResolution {
        using slices = std::tuple<Slices...>;
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

    template <typename RequiredSet, typename Bindings>
    struct all_requirements_bound_once;

    template <typename... Reqs, typename Bindings>
    struct all_requirements_bound_once<RequirementSet<Reqs...>, Bindings>
        : std::bool_constant<(... && (binding_count_v<Reqs, Bindings> == 1))> {};

    template <typename Component, typename Bindings>
    inline constexpr bool component_requirements_bound_once_v =
        all_requirements_bound_once<typename Component::required_set, Bindings>::value;

    template <typename Req, typename Provider>
    inline constexpr bool provider_declares_requirement_v =
        set_contains_v<typename ProvidedFor<Req>::type, typename Provider::provided_set>;

    template <typename Prov, typename Provider>
    inline constexpr bool provider_declares_token_v =
        set_contains_v<Prov, typename Provider::provided_set>;

    template <typename ProviderTag, typename Providers>
    struct provider_count;

    template <typename ProviderTag, typename... Providers>
    struct provider_count<ProviderTag, ProviderList<Providers...>>
        : std::integral_constant<std::size_t, (std::size_t{0} + ... + (std::same_as<ProviderTag, typename Providers::tag> ? 1u : 0u))> {};

    template <typename ProviderTag, typename Providers>
    inline constexpr std::size_t provider_count_v = provider_count<ProviderTag, Providers>::value;

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

    template <typename Slice, typename Providers>
    inline constexpr bool slice_resolved_v =
        component_requirements_bound_once_v<typename Slice::component, typename Slice::bindings> &&
        all_bindings_target_component_requirements_v<typename Slice::bindings, typename Slice::component> &&
        all_bindings_valid_v<typename Slice::bindings, Providers>;

    template <typename Providers, typename... Slices>
    inline constexpr bool profile_slices_resolved_v =
        provider_tokens_unique_v<Providers> &&
        (... && slice_resolved_v<Slices, Providers>);

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
    using TraceReq = rte::Requirement<cap::TextSink, role::debug_trace>;
    using ClockReq = rte::Requirement<cap::Clock, role::monotonic_time>;
    using DisplayReq = rte::Requirement<cap::Display, role::primary_display>;
    using UiAppProv = rte::Provided<cap::App, role::ui_app>;
    using DiagAppProv = rte::Provided<cap::App, role::diagnostics_app>;
    using LogProv = rte::Provided<cap::TextSink, role::log>;
    using TraceProv = rte::Provided<cap::TextSink, role::debug_trace>;
    using ClockProv = rte::Provided<cap::Clock, role::monotonic_time>;
    using DisplayProv = rte::Provided<cap::Display, role::primary_display>;

    using UiRequires = rte::RequirementSet<LogReq, ClockReq, DisplayReq>;
    using DiagRequires = rte::RequirementSet<TraceReq, ClockReq>;
    using UiProvides = rte::ProviderSet<UiAppProv>;
    using DiagProvides = rte::ProviderSet<DiagAppProv>;
    using LogProvides = rte::ProviderSet<LogProv>;
    using TraceProvides = rte::ProviderSet<TraceProv>;
    using ClockProvides = rte::ProviderSet<ClockProv>;
    using DisplayProvides = rte::ProviderSet<DisplayProv>;

    using UiComponent = rte::ComponentDesc<UiRequires, UiProvides>;
    using DiagComponent = rte::ComponentDesc<DiagRequires, DiagProvides>;
    using LogProviderDesc = rte::ProviderDesc<provider::memory_log, LogProvides>;
    using TraceProviderDesc = rte::ProviderDesc<provider::memory_trace, TraceProvides>;
    using SpareTextSinkDesc = rte::ProviderDesc<provider::spare_text_sink, LogProvides>;
    using ClockProviderDesc = rte::ProviderDesc<provider::fake_clock, ClockProvides>;
    using DisplayProviderDesc = rte::ProviderDesc<provider::host_display, DisplayProvides>;
    using Providers = rte::ProviderList<LogProviderDesc, TraceProviderDesc, ClockProviderDesc, DisplayProviderDesc>;

    using UiLogBinding = rte::ProfileBinding<LogReq, provider::memory_log>;
    using UiClockBinding = rte::ProfileBinding<ClockReq, provider::fake_clock>;
    using UiDisplayBinding = rte::ProfileBinding<DisplayReq, provider::host_display>;
    using DiagTraceBinding = rte::ProfileBinding<TraceReq, provider::memory_trace>;
    using DiagClockBinding = rte::ProfileBinding<ClockReq, provider::fake_clock>;
    using BadUiTraceBinding = rte::ProfileBinding<TraceReq, provider::memory_trace>;
    using BadDiagLogBinding = rte::ProfileBinding<LogReq, provider::memory_log>;
    using BadUiSpareBinding = rte::ProfileBinding<LogReq, provider::spare_text_sink>;

    using UiSlice = rte::ContextSlice<UiComponent, rte::BindingList<UiLogBinding, UiClockBinding, UiDisplayBinding>>;
    using DiagSlice = rte::ContextSlice<DiagComponent, rte::BindingList<DiagTraceBinding, DiagClockBinding>>;
    using BadUiLeakySlice = rte::ContextSlice<UiComponent, rte::BindingList<UiLogBinding, UiClockBinding, UiDisplayBinding, BadUiTraceBinding>>;
    using BadDiagLeakySlice = rte::ContextSlice<DiagComponent, rte::BindingList<DiagTraceBinding, DiagClockBinding, BadDiagLogBinding>>;
    using BadUiSpareSlice = rte::ContextSlice<UiComponent, rte::BindingList<BadUiSpareBinding, UiClockBinding, UiDisplayBinding>>;

    static_assert(rte::profile_slices_resolved_v<Providers, UiSlice, DiagSlice>);
    static_assert(!rte::profile_slices_resolved_v<Providers, BadUiLeakySlice, DiagSlice>);
    static_assert(!rte::profile_slices_resolved_v<Providers, UiSlice, BadDiagLeakySlice>);
    static_assert(!rte::profile_slices_resolved_v<Providers, BadUiSpareSlice>);
    static_assert(!rte::profile_slices_resolved_v<rte::ProviderList<LogProviderDesc, TraceProviderDesc, SpareTextSinkDesc, ClockProviderDesc, DisplayProviderDesc>,
                                                  UiSlice,
                                                  DiagSlice>);

    struct MemoryTextSink {
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

    struct HostDisplay {
        std::uint32_t last_color{0};
        std::uint32_t fills{0};

        void fill(std::uint32_t color) noexcept {
            last_color = color;
            ++fills;
        }
    };

    static_assert(cap::TextSink::satisfied_by<MemoryTextSink>);
    static_assert(cap::Clock::satisfied_by<FakeClock>);
    static_assert(cap::Display::satisfied_by<HostDisplay>);

    using LogRef = rte::ProviderRef<cap::TextSink, provider::memory_log, MemoryTextSink>;
    using TraceRef = rte::ProviderRef<cap::TextSink, provider::memory_trace, MemoryTextSink>;
    using SpareTextRef = rte::ProviderRef<cap::TextSink, provider::spare_text_sink, MemoryTextSink>;
    using ClockRef = rte::ProviderRef<cap::Clock, provider::fake_clock, FakeClock>;
    using DisplayRef = rte::ProviderRef<cap::Display, provider::host_display, HostDisplay>;

    using RuntimeLogBinding = rte::RuntimeBinding<LogReq, LogRef>;
    using RuntimeTraceBinding = rte::RuntimeBinding<TraceReq, TraceRef>;
    using RuntimeClockBinding = rte::RuntimeBinding<ClockReq, ClockRef>;
    using RuntimeDisplayBinding = rte::RuntimeBinding<DisplayReq, DisplayRef>;
    using UiContext = rte::ContextView<RuntimeLogBinding, RuntimeClockBinding, RuntimeDisplayBinding>;
    using DiagContext = rte::ContextView<RuntimeTraceBinding, RuntimeClockBinding>;

    static_assert(rte::has_requirement_v<LogReq, RuntimeLogBinding, RuntimeClockBinding, RuntimeDisplayBinding>);
    static_assert(rte::has_requirement_v<DisplayReq, RuntimeLogBinding, RuntimeClockBinding, RuntimeDisplayBinding>);
    static_assert(!rte::has_requirement_v<TraceReq, RuntimeLogBinding, RuntimeClockBinding, RuntimeDisplayBinding>);
    static_assert(rte::has_requirement_v<TraceReq, RuntimeTraceBinding, RuntimeClockBinding>);
    static_assert(!rte::has_requirement_v<LogReq, RuntimeTraceBinding, RuntimeClockBinding>);
    static_assert(!rte::has_requirement_v<DisplayReq, RuntimeTraceBinding, RuntimeClockBinding>);

    void ui_tick(UiContext& context) noexcept {
        auto& log = context.get<LogReq>();
        auto& clock = context.get<ClockReq>();
        auto& display = context.get<DisplayReq>();
        log.write("ui=");
        log.write(clock.now_ms() == 42 ? "42" : "unexpected");
        display.fill(0x00FF00u);
    }

    void diag_tick(DiagContext& context) noexcept {
        auto& trace = context.get<TraceReq>();
        auto& clock = context.get<ClockReq>();
        trace.write("diag=");
        trace.write(clock.now_ms() == 42 ? "42" : "unexpected");
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
    MemoryTextSink log{};
    MemoryTextSink trace{};
    MemoryTextSink spare{};
    FakeClock clock{};
    HostDisplay display{};

    [[maybe_unused]] SpareTextRef unbound_spare{spare};
    UiContext ui_context{
        RuntimeLogBinding{LogRef{log}},
        RuntimeClockBinding{ClockRef{clock}},
        RuntimeDisplayBinding{DisplayRef{display}},
    };
    DiagContext diag_context{
        RuntimeTraceBinding{TraceRef{trace}},
        RuntimeClockBinding{ClockRef{clock}},
    };

    ui_tick(ui_context);
    diag_tick(diag_context);

    if (!expect(log.view() == "ui=42", "ui context uses only ui log binding")) return 1;
    if (!expect(trace.view() == "diag=42", "diag context uses only trace binding")) return 1;
    if (!expect(spare.writes == 0, "unbound text provider is not implicitly selected")) return 1;
    if (!expect(display.fills == 1, "ui context can access display")) return 1;
    if (!expect(display.last_color == 0x00FF00u, "display receives ui color")) return 1;
    if (!expect(clock.now == 42, "contexts share clock provider without owning world")) return 1;

    std::puts("[rte-context-slice-smoke] ok");
    return 0;
}
