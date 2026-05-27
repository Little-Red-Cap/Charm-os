#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <meta>
#include <span>
#include <string_view>
#include <tuple>
#include <type_traits>

#if !defined(__cpp_impl_reflection)
#error "__cpp_impl_reflection is required"
#endif

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

namespace reflected_spec {
    struct demo_app {
        int requirements;
        int provides;
    };

    struct memory_log {
        int provides;
        int evidence;
    };

    struct spare_log {
        int provides;
        int evidence;
    };

    struct stdout_trace {
        int provides;
        int evidence;
    };

    struct fake_clock {
        int provides;
        int evidence;
    };

    struct stale_log {
        int provides;
        int evidence;
    };

    struct profile {
        int bindings;
    };
}

namespace spine {
    // Report-side evidence model used by this smoke.
    // The report is intentionally prototype-local and keeps a stable
    // diagnostics -> selected_providers section order.
    enum class Phase {
        service,
        app,
    };

    enum class EvidenceStatus : std::uint8_t {
        ok,
        error,
    };

    enum class ReportSection : std::uint8_t {
        diagnostics,
        selected_providers,
    };

    constexpr std::string_view report_section_text(ReportSection section) noexcept {
        switch (section) {
        case ReportSection::diagnostics:
            return "diagnostics";
        case ReportSection::selected_providers:
            return "selected_providers";
        }
        return "unknown";
    }

    struct EvidenceField {
        std::string_view key{};
        std::string_view value{};
    };

    struct EvidenceFrame {
        ReportSection section{ReportSection::diagnostics};
        std::string_view component{};
        std::string_view capability{};
        std::string_view provider{};
        EvidenceStatus status{EvidenceStatus::ok};
        std::array<EvidenceField, 4> fields{};
        std::size_t field_count{0};
    };

    struct EvidenceCollector {
        std::array<EvidenceFrame, 8> frames{};
        std::size_t count{0};

        util::Result<void> append(EvidenceFrame frame) noexcept {
            if (count >= frames.size()) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
            frames[count++] = frame;
            return {};
        }
    };

    struct ReportBuilder {
        EvidenceCollector collector{};
        ReportSection next_section{ReportSection::diagnostics};

        util::Result<void> append_diagnostic(EvidenceFrame frame) noexcept {
            if (next_section != ReportSection::diagnostics ||
                frame.section != ReportSection::diagnostics) {
                return util::unexpected(util::Errc::bad_state);
            }
            return collector.append(frame);
        }

        util::Result<void> begin_selected_providers() noexcept {
            if (next_section != ReportSection::diagnostics) {
                return util::unexpected(util::Errc::bad_state);
            }
            next_section = ReportSection::selected_providers;
            return {};
        }

        util::Result<void> append_selected_provider(EvidenceFrame frame) noexcept {
            if (next_section != ReportSection::selected_providers ||
                frame.section != ReportSection::selected_providers) {
                return util::unexpected(util::Errc::bad_state);
            }
            return collector.append(frame);
        }
    };

    template <typename Resolution>
    constexpr EvidenceFrame profile_resolution_evidence() noexcept;

    template <typename Resolution>
    util::Result<ReportBuilder> make_blocked_profile_report() noexcept {
        ReportBuilder report{};
        auto append = report.append_diagnostic(profile_resolution_evidence<Resolution>());
        if (!append) {
            return util::unexpected(append.error());
        }
        return report;
    }

    template <typename Resolution>
    util::Result<ReportBuilder> make_accepted_profile_report(
        const EvidenceCollector& selected_provider_evidence) noexcept {
        ReportBuilder report{};
        auto diagnostic = report.append_diagnostic(profile_resolution_evidence<Resolution>());
        if (!diagnostic) {
            return util::unexpected(diagnostic.error());
        }
        auto begin = report.begin_selected_providers();
        if (!begin) {
            return util::unexpected(begin.error());
        }
        for (std::size_t i = 0; i < selected_provider_evidence.count; ++i) {
            auto append = report.append_selected_provider(selected_provider_evidence.frames[i]);
            if (!append) {
                return util::unexpected(append.error());
            }
        }
        return report;
    }

    struct DiagnosticSetBuilder {
        EvidenceCollector collector{};

        util::Result<void> append(EvidenceFrame frame) noexcept {
            if (frame.section != ReportSection::diagnostics) {
                return util::unexpected(util::Errc::bad_state);
            }
            return collector.append(frame);
        }
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
    using EvidenceFn = EvidenceFrame (*)(const void*) noexcept;

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
        using spec_type = typename [:SpecRef:];
        using required_set = Requires;
        using provided_set = Provides;

        std::string_view name{};
        Phase phase{Phase::service};
        InitFn init{nullptr};
        EvidenceFn evidence{nullptr};
        void* ctx{nullptr};
    };

    template <auto SpecRef, typename Provides>
    struct ProviderDesc {
        static constexpr auto spec_ref = SpecRef;
        using provider_tag = typename [:SpecRef:];
        using provided_set = Provides;

        std::string_view name{};
        Phase phase{Phase::service};
        InitFn init{nullptr};
        EvidenceFn evidence{nullptr};
        void* ctx{nullptr};
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

    enum class ProfileResolutionStatus : std::uint8_t {
        ok,
        duplicate_provider_tag,
        duplicate_provider_token,
        missing_binding,
        duplicate_binding,
        extra_binding,
        invalid_binding,
    };

    // Compile-time reflected profile resolution rules.
    // These traits define the gate that accepted profiles must pass before
    // they are allowed to project into init.graph or runtime ContextView.

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

    template <typename RequiredSet, typename Bindings>
    struct all_requirements_bound_at_least_once;

    template <typename... Reqs, typename Bindings>
    struct all_requirements_bound_at_least_once<RequirementSet<Reqs...>, Bindings>
        : std::bool_constant<(... && (binding_count_v<Reqs, Bindings> >= 1))> {};

    template <typename Component, typename Bindings>
    inline constexpr bool component_requirements_bound_at_least_once_v =
        all_requirements_bound_at_least_once<typename Component::required_set, Bindings>::value;

    template <typename RequiredSet, typename Bindings>
    struct all_requirements_bound_at_most_once;

    template <typename... Reqs, typename Bindings>
    struct all_requirements_bound_at_most_once<RequirementSet<Reqs...>, Bindings>
        : std::bool_constant<(... && (binding_count_v<Reqs, Bindings> <= 1))> {};

    template <typename Component, typename Bindings>
    inline constexpr bool component_requirements_bound_at_most_once_v =
        all_requirements_bound_at_most_once<typename Component::required_set, Bindings>::value;

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

    template <typename Prov, typename ProviderSet>
    struct provider_set_declares_token;

    template <typename Prov, typename... ProviderProvs>
    struct provider_set_declares_token<Prov, ProviderSet<ProviderProvs...>>
        : std::bool_constant<(... || provided_tokens_equivalent_v<Prov, ProviderProvs>)> {};

    template <typename Prov, typename Provider>
    inline constexpr bool provider_declares_token_v =
        provider_set_declares_token<Prov, typename Provider::provided_set>::value;

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

    template <typename Resolution>
    concept resolved_profile = profile_resolved_v<Resolution>;

    template <typename Resolution>
    consteval ProfileResolutionStatus profile_resolution_status() {
        if constexpr (!provider_tags_unique_v<typename Resolution::providers>) {
            return ProfileResolutionStatus::duplicate_provider_tag;
        } else if constexpr (!provider_tokens_unique_v<typename Resolution::providers>) {
            return ProfileResolutionStatus::duplicate_provider_token;
        } else if constexpr (!component_requirements_bound_at_least_once_v<typename Resolution::component,
                                                                          typename Resolution::bindings>) {
            return ProfileResolutionStatus::missing_binding;
        } else if constexpr (!component_requirements_bound_at_most_once_v<typename Resolution::component,
                                                                         typename Resolution::bindings>) {
            return ProfileResolutionStatus::duplicate_binding;
        } else if constexpr (!all_bindings_target_component_v<typename Resolution::bindings,
                                                             typename Resolution::component>) {
            return ProfileResolutionStatus::extra_binding;
        } else if constexpr (!all_bindings_valid_v<typename Resolution::bindings,
                                                  typename Resolution::providers>) {
            return ProfileResolutionStatus::invalid_binding;
        } else {
            return ProfileResolutionStatus::ok;
        }
    }

    template <typename Resolution>
    inline constexpr ProfileResolutionStatus profile_resolution_status_v =
        profile_resolution_status<Resolution>();

    constexpr std::string_view profile_resolution_status_text(ProfileResolutionStatus status) noexcept {
        switch (status) {
        case ProfileResolutionStatus::ok:
            return "ok";
        case ProfileResolutionStatus::duplicate_provider_tag:
            return "duplicate_provider_tag";
        case ProfileResolutionStatus::duplicate_provider_token:
            return "duplicate_provider_token";
        case ProfileResolutionStatus::missing_binding:
            return "missing_binding";
        case ProfileResolutionStatus::duplicate_binding:
            return "duplicate_binding";
        case ProfileResolutionStatus::extra_binding:
            return "extra_binding";
        case ProfileResolutionStatus::invalid_binding:
            return "invalid_binding";
        }
        return "unknown";
    }

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
        return reflected_shape_has_fields<Provider>(Provider::spec_ref, {"provides", "evidence"});
    }

    template <typename Resolution>
    consteval bool profile_shape_ok() {
        return reflected_shape_has_fields<Resolution>(Resolution::spec_ref, {"bindings"});
    }

    template <typename Reflected>
    consteval std::string_view reflected_name() {
        return std::meta::identifier_of(Reflected::spec_ref);
    }

    template <typename Resolution>
    consteval std::string_view reflected_profile_name() {
        return std::meta::identifier_of(Resolution::spec_ref);
    }

    template <typename Token>
    constexpr std::string_view capability_name() noexcept {
        if constexpr (std::same_as<typename Token::kind, cap::TextSink> &&
                      std::same_as<typename Token::role, role::log>) {
            return "TextSink.log";
        } else if constexpr (std::same_as<typename Token::kind, cap::TextSink> &&
                             std::same_as<typename Token::role, role::debug_trace>) {
            return "TextSink.debug_trace";
        } else if constexpr (std::same_as<typename Token::kind, cap::Clock> &&
                             std::same_as<typename Token::role, role::monotonic_time>) {
            return "Clock.monotonic_time";
        } else if constexpr (std::same_as<typename Token::kind, cap::App> &&
                             std::same_as<typename Token::role, role::main_app>) {
            return "App.main_app";
        } else {
            return "unknown";
        }
    }

    template <typename Component>
    struct component_cap_names;

    template <auto SpecRef, typename Requires, typename... Provides>
    struct component_cap_names<ComponentDesc<SpecRef, Requires, ProviderSet<Provides...>>> {
        static constexpr auto provides() noexcept {
            return std::array<std::string_view, sizeof...(Provides)>{
                capability_name<Provides>()...
            };
        }
    };

    template <auto SpecRef, typename... Provides>
    struct component_cap_names<ProviderDesc<SpecRef, ProviderSet<Provides...>>> {
        static constexpr auto provides() noexcept {
            return std::array<std::string_view, sizeof...(Provides)>{
                capability_name<Provides>()...
            };
        }
    };

    template <typename Component>
    struct component_required_cap_names;

    template <auto SpecRef, typename Provides, typename... Requires>
    struct component_required_cap_names<ComponentDesc<SpecRef, RequirementSet<Requires...>, Provides>> {
        static constexpr auto required() noexcept {
            return std::array<std::string_view, sizeof...(Requires)>{
                capability_name<Requires>()...
            };
        }
    };

    template <auto SpecRef, typename Provides>
    struct component_required_cap_names<ProviderDesc<SpecRef, Provides>> {
        static constexpr auto required() noexcept {
            return std::array<std::string_view, 0>{};
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

    template <typename Component>
    util::Result<void> collect_component_evidence(
        EvidenceCollector& collector,
        const Component& component) noexcept {
        if (!component.evidence) {
            return {};
        }
        return collector.append(component.evidence(component.ctx));
    }

    template <typename... Components>
    util::Result<void> collect_evidence(
        EvidenceCollector& collector,
        const Components&... components) noexcept {
        util::Result<void> result{};
        auto collect_one = [&](const auto& component) noexcept {
            if (!result) {
                return;
            }
            auto collected = collect_component_evidence(collector, component);
            if (!collected) {
                result = util::unexpected(collected.error());
            }
        };
        (collect_one(components), ...);
        return result;
    }

    template <typename Resolution>
    struct ResolvedProfileProjection;

    template <auto SpecRef, typename Component, typename... Providers, typename Bindings>
        requires resolved_profile<ProfileResolution<SpecRef, Component, ProviderList<Providers...>, Bindings>>
    struct ResolvedProfileProjection<ProfileResolution<SpecRef, Component, ProviderList<Providers...>, Bindings>> {
        using resolution = ProfileResolution<SpecRef, Component, ProviderList<Providers...>, Bindings>;
        using tuple_type = std::tuple<ProjectedNode<Providers>..., ProjectedNode<Component>>;

        static auto project(const Providers&... providers, const Component& component) noexcept {
            ((void)providers, ...);
            (void)component;
            return tuple_type{};
        }

        static void materialize(
            tuple_type& projected,
            const Providers&... providers,
            const Component& component) noexcept {
            materialize_init_nodes(projected, providers..., component);
        }

        static util::Result<void> collect_evidence(
            EvidenceCollector& collector,
            const Providers&... providers,
            const Component& component) noexcept {
            return spine::collect_evidence(collector, providers..., component);
        }
    };

    template <typename Resolution, typename... Components>
    concept resolved_profile_projection_accepts = requires(const Components&... components) {
        ResolvedProfileProjection<Resolution>::project(components...);
    };

    // Diagnostic evidence is the common bridge between compile-time profile
    // resolution and the runtime-side unified report assembly.
    constexpr std::string_view status_text(EvidenceStatus status) noexcept {
        switch (status) {
        case EvidenceStatus::ok:
            return "ok";
        case EvidenceStatus::error:
            return "error";
        }
        return "unknown";
    }

    template <typename Resolution>
    constexpr EvidenceFrame profile_resolution_evidence() noexcept {
        constexpr auto status = profile_resolution_status_v<Resolution>;
        return EvidenceFrame{
            .section = ReportSection::diagnostics,
            .component = reflected_profile_name<Resolution>(),
            .capability = "profile.resolution",
            .provider = "compile-time-reflection",
            .status = status == ProfileResolutionStatus::ok ? EvidenceStatus::ok : EvidenceStatus::error,
            .fields = {{
                {"status", profile_resolution_status_text(status)},
                {"source", "reflected-profile"},
                {"projection", status == ProfileResolutionStatus::ok ? "allowed" : "blocked"},
            }},
            .field_count = 3,
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

    template <typename Req, typename Binding>
    inline constexpr bool runtime_binding_matches_req_v =
        std::same_as<typename Binding::requirement, Req>;

    template <typename Req, typename... Bindings>
    struct runtime_binding_count
        : std::integral_constant<std::size_t,
              (std::size_t{0} + ... + (runtime_binding_matches_req_v<Req, Bindings> ? 1u : 0u))> {};

    template <typename RequiredSet, typename... Bindings>
    struct runtime_requirements_bound_once;

    template <typename... Reqs, typename... Bindings>
    struct runtime_requirements_bound_once<RequirementSet<Reqs...>, Bindings...>
        : std::bool_constant<(... && (runtime_binding_count<Reqs, Bindings...>::value == 1))> {};

    template <typename Resolution, typename Binding>
    inline constexpr bool runtime_binding_is_valid_v =
        set_contains_v<typename Binding::requirement, typename Resolution::component::required_set> &&
        provider_count_v<typename Binding::provider::provider, typename Resolution::providers> == 1 &&
        provider_tag_declares_requirement_v<typename Binding::requirement,
                                            typename Binding::provider::provider,
                                            typename Resolution::providers>;

    template <typename Resolution, typename... Bindings>
    inline constexpr bool runtime_context_matches_profile_v =
        resolved_profile<Resolution> &&
        (... && runtime_binding_is_valid_v<Resolution, Bindings>) &&
        runtime_requirements_bound_once<typename Resolution::component::required_set, Bindings...>::value;

    template <typename Resolution, typename... Bindings>
        requires runtime_context_matches_profile_v<Resolution, Bindings...>
    auto make_resolved_context(Bindings... bindings) noexcept {
        return ContextView<Bindings...>{bindings...};
    }
}

namespace {
    // Local reflected prototype vocabulary.
    // Nothing in this block is promoted to Modules/*; the smoke keeps the
    // entire reflected-profile experiment self-contained on purpose.
    // This namespace is intentionally split into:
    // 1) compile-time reflected profile vocabulary
    // 2) host-side accepted projection fixture
    // 3) runtime ContextView vocabulary

    // Compile-time reflected capability and provider vocabulary.
    using LogReq = spine::Requirement<^^reflected_spec::demo_app, ^^cap::TextSink, ^^role::log>;
    using ClockReq = spine::Requirement<^^reflected_spec::demo_app, ^^cap::Clock, ^^role::monotonic_time>;
    using DebugTraceReq = spine::Requirement<^^reflected_spec::demo_app, ^^cap::TextSink, ^^role::debug_trace>;
    using AppReq = spine::Requirement<^^reflected_spec::demo_app, ^^cap::App, ^^role::main_app>;

    using LogProv = spine::Provided<^^reflected_spec::memory_log, ^^cap::TextSink, ^^role::log>;
    using SpareLogProv = spine::Provided<^^reflected_spec::spare_log, ^^cap::TextSink, ^^role::log>;
    using TraceProv = spine::Provided<^^reflected_spec::stdout_trace, ^^cap::TextSink, ^^role::debug_trace>;
    using ClockProv = spine::Provided<^^reflected_spec::fake_clock, ^^cap::Clock, ^^role::monotonic_time>;
    using AppProv = spine::Provided<^^reflected_spec::demo_app, ^^cap::App, ^^role::main_app>;

    using AppRequires = spine::RequirementSet<LogReq, ClockReq>;
    using AppProvides = spine::ProviderSet<AppProv>;
    using LogProvides = spine::ProviderSet<LogProv>;
    using SpareLogProvides = spine::ProviderSet<SpareLogProv>;
    using TraceProvides = spine::ProviderSet<TraceProv>;
    using ClockProvides = spine::ProviderSet<ClockProv>;

    using AppComponent = spine::ComponentDesc<^^reflected_spec::demo_app, AppRequires, AppProvides>;
    using MemoryLogProviderDesc = spine::ProviderDesc<^^reflected_spec::memory_log, LogProvides>;
    using SpareLogProviderDesc = spine::ProviderDesc<^^reflected_spec::spare_log, SpareLogProvides>;
    using StdoutTraceProviderDesc = spine::ProviderDesc<^^reflected_spec::stdout_trace, TraceProvides>;
    using FakeClockProviderDesc = spine::ProviderDesc<^^reflected_spec::fake_clock, ClockProvides>;

    using LogBinding = spine::ProfileBinding<LogReq, ^^reflected_spec::memory_log>;
    using ClockBinding = spine::ProfileBinding<ClockReq, ^^reflected_spec::fake_clock>;
    using DebugTraceBinding = spine::ProfileBinding<DebugTraceReq, ^^reflected_spec::stdout_trace>;
    using WrongRoleBinding = spine::ProfileBinding<LogReq, ^^reflected_spec::stdout_trace>;
    using DuplicateLogBinding = spine::ProfileBinding<LogReq, ^^reflected_spec::spare_log>;
    using StaleLogBinding = spine::ProfileBinding<LogReq, ^^reflected_spec::stale_log>;

    // Compile-time profile case matrix.
    // GoodProfile is the only accepted path; the others pin failure taxonomy.
    using GoodProfile = spine::ProfileResolution<
        ^^reflected_spec::profile,
        AppComponent,
        spine::ProviderList<MemoryLogProviderDesc, FakeClockProviderDesc>,
        spine::BindingList<LogBinding, ClockBinding>>;

    using MissingClockProfile = spine::ProfileResolution<
        ^^reflected_spec::profile,
        AppComponent,
        spine::ProviderList<MemoryLogProviderDesc, FakeClockProviderDesc>,
        spine::BindingList<LogBinding>>;

    using DuplicateBindingProfile = spine::ProfileResolution<
        ^^reflected_spec::profile,
        AppComponent,
        spine::ProviderList<MemoryLogProviderDesc, FakeClockProviderDesc>,
        spine::BindingList<LogBinding, LogBinding, ClockBinding>>;

    using WrongRoleProfile = spine::ProfileResolution<
        ^^reflected_spec::profile,
        AppComponent,
        spine::ProviderList<MemoryLogProviderDesc, StdoutTraceProviderDesc, FakeClockProviderDesc>,
        spine::BindingList<WrongRoleBinding, ClockBinding>>;

    using DuplicateProviderTokenProfile = spine::ProfileResolution<
        ^^reflected_spec::profile,
        AppComponent,
        spine::ProviderList<MemoryLogProviderDesc, SpareLogProviderDesc, FakeClockProviderDesc>,
        spine::BindingList<LogBinding, ClockBinding>>;

    using DuplicateProviderTagProfile = spine::ProfileResolution<
        ^^reflected_spec::profile,
        AppComponent,
        spine::ProviderList<MemoryLogProviderDesc, MemoryLogProviderDesc, FakeClockProviderDesc>,
        spine::BindingList<LogBinding, ClockBinding>>;

    using StaleBindingProfile = spine::ProfileResolution<
        ^^reflected_spec::profile,
        AppComponent,
        spine::ProviderList<MemoryLogProviderDesc, FakeClockProviderDesc>,
        spine::BindingList<StaleLogBinding, ClockBinding>>;

    using ExtraBindingProfile = spine::ProfileResolution<
        ^^reflected_spec::profile,
        AppComponent,
        spine::ProviderList<MemoryLogProviderDesc, StdoutTraceProviderDesc, FakeClockProviderDesc>,
        spine::BindingList<LogBinding, ClockBinding, DebugTraceBinding>>;

    static_assert(std::meta::identifier_of(AppComponent::spec_ref) == std::string_view{"demo_app"});
    static_assert(spine::component_shape_ok<AppComponent>());
    static_assert(spine::provider_shape_ok<MemoryLogProviderDesc>());
    static_assert(spine::provider_shape_ok<FakeClockProviderDesc>());
    static_assert(spine::profile_shape_ok<GoodProfile>());
    static_assert(spine::profile_resolved_v<GoodProfile>);
    static_assert(!spine::profile_resolved_v<MissingClockProfile>);
    static_assert(!spine::profile_resolved_v<DuplicateBindingProfile>);
    static_assert(!spine::profile_resolved_v<WrongRoleProfile>);
    static_assert(!spine::profile_resolved_v<DuplicateProviderTokenProfile>);
    static_assert(!spine::profile_resolved_v<DuplicateProviderTagProfile>);
    static_assert(!spine::profile_resolved_v<StaleBindingProfile>);
    static_assert(!spine::profile_resolved_v<ExtraBindingProfile>);
    static_assert(spine::profile_resolution_status_v<GoodProfile> == spine::ProfileResolutionStatus::ok);
    static_assert(spine::profile_resolution_status_v<DuplicateProviderTagProfile> ==
                  spine::ProfileResolutionStatus::duplicate_provider_tag);
    static_assert(spine::profile_resolution_status_v<DuplicateProviderTokenProfile> ==
                  spine::ProfileResolutionStatus::duplicate_provider_token);
    static_assert(spine::profile_resolution_status_v<MissingClockProfile> ==
                  spine::ProfileResolutionStatus::missing_binding);
    static_assert(spine::profile_resolution_status_v<DuplicateBindingProfile> ==
                  spine::ProfileResolutionStatus::duplicate_binding);
    static_assert(spine::profile_resolution_status_v<ExtraBindingProfile> ==
                  spine::ProfileResolutionStatus::extra_binding);
    static_assert(spine::profile_resolution_status_v<WrongRoleProfile> ==
                  spine::ProfileResolutionStatus::invalid_binding);
    static_assert(spine::profile_resolution_status_v<StaleBindingProfile> ==
                  spine::ProfileResolutionStatus::invalid_binding);

    // Host-side init/evidence fixture state for accepted profile projection.
    struct MemoryLogState {
        spine::InitTrace* trace{nullptr};
        bool ready{false};
        std::uint32_t init_calls{0};
        std::uint32_t formatted_log_writes{0};
    };

    struct ClockState {
        spine::InitTrace* trace{nullptr};
        bool ready{false};
        std::uint32_t init_calls{0};
        std::uint32_t formatted_log_writes{0};
    };

    struct AppState {
        spine::InitTrace* trace{nullptr};
        const MemoryLogState* log{nullptr};
        const ClockState* clock{nullptr};
        bool ready{false};
        std::uint32_t evidence_reads{0};
        std::uint32_t formatted_log_writes{0};
    };

    struct PresentationBuffer {
        std::array<char, 512> bytes{};
        std::size_t used{0};
        std::uint32_t formatted_frames{0};

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

    util::Result<void> init_memory_log(void* ctx) noexcept {
        auto& state = *static_cast<MemoryLogState*>(ctx);
        state.trace->record("memory_log");
        state.ready = true;
        ++state.init_calls;
        return {};
    }

    util::Result<void> init_clock(void* ctx) noexcept {
        auto& state = *static_cast<ClockState*>(ctx);
        state.trace->record("fake_clock");
        state.ready = true;
        ++state.init_calls;
        return {};
    }

    util::Result<void> init_app(void* ctx) noexcept {
        auto& state = *static_cast<AppState*>(ctx);
        if (!state.log || !state.log->ready || !state.clock || !state.clock->ready) {
            return util::unexpected(util::Errc::bad_state);
        }
        state.trace->record("demo_app");
        state.ready = true;
        return {};
    }

    spine::EvidenceFrame memory_log_evidence(const void* ctx) noexcept {
        const auto& state = *static_cast<const MemoryLogState*>(ctx);
        return spine::EvidenceFrame{
            .section = spine::ReportSection::selected_providers,
            .component = spine::reflected_name<MemoryLogProviderDesc>(),
            .capability = spine::capability_name<LogProv>(),
            .provider = spine::reflected_name<MemoryLogProviderDesc>(),
            .status = state.ready ? spine::EvidenceStatus::ok : spine::EvidenceStatus::error,
            .fields = {{
                {"source", "reflected-profile"},
                {"ready", state.ready ? "true" : "false"},
                {"init_calls", state.init_calls == 1 ? "1" : "unexpected"},
            }},
            .field_count = 3,
        };
    }

    spine::EvidenceFrame clock_evidence(const void* ctx) noexcept {
        const auto& state = *static_cast<const ClockState*>(ctx);
        return spine::EvidenceFrame{
            .section = spine::ReportSection::selected_providers,
            .component = spine::reflected_name<FakeClockProviderDesc>(),
            .capability = spine::capability_name<ClockProv>(),
            .provider = spine::reflected_name<FakeClockProviderDesc>(),
            .status = state.ready ? spine::EvidenceStatus::ok : spine::EvidenceStatus::error,
            .fields = {{
                {"source", "reflected-profile"},
                {"ready", state.ready ? "true" : "false"},
                {"init_calls", state.init_calls == 1 ? "1" : "unexpected"},
            }},
            .field_count = 3,
        };
    }

    struct AcceptedProjectionFixture {
        spine::InitTrace trace{};
        MemoryLogState log_state{.trace = &trace};
        ClockState clock_state{.trace = &trace};
        AppState app_state{.trace = &trace, .log = &log_state, .clock = &clock_state};
        MemoryLogProviderDesc memory_log_service{
            .name = spine::reflected_name<MemoryLogProviderDesc>(),
            .phase = spine::Phase::service,
            .init = init_memory_log,
            .evidence = memory_log_evidence,
            .ctx = &log_state,
        };
        FakeClockProviderDesc clock_service{
            .name = spine::reflected_name<FakeClockProviderDesc>(),
            .phase = spine::Phase::service,
            .init = init_clock,
            .evidence = clock_evidence,
            .ctx = &clock_state,
        };
        AppComponent app{
            .name = spine::reflected_name<AppComponent>(),
            .phase = spine::Phase::app,
            .init = init_app,
            .evidence = nullptr,
            .ctx = &app_state,
        };
    };

    template <typename Tuple>
    constexpr auto node_ptrs(Tuple& projected) noexcept {
        return std::apply([](auto&... item) {
            return std::array<const init::Node*, sizeof...(item)>{&item.node...};
        }, projected);
    }

    // Runtime ContextView vocabulary.
    // This proves accepted compile-time profile selections can materialize into
    // a minimal runtime binding surface without implicit provider fallback.
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

    using MemoryLogRef = spine::ProviderRef<cap::TextSink, reflected_spec::memory_log, MemoryLog>;
    using TraceRef = spine::ProviderRef<cap::TextSink, reflected_spec::stdout_trace, TraceSink>;
    using FakeClockRef = spine::ProviderRef<cap::Clock, reflected_spec::fake_clock, FakeClock>;
    using RuntimeLogBinding = spine::RuntimeBinding<LogReq, MemoryLogRef>;
    using RuntimeClockBinding = spine::RuntimeBinding<ClockReq, FakeClockRef>;
    using RuntimeTraceBinding = spine::RuntimeBinding<DebugTraceReq, TraceRef>;
    using AppContext = spine::ContextView<RuntimeLogBinding, RuntimeClockBinding>;

    static_assert(spine::resolved_profile_projection_accepts<
                  GoodProfile,
                  MemoryLogProviderDesc,
                  FakeClockProviderDesc,
                  AppComponent>);
    static_assert(!spine::resolved_profile_projection_accepts<
                  GoodProfile,
                  MemoryLogProviderDesc,
                  StdoutTraceProviderDesc,
                  FakeClockProviderDesc,
                  AppComponent>);
    static_assert(spine::runtime_context_matches_profile_v<
                  GoodProfile,
                  RuntimeLogBinding,
                  RuntimeClockBinding>);
    static_assert(!spine::runtime_context_matches_profile_v<
                  GoodProfile,
                  RuntimeLogBinding>);
    static_assert(!spine::runtime_context_matches_profile_v<
                  GoodProfile,
                  RuntimeLogBinding,
                  RuntimeClockBinding,
                  RuntimeTraceBinding>);
    static_assert(!spine::has_requirement_v<DebugTraceReq, RuntimeLogBinding, RuntimeClockBinding>);
    static_assert(!spine::has_requirement_v<AppReq, RuntimeLogBinding, RuntimeClockBinding>);

    void app_tick(AppContext& context) noexcept {
        auto& log = context.get<LogReq>();
        auto& clock = context.get<ClockReq>();
        log.write("reflected tick=");
        log.write(clock.now_ms() == 42 ? "42" : "unexpected");
    }

    void format_evidence_frame(
        PresentationBuffer& output,
        const spine::EvidenceFrame& frame) noexcept {
        output.append("section=");
        output.append(spine::report_section_text(frame.section));
        output.append(" ");
        output.append("component=");
        output.append(frame.component);
        output.append(" capability=");
        output.append(frame.capability);
        output.append(" provider=");
        output.append(frame.provider);
        output.append(" status=");
        output.append(spine::status_text(frame.status));
        for (std::size_t i = 0; i < frame.field_count; ++i) {
            output.append(" ");
            output.append(frame.fields[i].key);
            output.append("=");
            output.append(frame.fields[i].value);
        }
        output.append("\n");
        ++output.formatted_frames;
    }

    void format_evidence(
        PresentationBuffer& output,
        const spine::EvidenceCollector& collector) noexcept {
        for (std::size_t i = 0; i < collector.count; ++i) {
            format_evidence_frame(output, collector.frames[i]);
        }
    }

    [[nodiscard]] PresentationBuffer present_evidence(
        const spine::EvidenceCollector& collector) noexcept {
        PresentationBuffer presentation{};
        format_evidence(presentation, collector);
        return presentation;
    }

    [[nodiscard]] PresentationBuffer present_blocked_report(
        const spine::ReportBuilder& report) noexcept {
        return present_evidence(report.collector);
    }

    [[nodiscard]] PresentationBuffer present_accepted_report(
        const spine::ReportBuilder& report) noexcept {
        return present_evidence(report.collector);
    }

    [[nodiscard]] PresentationBuffer present_diagnostic_set(
        const spine::DiagnosticSetBuilder& diagnostics) noexcept {
        return present_evidence(diagnostics.collector);
    }

    [[nodiscard]] bool contains(std::string_view text, std::string_view needle) noexcept {
        return text.find(needle) != std::string_view::npos;
    }

    [[nodiscard]] bool expect(bool condition, const char* message) noexcept {
        if (!condition) {
            std::printf("[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    [[nodiscard]] bool evidence_field_is(
        const spine::EvidenceFrame& frame,
        std::size_t index,
        std::string_view key,
        std::string_view value) noexcept {
        return index < frame.field_count &&
               frame.fields[index].key == key &&
               frame.fields[index].value == value;
    }

    [[nodiscard]] bool expect_frame_status(
        const spine::EvidenceFrame& frame,
        spine::EvidenceStatus expected,
        const char* message) noexcept {
        return expect(frame.status == expected, message);
    }

    [[nodiscard]] bool expect_frame_section(
        const spine::EvidenceFrame& frame,
        spine::ReportSection expected,
        const char* message) noexcept {
        return expect(frame.section == expected, message);
    }

    [[nodiscard]] bool expect_presentation_frame_count(
        const PresentationBuffer& presentation,
        std::uint32_t expected,
        const char* message) noexcept {
        return expect(presentation.formatted_frames == expected, message);
    }

    [[nodiscard]] bool expect_contains(
        std::string_view text,
        std::string_view needle,
        const char* message) noexcept {
        return expect(contains(text, needle), message);
    }

    [[nodiscard]] bool expect_not_contains(
        std::string_view text,
        std::string_view needle,
        const char* message) noexcept {
        return expect(!contains(text, needle), message);
    }

    // Report verification focuses on two stable shapes:
    // accepted = diagnostic + selected provider evidence
    // blocked  = diagnostic only
    [[nodiscard]] bool verify_blocked_report(
        const spine::ReportBuilder& report,
        const PresentationBuffer& presentation,
        std::string_view expected_status,
        std::string_view expected_status_needle) noexcept {
        if (!expect(report.collector.count == 1, "blocked report contains only diagnostic evidence")) return false;
        if (!expect_frame_status(report.collector.frames[0], spine::EvidenceStatus::error,
                                 "blocked profile report is error")) return false;
        if (!expect_frame_section(report.collector.frames[0], spine::ReportSection::diagnostics,
                                  "blocked report frame stays in diagnostics section")) return false;
        if (!expect(evidence_field_is(report.collector.frames[0], 0, "status", expected_status),
                    "blocked report keeps failure reason")) return false;
        if (!expect(evidence_field_is(report.collector.frames[0], 2, "projection", "blocked"),
                    "blocked report rejects projection")) return false;
        if (!expect(report.next_section == spine::ReportSection::diagnostics,
                    "blocked report remains in diagnostics section")) return false;

        const auto rendered = presentation.view();
        if (!expect_presentation_frame_count(presentation, 1, "blocked presentation formats one diagnostic")) return false;
        if (!expect_contains(rendered, "capability=profile.resolution",
                             "blocked presentation includes profile diagnostic")) return false;
        if (!expect_contains(rendered, "section=diagnostics",
                             "blocked presentation includes diagnostics section")) return false;
        if (!expect_contains(rendered, expected_status_needle,
                             "blocked presentation includes failure reason")) return false;
        if (!expect_not_contains(rendered, "component=memory_log",
                                 "blocked presentation excludes provider evidence")) return false;
        if (!expect_not_contains(rendered, "component=fake_clock",
                                 "blocked presentation excludes clock evidence")) return false;
        return true;
    }

    [[nodiscard]] bool verify_accepted_report(
        const spine::ReportBuilder& report,
        const PresentationBuffer& presentation) noexcept {
        if (!expect(report.collector.count == 3, "accepted diagnostic and selected providers publish evidence")) return false;
        if (!expect(report.collector.frames[0].capability == "profile.resolution",
                    "unified report starts with profile diagnostic")) return false;
        if (!expect_frame_status(report.collector.frames[0], spine::EvidenceStatus::ok,
                                 "accepted profile diagnostic remains ok")) return false;
        if (!expect_frame_section(report.collector.frames[0], spine::ReportSection::diagnostics,
                                  "accepted diagnostic frame keeps diagnostics section")) return false;
        if (!expect(report.collector.frames[1].component == "memory_log", "log evidence keeps reflected component name")) return false;
        if (!expect(report.collector.frames[1].capability == "TextSink.log", "log evidence keeps reflected capability")) return false;
        if (!expect_frame_section(report.collector.frames[1], spine::ReportSection::selected_providers,
                                  "log evidence frame keeps selected provider section")) return false;
        if (!expect(report.collector.frames[2].component == "fake_clock", "clock evidence keeps reflected component name")) return false;
        if (!expect(report.collector.frames[2].capability == "Clock.monotonic_time", "clock evidence keeps reflected capability")) return false;
        if (!expect_frame_section(report.collector.frames[2], spine::ReportSection::selected_providers,
                                  "clock evidence frame keeps selected provider section")) return false;
        if (!expect(report.next_section == spine::ReportSection::selected_providers,
                    "accepted report ends in selected provider section")) return false;

        const auto rendered = presentation.view();
        if (!expect_presentation_frame_count(presentation, 3, "presentation formats unified evidence report")) return false;
        if (!expect_contains(rendered, "section=diagnostics",
                             "presentation includes diagnostics section")) return false;
        if (!expect_contains(rendered, "section=selected_providers",
                             "presentation includes selected provider section")) return false;
        if (!expect_contains(rendered, "capability=profile.resolution",
                             "presentation includes accepted profile diagnostic")) return false;
        if (!expect_contains(rendered, "component=memory_log",
                             "presentation includes memory_log evidence")) return false;
        if (!expect_contains(rendered, "component=fake_clock",
                             "presentation includes fake_clock evidence")) return false;
        if (!expect_not_contains(rendered, "component=stdout_trace",
                                 "presentation excludes unselected provider evidence")) return false;
        return true;
    }

    [[nodiscard]] bool verify_diagnostic_set(
        const spine::DiagnosticSetBuilder& diagnostics,
        const PresentationBuffer& presentation) noexcept {
        if (!expect(diagnostics.collector.count == 4, "diagnostics are collected outside init projection")) return false;
        if (!expect_frame_status(diagnostics.collector.frames[0], spine::EvidenceStatus::ok,
                                 "accepted profile diagnostic is ok")) return false;
        if (!expect_frame_status(diagnostics.collector.frames[1], spine::EvidenceStatus::error,
                                 "rejected profile diagnostic is error")) return false;
        if (!expect_frame_section(diagnostics.collector.frames[0], spine::ReportSection::diagnostics,
                                  "diagnostic evidence keeps diagnostics section")) return false;
        if (!expect(evidence_field_is(diagnostics.collector.frames[0], 0, "status", "ok"),
                    "accepted profile diagnostic keeps status")) return false;
        if (!expect(evidence_field_is(diagnostics.collector.frames[0], 2, "projection", "allowed"),
                    "accepted profile diagnostic allows projection")) return false;
        if (!expect(evidence_field_is(diagnostics.collector.frames[1], 0, "status", "missing_binding"),
                    "missing binding diagnostic keeps status")) return false;
        if (!expect(evidence_field_is(diagnostics.collector.frames[1], 2, "projection", "blocked"),
                    "rejected profile diagnostic blocks projection")) return false;
        if (!expect(evidence_field_is(diagnostics.collector.frames[2], 0, "status", "duplicate_provider_token"),
                    "duplicate token diagnostic keeps status")) return false;
        if (!expect(evidence_field_is(diagnostics.collector.frames[3], 0, "status", "extra_binding"),
                    "extra binding diagnostic keeps status")) return false;

        const auto rendered = presentation.view();
        if (!expect_presentation_frame_count(presentation, 4, "presentation formats diagnostic evidence")) return false;
        if (!expect_contains(rendered, "capability=profile.resolution",
                             "presentation includes profile resolution capability")) return false;
        if (!expect_contains(rendered, "status=missing_binding",
                             "presentation includes missing binding status")) return false;
        if (!expect_contains(rendered, "status=duplicate_provider_token",
                             "presentation includes duplicate token status")) return false;
        if (!expect_contains(rendered, "projection=blocked",
                             "presentation includes blocked projection")) return false;
        return true;
    }

    template <typename Resolution>
    [[nodiscard]] bool run_blocked_report_case(
        std::string_view expected_status,
        std::string_view expected_status_needle) noexcept {
        auto built = spine::make_blocked_profile_report<Resolution>();
        if (!expect(built.has_value(), "blocked profile report builder succeeds")) return false;
        auto& report = built.value();
        auto presentation = present_blocked_report(report);
        return verify_blocked_report(report, presentation, expected_status, expected_status_needle);
    }

    [[nodiscard]] bool run_diagnostic_set_case(
        const spine::DiagnosticSetBuilder& diagnostics) noexcept {
        auto presentation = present_diagnostic_set(diagnostics);
        return verify_diagnostic_set(diagnostics, presentation);
    }

    [[nodiscard]] bool verify_init_trace(const AcceptedProjectionFixture& fixture) noexcept {
        if (!expect(fixture.trace.count == 3, "selected providers and app initialize")) return false;
        if (!expect(fixture.trace.entries[0] == "memory_log", "bound log provider initializes first")) return false;
        if (!expect(fixture.trace.entries[1] == "fake_clock", "bound clock provider initializes before app")) return false;
        if (!expect(fixture.trace.entries[2] == "demo_app", "app initializes after reflected requirements")) return false;
        return true;
    }

    [[nodiscard]] util::Result<spine::EvidenceCollector> collect_selected_provider_evidence(
        const AcceptedProjectionFixture& fixture) noexcept {
        using Projection = spine::ResolvedProfileProjection<GoodProfile>;

        spine::EvidenceCollector selected{};
        auto collected = Projection::collect_evidence(
            selected,
            fixture.memory_log_service,
            fixture.clock_service,
            fixture.app);
        if (!collected) {
            return util::unexpected(collected.error());
        }
        return selected;
    }

    // Top-level smoke entrypoints keep the execution story explicit instead of
    // hiding init/projection flow behind a generic test registry.
    [[nodiscard]] bool run_profile_resolution_diagnostics() noexcept {
        constexpr auto good = spine::profile_resolution_evidence<GoodProfile>();
        constexpr auto missing = spine::profile_resolution_evidence<MissingClockProfile>();
        constexpr auto duplicate_token = spine::profile_resolution_evidence<DuplicateProviderTokenProfile>();
        constexpr auto extra = spine::profile_resolution_evidence<ExtraBindingProfile>();
        constexpr auto invalid = spine::profile_resolution_evidence<WrongRoleProfile>();

        static_assert(good.status == spine::EvidenceStatus::ok);
        static_assert(missing.status == spine::EvidenceStatus::error);
        static_assert(good.fields[0].value == std::string_view{"ok"});
        static_assert(missing.fields[0].value == std::string_view{"missing_binding"});
        static_assert(duplicate_token.fields[0].value == std::string_view{"duplicate_provider_token"});
        static_assert(extra.fields[0].value == std::string_view{"extra_binding"});
        static_assert(invalid.fields[0].value == std::string_view{"invalid_binding"});

        spine::DiagnosticSetBuilder diagnostics{};
        auto append = diagnostics.append(good);
        if (!expect(append.has_value(), "good profile diagnostic evidence appends")) return false;
        append = diagnostics.append(missing);
        if (!expect(append.has_value(), "missing binding diagnostic evidence appends")) return false;
        append = diagnostics.append(duplicate_token);
        if (!expect(append.has_value(), "duplicate token diagnostic evidence appends")) return false;
        append = diagnostics.append(extra);
        if (!expect(append.has_value(), "extra binding diagnostic evidence appends")) return false;

        return run_diagnostic_set_case(diagnostics);
    }

    [[nodiscard]] bool run_blocked_profile_report() noexcept {
        return run_blocked_report_case<MissingClockProfile>("missing_binding", "status=missing_binding");
    }

    [[nodiscard]] bool run_reflected_profile_projection() noexcept {
        static_assert(spine::profile_resolved_v<GoodProfile>);

        spine::ReportBuilder report{};
        AcceptedProjectionFixture fixture{};

        using Projection = spine::ResolvedProfileProjection<GoodProfile>;
        auto projected = Projection::project(
            fixture.memory_log_service,
            fixture.clock_service,
            fixture.app);
        Projection::materialize(
            projected,
            fixture.memory_log_service,
            fixture.clock_service,
            fixture.app);
        auto nodes = node_ptrs(projected);
        init::Graph<6, 8> graph{};
        auto build = graph.build(nodes);
        if (!expect(build.has_value(), "accepted reflected profile projects to init graph")) return false;
        if (!expect(graph.ordered() == 3, "init projection contains selected providers and app")) return false;
        if (!expect(report.collector.count == 0, "evidence collector is empty before init")) return false;

        auto start = graph.start();
        if (!expect(start.has_value(), "reflected profile init graph starts")) return false;
        if (!verify_init_trace(fixture)) return false;
        if (!expect(report.collector.count == 0, "init does not collect evidence")) return false;

        auto invalid_order = report.append_selected_provider(memory_log_evidence(&fixture.log_state));
        if (!expect(!invalid_order.has_value(), "provider evidence is blocked before provider section begins")) return false;

        auto selected_result = collect_selected_provider_evidence(fixture);
        if (!expect(selected_result.has_value(), "evidence side-channel collection succeeds")) return false;
        auto selected = selected_result.value();
        if (!expect(selected.count == 2, "selected provider collection keeps two provider frames")) return false;

        auto built = spine::make_accepted_profile_report<GoodProfile>(selected);
        if (!expect(built.has_value(), "accepted profile report builder succeeds")) return false;
        report = built.value();
        auto invalid_diagnostic_order = report.append_diagnostic(spine::profile_resolution_evidence<GoodProfile>());
        if (!expect(!invalid_diagnostic_order.has_value(),
                    "diagnostic evidence is blocked after provider section begins")) return false;

        auto presentation = present_accepted_report(report);
        if (!verify_accepted_report(report, presentation)) return false;
        if (!expect(fixture.log_state.formatted_log_writes == 0, "evidence presentation does not mutate log provider")) return false;
        if (!expect(fixture.clock_state.formatted_log_writes == 0, "evidence presentation does not mutate clock provider")) return false;
        if (!expect(fixture.app_state.evidence_reads == 0, "app does not consume evidence side-channel")) return false;
        return true;
    }

    [[nodiscard]] bool run_context_projection() noexcept {
        MemoryLog log{};
        TraceSink trace{};
        FakeClock clock{};

        [[maybe_unused]] TraceRef unbound_trace{trace};
        auto context = spine::make_resolved_context<GoodProfile>(
            RuntimeLogBinding{MemoryLogRef{log}},
            RuntimeClockBinding{FakeClockRef{clock}});

        app_tick(context);

        if (!expect(log.view() == "reflected tick=42", "accepted reflected profile materializes ContextView")) return false;
        if (!expect(log.writes == 2, "bound provider receives app writes")) return false;
        if (!expect(trace.writes == 0, "unbound provider is not implicitly selected")) return false;
        return true;
    }
}

int main() {
    if (!run_profile_resolution_diagnostics()) return 1;
    if (!run_blocked_profile_report()) return 1;
    if (!run_reflected_profile_projection()) return 1;
    if (!run_context_projection()) return 1;

    std::puts("[charm-spine-reflected-profile-smoke] ok");
    return 0;
}
