module;

#include <array>
#include <span>
#include <string_view>

export module init.binding;

export import init.node;

import util.core;

export namespace init {
    constexpr std::string_view capability_name_view(std::string_view name) noexcept {
        return name;
    }

    constexpr std::string_view capability_name_view(const char* name) noexcept {
        return name ? std::string_view{name} : std::string_view{};
    }

    constexpr CapId cap_id_or_zero(std::string_view name) noexcept {
        return name.empty() ? 0u : cap_id(name);
    }

    template <typename... Names>
    constexpr auto capability_ids(Names... names) noexcept {
        return std::array<CapId, sizeof...(Names)>{
            cap_id_or_zero(capability_name_view(names))...
        };
    }

    template <typename... Names>
    constexpr auto capability_names(Names... names) noexcept {
        return std::array<std::string_view, sizeof...(Names)>{
            capability_name_view(names)...
        };
    }

    template <util::usize ProvideCount>
    constexpr Node make_binding_node(std::string_view name,
                                     Phase phase,
                                     util::u32 runlevel_mask,
                                     const std::array<CapId, ProvideCount>& provides,
                                     InitFn init = nullptr,
                                     DeinitFn deinit = nullptr,
                                     void* ctx = nullptr) noexcept {
        return Node{
            name,
            phase,
            runlevel_mask,
            std::span<const CapId>(provides.data(), provides.size()),
            {},
            init,
            deinit,
            ctx
        };
    }

    constexpr Node make_binding_node(std::string_view name,
                                     Phase phase,
                                     util::u32 runlevel_mask,
                                     std::span<const CapId> provides,
                                     InitFn init = nullptr,
                                     DeinitFn deinit = nullptr,
                                     void* ctx = nullptr) noexcept {
        return Node{
            name,
            phase,
            runlevel_mask,
            provides,
            {},
            init,
            deinit,
            ctx
        };
    }

    template <util::usize ProvideCount, util::usize RequireCount>
    constexpr Node make_binding_node(std::string_view name,
                                     Phase phase,
                                     util::u32 runlevel_mask,
                                     const std::array<CapId, ProvideCount>& provides,
                                     const std::array<CapId, RequireCount>& requires_caps,
                                     InitFn init = nullptr,
                                     DeinitFn deinit = nullptr,
                                     void* ctx = nullptr) noexcept {
        return Node{
            name,
            phase,
            runlevel_mask,
            std::span<const CapId>(provides.data(), provides.size()),
            std::span<const CapId>(requires_caps.data(), requires_caps.size()),
            init,
            deinit,
            ctx
        };
    }

    constexpr Node make_binding_node(std::string_view name,
                                     Phase phase,
                                     util::u32 runlevel_mask,
                                     std::span<const CapId> provides,
                                     std::span<const CapId> requires_caps,
                                     InitFn init = nullptr,
                                     DeinitFn deinit = nullptr,
                                     void* ctx = nullptr) noexcept {
        return Node{
            name,
            phase,
            runlevel_mask,
            provides,
            requires_caps,
            init,
            deinit,
            ctx
        };
    }

    template <typename Binding>
    constexpr std::string_view lookup_capability_name(const Binding& value, CapId id) noexcept {
        if constexpr (requires(const Binding& candidate) {
                          candidate.capability_name(id);
                      }) {
            return std::string_view{value.capability_name(id)};
        } else {
            return {};
        }
    }

    template <util::usize Count>
    constexpr const std::string_view* find_capability_name(
        CapId id,
        const std::array<CapId, Count>& caps,
        const std::array<std::string_view, Count>& names) noexcept {
        for (util::usize i = 0; i < Count; ++i) {
            if (caps[i] == id) {
                return &names[i];
            }
        }
        return nullptr;
    }

    constexpr const std::string_view* find_capability_name(
        CapId id,
        std::span<const CapId> caps,
        std::span<const std::string_view> names) noexcept {
        const auto count = caps.size() < names.size() ? caps.size() : names.size();
        for (util::usize i = 0; i < count; ++i) {
            if (caps[i] == id) {
                return &names[i];
            }
        }
        return nullptr;
    }

    template <util::usize Count>
    constexpr std::string_view lookup_capability_name(
        CapId id,
        const std::array<CapId, Count>& caps,
        const std::array<std::string_view, Count>& names) noexcept {
        if (const auto* name = find_capability_name(id, caps, names)) {
            return *name;
        }
        return {};
    }

    constexpr std::string_view lookup_capability_name(
        CapId id,
        std::span<const CapId> caps,
        std::span<const std::string_view> names) noexcept {
        if (const auto* name = find_capability_name(id, caps, names)) {
            return *name;
        }
        return {};
    }

    template <util::usize ProvideCount, util::usize RequireCount>
    constexpr std::string_view lookup_capability_name(
        CapId id,
        const std::array<CapId, ProvideCount>& provides,
        const std::array<std::string_view, ProvideCount>& provide_names,
        const std::array<CapId, RequireCount>& requires_caps,
        const std::array<std::string_view, RequireCount>& require_names) noexcept {
        if (const auto* name = find_capability_name(id, provides, provide_names)) {
            return *name;
        }
        if (const auto* name = find_capability_name(id, requires_caps, require_names)) {
            return *name;
        }
        return {};
    }

    constexpr std::string_view lookup_capability_name(
        CapId id,
        std::span<const CapId> provides,
        std::span<const std::string_view> provide_names,
        std::span<const CapId> requires_caps,
        std::span<const std::string_view> require_names) noexcept {
        if (const auto* name = find_capability_name(id, provides, provide_names)) {
            return *name;
        }
        if (const auto* name = find_capability_name(id, requires_caps, require_names)) {
            return *name;
        }
        return {};
    }
}
