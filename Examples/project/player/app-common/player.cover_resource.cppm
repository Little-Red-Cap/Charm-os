module;

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

export module player.cover_resource;

export namespace player {
    enum class DefaultCoverVariant : std::uint8_t {
        HomeHeroPill = 0,
        HomeOrbitDisc,
        HomeOrbitStack,
        HomeCenterPill,
        HomeBottomCard,
        HomeBottomCut,
    };

    enum class CoverResourceKind : std::uint8_t {
        Unknown = 0,
        EmbeddedTrack,
        FolderFile,
    };

    struct CoverResourceRequest {
        std::string_view path{};
        CoverResourceKind kind{CoverResourceKind::Unknown};
        DefaultCoverVariant fallback_variant{DefaultCoverVariant::HomeHeroPill};
    };

    struct CoverResourceView {
        std::string_view key{};
        std::span<const std::uint32_t> argb{};
        int width{0};
        int height{0};
    };

    using PlayerCoverResourceResolveFn =
        bool (*)(void* ctx, const CoverResourceRequest& request, CoverResourceView& out) noexcept;

    struct PlayerCoverResourceProviderBinding {
        void* ctx{nullptr};
        PlayerCoverResourceResolveFn resolve_fn{nullptr};

        [[nodiscard]] bool valid() const noexcept {
            return resolve_fn != nullptr;
        }

        friend constexpr bool operator==(const PlayerCoverResourceProviderBinding&,
                                         const PlayerCoverResourceProviderBinding&) = default;
    };

    void set_cover_resource_provider_binding(PlayerCoverResourceProviderBinding binding) noexcept;
    PlayerCoverResourceProviderBinding cover_resource_provider_binding() noexcept;
    bool resolve_cover_resource(const CoverResourceRequest& request, CoverResourceView& out) noexcept;
}

namespace player::detail {
    PlayerCoverResourceProviderBinding& active_cover_resource_provider_binding() noexcept {
        static PlayerCoverResourceProviderBinding binding{};
        return binding;
    }
}

namespace player {
    void set_cover_resource_provider_binding(PlayerCoverResourceProviderBinding binding) noexcept {
        detail::active_cover_resource_provider_binding() = binding;
    }

    PlayerCoverResourceProviderBinding cover_resource_provider_binding() noexcept {
        return detail::active_cover_resource_provider_binding();
    }

    bool resolve_cover_resource(const CoverResourceRequest& request, CoverResourceView& out) noexcept {
        const auto binding = detail::active_cover_resource_provider_binding();
        return binding.valid() && binding.resolve_fn(binding.ctx, request, out);
    }
}
