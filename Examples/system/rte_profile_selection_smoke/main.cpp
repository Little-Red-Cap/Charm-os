#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

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
    struct monotonic_time {};
    struct primary_display {};
    struct main_app {};
}

namespace provider {
    struct host_log {};
    struct host_clock {};
    struct host_framebuffer {};
    struct h747_uart_log {};
    struct h747_systick_clock {};
    struct h747_dsi_display {};
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

    template <typename... Providers>
    struct ProviderList {};

    template <typename... Bindings>
    struct BindingList {};

    template <typename NameTag, typename Component, typename Providers, typename Bindings>
    struct ProfileSpec {
        using name_tag = NameTag;
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

    template <typename Profile>
    inline constexpr bool profile_resolved_v =
        provider_tags_unique_v<typename Profile::providers> &&
        component_requirements_bound_once_v<typename Profile::component, typename Profile::bindings> &&
        all_bindings_target_component_requirements_v<typename Profile::bindings,
                                                     typename Profile::component> &&
        all_bindings_valid_v<typename Profile::bindings,
                             typename Profile::providers>;

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

    template <typename Req, typename BindingList>
    struct selected_provider_from_list;

    template <typename Req, typename... Bindings>
    struct selected_provider_from_list<Req, BindingList<Bindings...>> {
        using type = typename selected_provider<Req, Bindings...>::type;
    };

    template <typename Req, typename Bindings>
    using selected_provider_from_list_t = typename selected_provider_from_list<Req, Bindings>::type;

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
    struct host_profile {};
    struct h747_profile {};

    using LogReq = rte::Requirement<cap::TextSink, role::log>;
    using ClockReq = rte::Requirement<cap::Clock, role::monotonic_time>;
    using DisplayReq = rte::Requirement<cap::Display, role::primary_display>;
    using AppProv = rte::Provided<cap::App, role::main_app>;
    using LogProv = rte::Provided<cap::TextSink, role::log>;
    using ClockProv = rte::Provided<cap::Clock, role::monotonic_time>;
    using DisplayProv = rte::Provided<cap::Display, role::primary_display>;

    using AppRequires = rte::RequirementSet<LogReq, ClockReq, DisplayReq>;
    using AppProvides = rte::ProviderSet<AppProv>;
    using LogProvides = rte::ProviderSet<LogProv>;
    using ClockProvides = rte::ProviderSet<ClockProv>;
    using DisplayProvides = rte::ProviderSet<DisplayProv>;
    using DisplayDemoComponent = rte::ComponentDesc<AppRequires, AppProvides>;

    using HostLogProviderDesc = rte::ProviderDesc<provider::host_log, LogProvides>;
    using HostClockProviderDesc = rte::ProviderDesc<provider::host_clock, ClockProvides>;
    using HostDisplayProviderDesc = rte::ProviderDesc<provider::host_framebuffer, DisplayProvides>;
    using BoardLogProviderDesc = rte::ProviderDesc<provider::h747_uart_log, LogProvides>;
    using BoardClockProviderDesc = rte::ProviderDesc<provider::h747_systick_clock, ClockProvides>;
    using BoardDisplayProviderDesc = rte::ProviderDesc<provider::h747_dsi_display, DisplayProvides>;

    using HostLogBinding = rte::ProfileBinding<LogReq, provider::host_log>;
    using HostClockBinding = rte::ProfileBinding<ClockReq, provider::host_clock>;
    using HostDisplayBinding = rte::ProfileBinding<DisplayReq, provider::host_framebuffer>;
    using BoardLogBinding = rte::ProfileBinding<LogReq, provider::h747_uart_log>;
    using BoardClockBinding = rte::ProfileBinding<ClockReq, provider::h747_systick_clock>;
    using BoardDisplayBinding = rte::ProfileBinding<DisplayReq, provider::h747_dsi_display>;

    using HostProfile = rte::ProfileSpec<
        host_profile,
        DisplayDemoComponent,
        rte::ProviderList<HostLogProviderDesc, HostClockProviderDesc, HostDisplayProviderDesc>,
        rte::BindingList<HostLogBinding, HostClockBinding, HostDisplayBinding>>;

    using BoardProfile = rte::ProfileSpec<
        h747_profile,
        DisplayDemoComponent,
        rte::ProviderList<BoardLogProviderDesc, BoardClockProviderDesc, BoardDisplayProviderDesc>,
        rte::BindingList<BoardLogBinding, BoardClockBinding, BoardDisplayBinding>>;

    using MixedProfile = rte::ProfileSpec<
        host_profile,
        DisplayDemoComponent,
        rte::ProviderList<HostLogProviderDesc, BoardClockProviderDesc, BoardDisplayProviderDesc>,
        rte::BindingList<HostLogBinding, HostClockBinding, BoardDisplayBinding>>;

    static_assert(rte::profile_resolved_v<HostProfile>);
    static_assert(rte::profile_resolved_v<BoardProfile>);
    static_assert(!rte::profile_resolved_v<MixedProfile>);
    static_assert(std::same_as<rte::selected_provider_from_list_t<LogReq, typename HostProfile::bindings>,
                               provider::host_log>);
    static_assert(std::same_as<rte::selected_provider_from_list_t<LogReq, typename BoardProfile::bindings>,
                               provider::h747_uart_log>);

    template <typename Tag>
    struct BufferedTextSink {
        std::array<char, 160> bytes{};
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

    template <typename Tag>
    struct FixedClock {
        std::uint32_t now{42};

        [[nodiscard]] std::uint32_t now_ms() noexcept {
            return now;
        }
    };

    template <typename Tag>
    struct SolidFillDisplay {
        std::uint32_t last_color{0};
        std::uint32_t fills{0};

        void fill(std::uint32_t color) noexcept {
            last_color = color;
            ++fills;
        }
    };

    using HostLogSink = BufferedTextSink<provider::host_log>;
    using BoardUartLogSink = BufferedTextSink<provider::h747_uart_log>;
    using HostClock = FixedClock<provider::host_clock>;
    using BoardSystickClock = FixedClock<provider::h747_systick_clock>;
    using HostFramebuffer = SolidFillDisplay<provider::host_framebuffer>;
    using BoardDsiDisplay = SolidFillDisplay<provider::h747_dsi_display>;

    static_assert(cap::TextSink::satisfied_by<HostLogSink>);
    static_assert(cap::TextSink::satisfied_by<BoardUartLogSink>);
    static_assert(cap::Clock::satisfied_by<HostClock>);
    static_assert(cap::Clock::satisfied_by<BoardSystickClock>);
    static_assert(cap::Display::satisfied_by<HostFramebuffer>);
    static_assert(cap::Display::satisfied_by<BoardDsiDisplay>);

    using HostLogRef = rte::ProviderRef<cap::TextSink, provider::host_log, HostLogSink>;
    using HostClockRef = rte::ProviderRef<cap::Clock, provider::host_clock, HostClock>;
    using HostDisplayRef = rte::ProviderRef<cap::Display, provider::host_framebuffer, HostFramebuffer>;
    using BoardLogRef = rte::ProviderRef<cap::TextSink, provider::h747_uart_log, BoardUartLogSink>;
    using BoardClockRef = rte::ProviderRef<cap::Clock, provider::h747_systick_clock, BoardSystickClock>;
    using BoardDisplayRef = rte::ProviderRef<cap::Display, provider::h747_dsi_display, BoardDsiDisplay>;

    using HostContext = rte::ContextView<
        rte::RuntimeBinding<LogReq, HostLogRef>,
        rte::RuntimeBinding<ClockReq, HostClockRef>,
        rte::RuntimeBinding<DisplayReq, HostDisplayRef>>;

    using BoardContext = rte::ContextView<
        rte::RuntimeBinding<LogReq, BoardLogRef>,
        rte::RuntimeBinding<ClockReq, BoardClockRef>,
        rte::RuntimeBinding<DisplayReq, BoardDisplayRef>>;

    static_assert(rte::has_requirement_v<LogReq,
                                          rte::RuntimeBinding<LogReq, HostLogRef>,
                                          rte::RuntimeBinding<ClockReq, HostClockRef>,
                                          rte::RuntimeBinding<DisplayReq, HostDisplayRef>>);
    static_assert(rte::has_requirement_v<DisplayReq,
                                          rte::RuntimeBinding<LogReq, BoardLogRef>,
                                          rte::RuntimeBinding<ClockReq, BoardClockRef>,
                                          rte::RuntimeBinding<DisplayReq, BoardDisplayRef>>);

    template <typename Context, typename Req>
    using context_cap_t = std::remove_reference_t<decltype(std::declval<Context&>().template get<Req>())>;

    template <typename Context>
    concept DisplayDemoContext =
        requires(Context& context) {
            context.template get<LogReq>();
            context.template get<ClockReq>();
            context.template get<DisplayReq>();
        } &&
        cap::TextSink::template satisfied_by<context_cap_t<Context, LogReq>> &&
        cap::Clock::template satisfied_by<context_cap_t<Context, ClockReq>> &&
        cap::Display::template satisfied_by<context_cap_t<Context, DisplayReq>>;

    static_assert(DisplayDemoContext<HostContext>);
    static_assert(DisplayDemoContext<BoardContext>);

    template <DisplayDemoContext Context>
    void app_tick(Context& context) noexcept {
        auto& log = context.template get<LogReq>();
        auto& clock = context.template get<ClockReq>();
        auto& display = context.template get<DisplayReq>();
        log.write("frame=");
        log.write(clock.now_ms() == 42 ? "42" : "unexpected");
        display.fill(0x00224466u);
    }

    template <typename Profile>
    constexpr std::string_view profile_name() noexcept {
        if constexpr (std::same_as<typename Profile::name_tag, host_profile>) {
            return "host";
        } else if constexpr (std::same_as<typename Profile::name_tag, h747_profile>) {
            return "h747";
        } else {
            return "unknown";
        }
    }

    template <typename ProviderTag>
    constexpr std::string_view provider_name() noexcept {
        if constexpr (std::same_as<ProviderTag, provider::host_log>) {
            return "host_log";
        } else if constexpr (std::same_as<ProviderTag, provider::host_clock>) {
            return "host_clock";
        } else if constexpr (std::same_as<ProviderTag, provider::host_framebuffer>) {
            return "host_framebuffer";
        } else if constexpr (std::same_as<ProviderTag, provider::h747_uart_log>) {
            return "h747_uart_log";
        } else if constexpr (std::same_as<ProviderTag, provider::h747_systick_clock>) {
            return "h747_systick_clock";
        } else if constexpr (std::same_as<ProviderTag, provider::h747_dsi_display>) {
            return "h747_dsi_display";
        } else {
            return "unknown";
        }
    }

    template <typename Profile, typename Req>
    rte::EvidenceFrame selected_provider_evidence(std::string_view capability) noexcept {
        using ProviderTag = rte::selected_provider_from_list_t<Req, typename Profile::bindings>;
        return rte::EvidenceFrame{
            .capability = capability,
            .provider = provider_name<ProviderTag>(),
            .status = rte::EvidenceStatus::ok,
            .fields = {{
                {"profile", profile_name<Profile>()},
                {"selection", "explicit_binding"},
            }},
            .field_count = 2,
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
    HostLogSink host_log{};
    HostClock host_clock{};
    HostFramebuffer host_display{};
    HostContext host_context{
        rte::RuntimeBinding<LogReq, HostLogRef>{HostLogRef{host_log}},
        rte::RuntimeBinding<ClockReq, HostClockRef>{HostClockRef{host_clock}},
        rte::RuntimeBinding<DisplayReq, HostDisplayRef>{HostDisplayRef{host_display}},
    };

    BoardUartLogSink board_log{};
    BoardSystickClock board_clock{};
    BoardDsiDisplay board_display{};
    BoardContext board_context{
        rte::RuntimeBinding<LogReq, BoardLogRef>{BoardLogRef{board_log}},
        rte::RuntimeBinding<ClockReq, BoardClockRef>{BoardClockRef{board_clock}},
        rte::RuntimeBinding<DisplayReq, BoardDisplayRef>{BoardDisplayRef{board_display}},
    };

    app_tick(host_context);
    app_tick(board_context);

    const auto host_log_evidence = selected_provider_evidence<HostProfile, LogReq>("TextSink.log");
    const auto host_display_evidence = selected_provider_evidence<HostProfile, DisplayReq>("Display.primary_display");
    const auto board_log_evidence = selected_provider_evidence<BoardProfile, LogReq>("TextSink.log");
    const auto board_display_evidence = selected_provider_evidence<BoardProfile, DisplayReq>("Display.primary_display");

    if (!expect(host_log.view() == "frame=42", "host profile preserves app-level log semantics")) return 1;
    if (!expect(board_log.view() == "frame=42", "board profile preserves app-level log semantics")) return 1;
    if (!expect(host_display.last_color == 0x00224466u, "host display receives same app color")) return 1;
    if (!expect(board_display.last_color == 0x00224466u, "board display receives same app color")) return 1;
    if (!expect(host_display.fills == 1 && board_display.fills == 1, "profile selection does not duplicate app work")) return 1;
    if (!expect(host_log_evidence.provider == "host_log", "host evidence keeps selected log provider")) return 1;
    if (!expect(host_display_evidence.provider == "host_framebuffer", "host evidence keeps selected display provider")) return 1;
    if (!expect(board_log_evidence.provider == "h747_uart_log", "board evidence keeps selected log provider")) return 1;
    if (!expect(board_display_evidence.provider == "h747_dsi_display", "board evidence keeps selected display provider")) return 1;
    if (!expect(host_log_evidence.fields[0].value == "host", "host evidence reports profile selection")) return 1;
    if (!expect(board_log_evidence.fields[0].value == "h747", "board evidence reports profile selection")) return 1;

    std::puts("[rte-profile-selection-smoke] ok");
    return 0;
}
