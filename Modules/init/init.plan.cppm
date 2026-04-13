module;

#include <optional>
#include <span>
#include <string_view>
#include <tuple>

export module init.plan;

import init.meta;
import init.node;
import util.core;

export namespace init {
    struct cap_name_entry {
        CapId id{0};
        std::string_view name{};
    };

    template <typename T>
    inline constexpr bool unsupported_as_plan_v = false;

    template <typename T>
    concept plan_like = requires { typename T::init_plan_tag; };

    template <typename T>
    concept has_plan_method = requires(const T& candidate) {
        candidate.plan();
    };

    template <typename T>
    concept has_single_node_member = requires(const T& candidate) {
        candidate.node;
    };

    template <typename T>
    concept has_legacy_for_each = requires(const T& candidate) {
        candidate.for_each_legacy_node([](const Node&) noexcept {});
    };

    template <typename T>
    concept has_legacy_node_span = requires(const T& candidate) {
        candidate.node_span();
    };

    template <typename T>
    concept has_legacy_traversal = has_legacy_for_each<T> || has_legacy_node_span<T>;

    template <typename Derived>
    struct plan_ops;

    template <typename Recipe>
    struct bound_recipe;

    template <typename Chain>
    struct legacy_ref;

    template <typename Chain>
    struct legacy_optional_ref;

    template <typename Item>
    struct single_node_ref;

    template <typename Item>
    struct maybe_ref;

    struct legacy_nodes_ref;

    template <typename... Items>
    struct plan;

    template <typename Inner, typename Requires>
    struct after_plan;

    template <typename Inner, typename Cap>
    struct export_plan;

    template <typename Inner>
    struct phase_limit_plan;

    template <typename Inner>
    struct runlevel_plan;

    template <typename Derived>
    struct plan_ops {
        using init_plan_tag = void;

        constexpr const Derived& self() const noexcept {
            return static_cast<const Derived&>(*this);
        }

        template <typename... Caps>
        constexpr auto after() const noexcept {
            return after_plan<Derived, cap_list<Caps...>>{self()};
        }

        template <typename Cap>
        [[deprecated("use ready_as<Cap>() or an explicit barrier recipe")]]
        constexpr auto export_as() const noexcept {
            return export_plan<Derived, Cap>{self()};
        }

        template <typename Cap>
        constexpr auto ready_as() const noexcept {
            return export_plan<Derived, Cap>{self()};
        }

        constexpr auto phase_limit(Phase value) const noexcept {
            return phase_limit_plan<Derived>{self(), value};
        }

        constexpr auto runlevel(util::u32 mask) const noexcept {
            return runlevel_plan<Derived>{self(), mask};
        }
    };

    template <typename Recipe>
    struct bound_recipe : plan_ops<bound_recipe<Recipe>> {
        using runtime_type = typename Recipe::runtime_type;

        runtime_type* ctx{};

        constexpr explicit bound_recipe(runtime_type* value) noexcept
            : ctx(value) {
        }
    };

    template <typename Chain>
    struct legacy_ref : plan_ops<legacy_ref<Chain>> {
        const Chain* chain{};

        constexpr explicit legacy_ref(const Chain* value) noexcept
            : chain(value) {
        }
    };

    template <typename Chain>
    struct legacy_optional_ref : plan_ops<legacy_optional_ref<Chain>> {
        const std::optional<Chain>* chain{};

        constexpr explicit legacy_optional_ref(const std::optional<Chain>* value) noexcept
            : chain(value) {
        }
    };

    template <typename Item>
    struct single_node_ref : plan_ops<single_node_ref<Item>> {
        const Item* value{};

        constexpr explicit single_node_ref(const Item* item) noexcept
            : value(item) {
        }
    };

    template <typename Item>
    struct maybe_ref : plan_ops<maybe_ref<Item>> {
        const std::optional<Item>* value{};

        constexpr explicit maybe_ref(const std::optional<Item>* item) noexcept
            : value(item) {
        }
    };

    struct legacy_nodes_ref : plan_ops<legacy_nodes_ref> {
        std::span<const init::Node* const> nodes{};
        std::span<const cap_name_entry> capability_names{};

        constexpr explicit legacy_nodes_ref(std::span<const init::Node* const> value,
                                            std::span<const cap_name_entry> capability_names_value = {}) noexcept
            : nodes(value), capability_names(capability_names_value) {
        }
    };

    template <typename... Items>
    struct plan : plan_ops<plan<Items...>> {
        std::tuple<Items...> items{};

        constexpr explicit plan(std::tuple<Items...> value) noexcept
            : items(value) {
        }
    };

    template <typename Inner, typename Requires>
    struct after_plan : plan_ops<after_plan<Inner, Requires>> {
        Inner inner;

        constexpr explicit after_plan(const Inner& value) noexcept
            : inner(value) {
        }
    };

    template <typename Inner, typename Cap>
    struct export_plan : plan_ops<export_plan<Inner, Cap>> {
        Inner inner;

        constexpr explicit export_plan(const Inner& value) noexcept
            : inner(value) {
        }
    };

    template <typename Inner>
    struct phase_limit_plan : plan_ops<phase_limit_plan<Inner>> {
        Inner inner;
        Phase value{Phase::app};

        constexpr phase_limit_plan(const Inner& inner_value, Phase phase_value) noexcept
            : inner(inner_value), value(phase_value) {
        }
    };

    template <typename Inner>
    struct runlevel_plan : plan_ops<runlevel_plan<Inner>> {
        Inner inner;
        util::u32 mask{static_cast<util::u32>(Runlevel::all)};

        constexpr runlevel_plan(const Inner& inner_value, util::u32 mask_value) noexcept
            : inner(inner_value), mask(mask_value) {
        }
    };

    template <typename Recipe>
    constexpr auto bind(typename Recipe::runtime_type& ctx) noexcept {
        return bound_recipe<Recipe>{&ctx};
    }

    template <typename Chain>
        requires ((!plan_like<Chain> && !has_plan_method<Chain> && !has_single_node_member<Chain>) ||
                  has_legacy_for_each<Chain>)
    constexpr auto legacy(const Chain& chain) noexcept {
        return legacy_ref<Chain>{&chain};
    }

    template <typename Chain>
        requires (!has_legacy_for_each<Chain> && has_legacy_node_span<Chain>)
    [[deprecated("prefer for_each_legacy_node(...) or compat_nodes(...) instead of legacy(node_span())")]]
    constexpr auto legacy(const Chain& chain) noexcept {
        return legacy_ref<Chain>{&chain};
    }

    template <typename Chain>
        requires ((plan_like<Chain> || has_plan_method<Chain> || has_single_node_member<Chain>) &&
                  !has_legacy_traversal<Chain>)
    [[deprecated("use as_plan(...) for plan-like types and single-node bindings")]]
    constexpr auto legacy(const Chain& chain) noexcept {
        return legacy_ref<Chain>{&chain};
    }

    template <typename Chain>
    [[deprecated("use maybe(...) for optional plans or optional legacy values")]]
    constexpr auto legacy(const std::optional<Chain>& chain) noexcept {
        return legacy_optional_ref<Chain>{&chain};
    }

    template <typename Item>
    constexpr auto maybe(const std::optional<Item>& value) noexcept {
        return maybe_ref<Item>{&value};
    }

    template <typename Item>
    constexpr auto as_plan(const Item& value) noexcept {
        if constexpr (plan_like<Item>) {
            return value;
        } else if constexpr (has_plan_method<Item>) {
            return value.plan();
        } else if constexpr (has_single_node_member<Item>) {
            return single_node_ref<Item>{&value};
        } else {
            static_assert(unsupported_as_plan_v<Item>,
                          "init::as_plan(...) expects a plan-like type, a type with plan(), or a single-node binding; use maybe(...) for optional items");
        }
    }

    constexpr auto compat_nodes(std::span<const init::Node* const> nodes) noexcept {
        return legacy_nodes_ref{nodes};
    }

    constexpr auto compat_nodes(std::span<const init::Node* const> nodes,
                                std::span<const cap_name_entry> capability_names) noexcept {
        return legacy_nodes_ref{nodes, capability_names};
    }

    [[deprecated("use compat_nodes(...) only as a temporary compatibility path for raw Node spans")]]
    constexpr auto legacy_nodes(std::span<const init::Node* const> nodes) noexcept {
        return compat_nodes(nodes);
    }

    template <typename... Items>
    constexpr auto compose(Items... items) noexcept {
        return plan<Items...>{std::tuple<Items...>{items...}};
    }

    template <typename PlanLike, typename... Caps>
    constexpr auto after(PlanLike value, cap_list<Caps...>) noexcept {
        return after_plan<PlanLike, cap_list<Caps...>>{value};
    }

    template <typename PlanLike>
    constexpr auto phase_limit(PlanLike value, Phase max_phase) noexcept {
        return phase_limit_plan<PlanLike>{value, max_phase};
    }

    template <typename PlanLike>
    constexpr auto runlevel(PlanLike value, util::u32 mask) noexcept {
        return runlevel_plan<PlanLike>{value, mask};
    }
}
