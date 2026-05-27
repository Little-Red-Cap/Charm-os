#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <type_traits>

namespace cap {
    struct TextSink {};
    struct Clock {};
    struct RasterDisplay {};
    struct Input {};
    struct App {};
}

namespace role {
    struct log {};
    struct monotonic_time {};
    struct primary_display {};
    struct primary_input {};
    struct main_app {};
}

namespace provider {
    struct host_log {};
    struct host_clock {};
    struct host_framebuffer {};
    struct host_input {};
    struct h747_uart_log {};
    struct h747_systick_clock {};
    struct h747_dsi_display {};
    struct h747_input_service {};
}

namespace rte {
    struct ExplainField {
        std::string_view key{};
        std::string_view value{};
        bool control{false};
    };

    struct ExplainRow {
        std::string_view capability{};
        std::string_view provider{};
        std::array<ExplainField, 6> fields{};
        std::size_t field_count{0};
    };

    struct ExplainReport {
        std::string_view profile{};
        std::string_view board{};
        std::string_view component{};
        std::array<ExplainRow, 4> rows{};
        std::size_t row_count{0};
    };

    struct PresentationBuffer {
        std::array<char, 1024> bytes{};
        std::size_t used{0};
        std::uint32_t formatted_rows{0};

        void append(std::string_view text) noexcept {
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

    template <typename ProfileTag, typename BoardTag, typename Component, typename Providers, typename Bindings>
    struct ProfileSpec {
        using profile_tag = ProfileTag;
        using board_tag = BoardTag;
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
        : std::integral_constant<std::size_t,
                                 (std::size_t{0} + ... + (binding_matches_req_v<Req, Bindings> ? 1u : 0u))> {};

    template <typename Req, typename Bindings>
    inline constexpr std::size_t binding_count_v = binding_count<Req, Bindings>::value;

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
    struct all_bindings_target_component_requirements;

    template <typename... Bindings, typename Component>
    struct all_bindings_target_component_requirements<BindingList<Bindings...>, Component>
        : std::bool_constant<(... && binding_targets_component_requirement_v<Bindings, Component>)> {};

    template <typename Bindings, typename Component>
    inline constexpr bool all_bindings_target_component_requirements_v =
        all_bindings_target_component_requirements<Bindings, Component>::value;

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

    template <typename ProviderTag, typename Providers>
    struct provider_count;

    template <typename ProviderTag, typename... Providers>
    struct provider_count<ProviderTag, ProviderList<Providers...>>
        : std::integral_constant<std::size_t,
                                 (std::size_t{0} + ... + (std::same_as<ProviderTag, typename Providers::tag> ? 1u : 0u))> {};

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

    template <typename Profile>
    inline constexpr bool profile_resolved_v =
        provider_tags_unique_v<typename Profile::providers> &&
        component_requirements_bound_once_v<typename Profile::component, typename Profile::bindings> &&
        all_bindings_target_component_requirements_v<typename Profile::bindings, typename Profile::component> &&
        all_bindings_valid_v<typename Profile::bindings, typename Profile::providers>;

    template <typename Profile>
    struct ResolvedProfile {
        using profile = Profile;
    };

    template <typename Profile>
        requires profile_resolved_v<Profile>
    constexpr ResolvedProfile<Profile> resolve_profile() noexcept {
        return {};
    }

    template <typename T>
    struct is_resolved_profile : std::false_type {};

    template <typename Profile>
    struct is_resolved_profile<ResolvedProfile<Profile>> : std::true_type {};

    template <typename T>
    inline constexpr bool explain_projectable_v = is_resolved_profile<std::remove_cvref_t<T>>::value;

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

    template <typename Req, typename Bindings>
    struct selected_provider_from_list;

    template <typename Req, typename... Bindings>
    struct selected_provider_from_list<Req, BindingList<Bindings...>> {
        using type = typename selected_provider<Req, Bindings...>::type;
    };

    template <typename Req, typename Bindings>
    using selected_provider_from_list_t = typename selected_provider_from_list<Req, Bindings>::type;
}

namespace {
    struct host_player_profile {};
    struct h747_player_profile {};
    struct host_board {};
    struct h747_player_board {};
    struct runtime_provider_marker {
        runtime_provider_marker() noexcept {
            ++constructed;
        }

        inline static std::uint32_t constructed{0};
    };

    using LogReq = rte::Requirement<cap::TextSink, role::log>;
    using ClockReq = rte::Requirement<cap::Clock, role::monotonic_time>;
    using DisplayReq = rte::Requirement<cap::RasterDisplay, role::primary_display>;
    using InputReq = rte::Requirement<cap::Input, role::primary_input>;
    using AppProv = rte::Provided<cap::App, role::main_app>;
    using LogProv = rte::Provided<cap::TextSink, role::log>;
    using ClockProv = rte::Provided<cap::Clock, role::monotonic_time>;
    using DisplayProv = rte::Provided<cap::RasterDisplay, role::primary_display>;
    using InputProv = rte::Provided<cap::Input, role::primary_input>;

    using PlayerRequires = rte::RequirementSet<LogReq, ClockReq, DisplayReq, InputReq>;
    using PlayerProvides = rte::ProviderSet<AppProv>;
    using LogProvides = rte::ProviderSet<LogProv>;
    using ClockProvides = rte::ProviderSet<ClockProv>;
    using DisplayProvides = rte::ProviderSet<DisplayProv>;
    using InputProvides = rte::ProviderSet<InputProv>;
    using PlayerComponent = rte::ComponentDesc<PlayerRequires, PlayerProvides>;

    using HostLogProvider = rte::ProviderDesc<provider::host_log, LogProvides>;
    using HostClockProvider = rte::ProviderDesc<provider::host_clock, ClockProvides>;
    using HostDisplayProvider = rte::ProviderDesc<provider::host_framebuffer, DisplayProvides>;
    using HostInputProvider = rte::ProviderDesc<provider::host_input, InputProvides>;
    using H747LogProvider = rte::ProviderDesc<provider::h747_uart_log, LogProvides>;
    using H747ClockProvider = rte::ProviderDesc<provider::h747_systick_clock, ClockProvides>;
    using H747DisplayProvider = rte::ProviderDesc<provider::h747_dsi_display, DisplayProvides>;
    using H747InputProvider = rte::ProviderDesc<provider::h747_input_service, InputProvides>;

    using HostProfile = rte::ProfileSpec<
        host_player_profile,
        host_board,
        PlayerComponent,
        rte::ProviderList<HostLogProvider, HostClockProvider, HostDisplayProvider, HostInputProvider>,
        rte::BindingList<
            rte::ProfileBinding<LogReq, provider::host_log>,
            rte::ProfileBinding<ClockReq, provider::host_clock>,
            rte::ProfileBinding<DisplayReq, provider::host_framebuffer>,
            rte::ProfileBinding<InputReq, provider::host_input>>>;

    using H747Profile = rte::ProfileSpec<
        h747_player_profile,
        h747_player_board,
        PlayerComponent,
        rte::ProviderList<H747LogProvider, H747ClockProvider, H747DisplayProvider, H747InputProvider>,
        rte::BindingList<
            rte::ProfileBinding<LogReq, provider::h747_uart_log>,
            rte::ProfileBinding<ClockReq, provider::h747_systick_clock>,
            rte::ProfileBinding<DisplayReq, provider::h747_dsi_display>,
            rte::ProfileBinding<InputReq, provider::h747_input_service>>>;

    using MissingInputBindingProfile = rte::ProfileSpec<
        h747_player_profile,
        h747_player_board,
        PlayerComponent,
        rte::ProviderList<H747LogProvider, H747ClockProvider, H747DisplayProvider, H747InputProvider>,
        rte::BindingList<
            rte::ProfileBinding<LogReq, provider::h747_uart_log>,
            rte::ProfileBinding<ClockReq, provider::h747_systick_clock>,
            rte::ProfileBinding<DisplayReq, provider::h747_dsi_display>>>;

    using MismatchedProviderProfile = rte::ProfileSpec<
        host_player_profile,
        host_board,
        PlayerComponent,
        rte::ProviderList<HostLogProvider, HostClockProvider, HostDisplayProvider, HostInputProvider>,
        rte::BindingList<
            rte::ProfileBinding<LogReq, provider::host_log>,
            rte::ProfileBinding<ClockReq, provider::host_clock>,
            rte::ProfileBinding<DisplayReq, provider::host_input>,
            rte::ProfileBinding<InputReq, provider::host_input>>>;

    static_assert(rte::profile_resolved_v<HostProfile>);
    static_assert(rte::profile_resolved_v<H747Profile>);
    static_assert(!rte::profile_resolved_v<MissingInputBindingProfile>);
    static_assert(!rte::profile_resolved_v<MismatchedProviderProfile>);
    static_assert(!rte::explain_projectable_v<HostProfile>);
    static_assert(!rte::explain_projectable_v<MissingInputBindingProfile>);
    static_assert(!rte::explain_projectable_v<MismatchedProviderProfile>);
    static_assert(rte::explain_projectable_v<decltype(rte::resolve_profile<HostProfile>())>);

    template <typename Profile>
    constexpr std::string_view profile_name() noexcept {
        if constexpr (std::same_as<typename Profile::profile_tag, host_player_profile>) {
            return "host_player";
        } else if constexpr (std::same_as<typename Profile::profile_tag, h747_player_profile>) {
            return "h747_player";
        } else {
            return "unknown_profile";
        }
    }

    template <typename Profile>
    constexpr std::string_view board_name() noexcept {
        if constexpr (std::same_as<typename Profile::board_tag, host_board>) {
            return "host_mock";
        } else if constexpr (std::same_as<typename Profile::board_tag, h747_player_board>) {
            return "stm32h747_player";
        } else {
            return "unknown_board";
        }
    }

    template <typename Req>
    constexpr std::string_view capability_name() noexcept {
        if constexpr (std::same_as<Req, LogReq>) {
            return "TextSink.log";
        } else if constexpr (std::same_as<Req, ClockReq>) {
            return "Clock.monotonic_time";
        } else if constexpr (std::same_as<Req, DisplayReq>) {
            return "RasterDisplay.primary_display";
        } else if constexpr (std::same_as<Req, InputReq>) {
            return "Input.primary_input";
        } else {
            return "unknown_capability";
        }
    }

    template <typename ProviderTag>
    constexpr std::string_view provider_name() noexcept {
        if constexpr (std::same_as<ProviderTag, provider::host_log>) {
            return "host_buffered_log";
        } else if constexpr (std::same_as<ProviderTag, provider::host_clock>) {
            return "host_manual_clock";
        } else if constexpr (std::same_as<ProviderTag, provider::host_framebuffer>) {
            return "host_memory_framebuffer";
        } else if constexpr (std::same_as<ProviderTag, provider::host_input>) {
            return "host_null_input";
        } else if constexpr (std::same_as<ProviderTag, provider::h747_uart_log>) {
            return "h747_uart1_console";
        } else if constexpr (std::same_as<ProviderTag, provider::h747_systick_clock>) {
            return "h747_systick_clock";
        } else if constexpr (std::same_as<ProviderTag, provider::h747_dsi_display>) {
            return "h747_dsi_ltdc_display";
        } else if constexpr (std::same_as<ProviderTag, provider::h747_input_service>) {
            return "h747_input_service";
        } else {
            return "unknown_provider";
        }
    }

    template <typename ProviderTag>
    constexpr std::array<rte::ExplainField, 6> provider_fields() noexcept {
        if constexpr (std::same_as<ProviderTag, provider::host_log>) {
            return {{{"profile", "host_player", true}, {"selection", "explicit_binding", true}, {"transport", "memory_buffer", false}}};
        } else if constexpr (std::same_as<ProviderTag, provider::host_clock>) {
            return {{{"profile", "host_player", true}, {"selection", "explicit_binding", true}, {"source", "manual_clock", false}}};
        } else if constexpr (std::same_as<ProviderTag, provider::host_framebuffer>) {
            return {{{"profile", "host_player", true}, {"selection", "explicit_binding", true}, {"mode", "180x320", false}, {"format", "argb8888", false}, {"buffer_policy", "single_memory_framebuffer", false}}};
        } else if constexpr (std::same_as<ProviderTag, provider::host_input>) {
            return {{{"profile", "host_player", true}, {"selection", "explicit_binding", true}, {"source", "null_input", false}, {"pointer", "none", false}, {"encoders", "none", false}}};
        } else if constexpr (std::same_as<ProviderTag, provider::h747_uart_log>) {
            return {{{"profile", "h747_player", true}, {"selection", "explicit_binding", true}, {"transport", "uart1_console", false}}};
        } else if constexpr (std::same_as<ProviderTag, provider::h747_systick_clock>) {
            return {{{"profile", "h747_player", true}, {"selection", "explicit_binding", true}, {"source", "systick", false}}};
        } else if constexpr (std::same_as<ProviderTag, provider::h747_dsi_display>) {
            return {{{"profile", "h747_player", true}, {"selection", "explicit_binding", true}, {"mode", "720x1280", false}, {"format", "argb8888", false}, {"buffer_policy", "double_buffer_vblank_reload", false}}};
        } else if constexpr (std::same_as<ProviderTag, provider::h747_input_service>) {
            return {{{"profile", "h747_player", true}, {"selection", "explicit_binding", true}, {"source", "h747_input_service", false}, {"pointer", "gt970_i2c4_best_effort", false}, {"encoders", "dual_encoder", false}}};
        } else {
            return {};
        }
    }

    template <typename ProviderTag>
    constexpr std::size_t provider_field_count() noexcept {
        if constexpr (std::same_as<ProviderTag, provider::host_log> ||
                      std::same_as<ProviderTag, provider::host_clock> ||
                      std::same_as<ProviderTag, provider::h747_uart_log> ||
                      std::same_as<ProviderTag, provider::h747_systick_clock>) {
            return 3;
        } else {
            return 5;
        }
    }

    template <typename Profile, typename Req>
    constexpr rte::ExplainRow explain_row() noexcept {
        using ProviderTag = rte::selected_provider_from_list_t<Req, typename Profile::bindings>;
        return rte::ExplainRow{
            .capability = capability_name<Req>(),
            .provider = provider_name<ProviderTag>(),
            .fields = provider_fields<ProviderTag>(),
            .field_count = provider_field_count<ProviderTag>(),
        };
    }

    template <typename Profile>
    constexpr rte::ExplainReport project_explain(rte::ResolvedProfile<Profile>) noexcept {
        return rte::ExplainReport{
            .profile = profile_name<Profile>(),
            .board = board_name<Profile>(),
            .component = "player_app",
            .rows = {{
                explain_row<Profile, LogReq>(),
                explain_row<Profile, ClockReq>(),
                explain_row<Profile, DisplayReq>(),
                explain_row<Profile, InputReq>(),
            }},
            .row_count = 4,
        };
    }

    void format_report(rte::PresentationBuffer& output, const rte::ExplainReport& report) noexcept {
        output.append("profile=");
        output.append(report.profile);
        output.append(" board=");
        output.append(report.board);
        output.append(" component=");
        output.append(report.component);
        output.append("\n");
        for (std::size_t row = 0; row < report.row_count; ++row) {
            const auto& item = report.rows[row];
            output.append("capability=");
            output.append(item.capability);
            output.append(" provider=");
            output.append(item.provider);
            for (std::size_t field = 0; field < item.field_count; ++field) {
                output.append(" ");
                output.append(item.fields[field].key);
                output.append("=");
                output.append(item.fields[field].value);
            }
            output.append("\n");
            ++output.formatted_rows;
        }
    }

    const rte::ExplainRow* find_row(const rte::ExplainReport& report, std::string_view capability) noexcept {
        for (std::size_t i = 0; i < report.row_count; ++i) {
            if (report.rows[i].capability == capability) {
                return &report.rows[i];
            }
        }
        return nullptr;
    }

    std::string_view field_value(const rte::ExplainRow& row, std::string_view key) noexcept {
        for (std::size_t i = 0; i < row.field_count; ++i) {
            if (row.fields[i].key == key) {
                return row.fields[i].value;
            }
        }
        return {};
    }

    bool contains(std::string_view text, std::string_view needle) noexcept {
        return text.find(needle) != std::string_view::npos;
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
    constexpr auto host = project_explain(rte::resolve_profile<HostProfile>());
    constexpr auto h747 = project_explain(rte::resolve_profile<H747Profile>());

    if (!expect(runtime_provider_marker::constructed == 0, "explain projection does not construct runtime providers")) return 1;
    if (!expect(host.profile == "host_player", "host report keeps profile name")) return 1;
    if (!expect(h747.profile == "h747_player", "h747 report keeps profile name")) return 1;
    if (!expect(host.board == "host_mock", "host report keeps board name")) return 1;
    if (!expect(h747.board == "stm32h747_player", "h747 report keeps board name")) return 1;
    if (!expect(host.component == h747.component, "host and h747 reports explain the same app component")) return 1;
    if (!expect(host.row_count == 4 && h747.row_count == 4, "reports contain four phase-1 bindings")) return 1;

    for (std::size_t i = 0; i < host.row_count; ++i) {
        if (!expect(host.rows[i].capability == h747.rows[i].capability, "capability semantics match across profiles")) return 1;
        if (!expect(host.rows[i].provider != h747.rows[i].provider, "provider identity differs across profiles")) return 1;
        if (!expect(field_value(host.rows[i], "selection") == "explicit_binding", "host binding selection is explicit")) return 1;
        if (!expect(field_value(h747.rows[i], "selection") == "explicit_binding", "h747 binding selection is explicit")) return 1;
    }

    const auto* host_display = find_row(host, "RasterDisplay.primary_display");
    const auto* h747_display = find_row(h747, "RasterDisplay.primary_display");
    const auto* host_input = find_row(host, "Input.primary_input");
    const auto* h747_input = find_row(h747, "Input.primary_input");
    if (!expect(host_display != nullptr && h747_display != nullptr, "display rows exist")) return 1;
    if (!expect(host_input != nullptr && h747_input != nullptr, "input rows exist")) return 1;
    if (!expect(field_value(*host_display, "mode") == "180x320", "host display mode is explainable")) return 1;
    if (!expect(field_value(*h747_display, "mode") == "720x1280", "h747 display mode is explainable")) return 1;
    if (!expect(field_value(*host_display, "format") == field_value(*h747_display, "format"),
                "display format semantics match")) return 1;
    if (!expect(field_value(*host_display, "buffer_policy") != field_value(*h747_display, "buffer_policy"),
                "display buffer policies explain profile differences")) return 1;
    if (!expect(field_value(*host_input, "source") == "null_input", "host input source is explainable")) return 1;
    if (!expect(field_value(*h747_input, "pointer") == "gt970_i2c4_best_effort", "h747 touch source is explainable")) return 1;
    if (!expect(field_value(*h747_input, "encoders") == "dual_encoder", "h747 encoder source is explainable")) return 1;

    rte::PresentationBuffer presentation{};
    format_report(presentation, h747);
    if (!expect(presentation.formatted_rows == h747.row_count, "presentation formats explain rows")) return 1;
    if (!expect(contains(presentation.view(), "provider=h747_dsi_ltdc_display"), "presentation includes display provider")) return 1;
    if (!expect(contains(presentation.view(), "buffer_policy=double_buffer_vblank_reload"), "presentation includes display facts")) return 1;
    if (!expect(h747.rows[2].provider == "h747_dsi_ltdc_display", "presentation does not mutate report")) return 1;
    if (!expect(runtime_provider_marker::constructed == 0, "presentation does not construct runtime providers")) return 1;

    std::puts("[rte-explain-projection-smoke] ok");
    return 0;
}
