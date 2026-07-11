#pragma once

#include <cstddef>
#include <tuple>
#include <type_traits>

namespace charm::backend::contract {
    template <typename T>
    concept ProviderInstanceToken = requires {
        typename T::charm_provider_instance_tag;
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

    template <ProviderInstanceToken ProviderInstance, typename Provides>
    struct ProviderDesc {
        using provider_instance = ProviderInstance;
        using tag = ProviderInstance;
        using provided_set = Provides;
    };

    template <ProviderInstanceToken ProviderInstance,
              typename ProviderType,
              typename Backend,
              typename RuntimeDomain,
              typename Adapter,
              typename... ExtraMetadata>
    struct ProviderMeta {
        using provider_instance = ProviderInstance;
        using provider_type = ProviderType;
        using backend = Backend;
        using runtime_domain = RuntimeDomain;
        using adapter = Adapter;
        using extra_metadata = std::tuple<ExtraMetadata...>;
    };

    template <typename Req, ProviderInstanceToken ProviderInstance>
    struct ProfileBinding {
        using requirement = Req;
        using provider_instance = ProviderInstance;
    };

    template <typename Req, typename Target>
    concept CanMakeProfileBinding = requires {
        typename ProfileBinding<Req, Target>;
    };

    template <typename Req, typename Provider>
    inline constexpr bool provider_declares_requirement_v =
        set_contains_v<typename ProvidedFor<Req>::type, typename Provider::provided_set>;

    template <typename Req, typename ProviderInstance, typename Providers>
    struct provider_instance_declares_requirement;

    template <typename Req, typename ProviderInstance, typename... Providers>
    struct provider_instance_declares_requirement<Req, ProviderInstance, std::tuple<Providers...>>
        : std::bool_constant<(... || (std::same_as<ProviderInstance, typename Providers::provider_instance> &&
                                      provider_declares_requirement_v<Req, Providers>))> {};

    template <typename Req, typename ProviderInstance, typename Providers>
    inline constexpr bool provider_instance_declares_requirement_v =
        provider_instance_declares_requirement<Req, ProviderInstance, Providers>::value;

    template <typename Binding, typename Providers>
    inline constexpr bool binding_valid_v =
        provider_instance_declares_requirement_v<typename Binding::requirement,
                                                typename Binding::provider_instance,
                                                Providers>;

    template <typename Req, typename Binding>
    inline constexpr bool binding_matches_req_v =
        std::same_as<typename Binding::requirement, Req>;

    template <typename Req, typename Bindings>
    struct binding_count;

    template <typename Req, typename... Bindings>
    struct binding_count<Req, std::tuple<Bindings...>>
        : std::integral_constant<std::size_t, (std::size_t{0} + ... + (binding_matches_req_v<Req, Bindings> ? 1U : 0U))> {};

    template <typename Req, typename Bindings>
    inline constexpr std::size_t binding_count_v = binding_count<Req, Bindings>::value;

    template <typename Bindings, typename Requirements>
    struct requirements_bound_once;

    template <typename Bindings, typename... Reqs>
    struct requirements_bound_once<Bindings, RequirementSet<Reqs...>>
        : std::bool_constant<(... && (binding_count_v<Reqs, Bindings> == 1U))> {};

    template <typename Bindings, typename Requirements>
    inline constexpr bool requirements_bound_once_v =
        requirements_bound_once<Bindings, Requirements>::value;

    template <typename Binding, typename Metas>
    struct binding_has_meta;

    template <typename Binding, typename... Metas>
    struct binding_has_meta<Binding, std::tuple<Metas...>>
        : std::bool_constant<(... || std::same_as<typename Binding::provider_instance,
                                                  typename Metas::provider_instance>)> {};

    template <typename Binding, typename Metas>
    inline constexpr bool binding_has_meta_v = binding_has_meta<Binding, Metas>::value;

    template <typename Kind, ProviderInstanceToken ProviderInstance, typename Impl>
    struct ProviderRef {
        using kind = Kind;
        using provider_instance = ProviderInstance;
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

    template <typename... Bindings>
    class ContextView {
    public:
        constexpr explicit ContextView(Bindings... bindings) noexcept
            : bindings_(bindings...) {}

        template <typename Req>
            requires ((... || std::same_as<typename Bindings::requirement, Req>))
        [[nodiscard]] constexpr auto& get() noexcept {
            return binding_for<Req>();
        }

    private:
        using Tuple = std::tuple<Bindings...>;

        template <typename Req, std::size_t Index = 0>
        [[nodiscard]] constexpr auto& binding_for() noexcept {
            static_assert(Index < sizeof...(Bindings), "missing binding for requirement");
            using Binding = std::tuple_element_t<Index, Tuple>;
            if constexpr (std::same_as<typename Binding::requirement, Req>) {
                return std::get<Index>(bindings_).get();
            } else {
                return binding_for<Req, Index + 1>();
            }
        }

        Tuple bindings_;
    };
}
