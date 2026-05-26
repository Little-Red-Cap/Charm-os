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

    struct PlayerCoverResourceRecord {
        std::string_view path{};
        CoverResourceKind kind{CoverResourceKind::Unknown};
        std::string_view key{};
        std::span<const std::uint32_t> argb{};
        int width{0};
        int height{0};

        [[nodiscard]] bool valid() const noexcept {
            if (path.empty() || width <= 0 || height <= 0) {
                return false;
            }
            const auto pixel_count =
                static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
            return pixel_count > 0 && argb.size() >= pixel_count;
        }
    };

    struct PlayerCoverResourceRecordTableView {
        std::span<const PlayerCoverResourceRecord> records{};

        [[nodiscard]] bool valid() const noexcept {
            return !records.empty();
        }
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
    PlayerCoverResourceProviderBinding make_cover_resource_record_table_binding(
        const PlayerCoverResourceRecordTableView& table) noexcept;
    bool resolve_cover_resource(const CoverResourceRequest& request, CoverResourceView& out) noexcept;
    bool resolve_cover_resource_record_table(const PlayerCoverResourceRecordTableView& table,
                                             const CoverResourceRequest& request,
                                             CoverResourceView& out) noexcept;
}

namespace player::detail {
    PlayerCoverResourceProviderBinding& active_cover_resource_provider_binding() noexcept {
        static PlayerCoverResourceProviderBinding binding{};
        return binding;
    }

    bool cover_resource_record_matches(const PlayerCoverResourceRecord& record,
                                       const CoverResourceRequest& request) noexcept {
        if (record.path != request.path) {
            return false;
        }
        if (record.kind == request.kind) {
            return true;
        }
        return record.kind == CoverResourceKind::Unknown || request.kind == CoverResourceKind::Unknown;
    }

    void assign_cover_resource_record_view(const PlayerCoverResourceRecord& record,
                                           CoverResourceView& out) noexcept {
        const auto pixel_count =
            static_cast<std::size_t>(record.width) * static_cast<std::size_t>(record.height);
        out.key = record.key;
        out.argb = record.argb.first(pixel_count);
        out.width = record.width;
        out.height = record.height;
    }

    bool resolve_cover_resource_record_table_binding(void* ctx,
                                                     const CoverResourceRequest& request,
                                                     CoverResourceView& out) noexcept {
        if (!ctx) {
            return false;
        }
        return resolve_cover_resource_record_table(
            *static_cast<const PlayerCoverResourceRecordTableView*>(ctx),
            request,
            out);
    }
}

namespace player {
    void set_cover_resource_provider_binding(PlayerCoverResourceProviderBinding binding) noexcept {
        detail::active_cover_resource_provider_binding() = binding;
    }

    PlayerCoverResourceProviderBinding cover_resource_provider_binding() noexcept {
        return detail::active_cover_resource_provider_binding();
    }

    PlayerCoverResourceProviderBinding make_cover_resource_record_table_binding(
        const PlayerCoverResourceRecordTableView& table) noexcept {
        if (!table.valid()) {
            return {};
        }
        return PlayerCoverResourceProviderBinding{
            .ctx = const_cast<PlayerCoverResourceRecordTableView*>(&table),
            .resolve_fn = &detail::resolve_cover_resource_record_table_binding,
        };
    }

    bool resolve_cover_resource(const CoverResourceRequest& request, CoverResourceView& out) noexcept {
        const auto binding = detail::active_cover_resource_provider_binding();
        return binding.valid() && binding.resolve_fn(binding.ctx, request, out);
    }

    bool resolve_cover_resource_record_table(const PlayerCoverResourceRecordTableView& table,
                                             const CoverResourceRequest& request,
                                             CoverResourceView& out) noexcept {
        out = {};
        if (!table.valid() || request.path.empty()) {
            return false;
        }

        const PlayerCoverResourceRecord* wildcard_match = nullptr;
        for (const auto& record : table.records) {
            if (!record.valid() || record.path != request.path) {
                continue;
            }
            if (record.kind == request.kind) {
                detail::assign_cover_resource_record_view(record, out);
                return true;
            }
            if (!wildcard_match && detail::cover_resource_record_matches(record, request)) {
                wildcard_match = &record;
            }
        }

        if (!wildcard_match) {
            return false;
        }
        detail::assign_cover_resource_record_view(*wildcard_match, out);
        return true;
    }
}
