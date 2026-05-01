module;

#include <array>
#include <cstddef>
#include <cstdint>

export module charm.ui.scene.layer_runtime;

export import charm.core.config;
export import charm.core.geometry;
export import charm.gfx.pixel_format;

export namespace ui::scene {
    enum class LayerState : std::uint8_t {
        Hidden,
        Live,
        Frozen,
        Transitioning,
        StaleSnapshot,
    };

    enum class SnapshotKind : std::uint8_t {
        CommandBuffer,
        PixelSurface,
        EmptyFallback,
    };

    struct LayerEpoch {
        std::uint32_t layout{0};
        std::uint32_t style{0};
        std::uint32_t theme{0};
        std::uint32_t density{0};
        std::uint32_t font{0};
        std::uint32_t content{0};
        std::uint32_t image{0};

        [[nodiscard]] constexpr bool operator==(const LayerEpoch&) const noexcept = default;
        [[nodiscard]] constexpr bool operator!=(const LayerEpoch& other) const noexcept {
            return !(*this == other);
        }
    };

    struct SnapshotHandle {
        std::uint16_t slot{0xFFFF};
        std::uint16_t generation{0};

        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return slot != 0xFFFF;
        }
    };

    struct SnapshotSpec {
        Rect bounds{};
        SnapshotKind preferred_kind{SnapshotKind::CommandBuffer};
        PixelFormat preferred_format{screen_pixel_format};
        bool allow_alpha{false};
        bool allow_partial{false};
    };

    struct LayerTransform {
        std::int16_t x{0};
        std::int16_t y{0};
        std::uint8_t opacity{255};
    };

    struct LayerStats {
        std::uint16_t snapshot_count{0};
        std::uint16_t snapshot_rebuild_count{0};
        std::uint16_t stale_snapshot_count{0};
        std::uint32_t layer_bytes{0};
        std::uint32_t composite_pixels{0};
    };

    struct SnapshotRecord {
        SnapshotKind kind{SnapshotKind::EmptyFallback};
        PixelFormat format{screen_pixel_format};
        Rect bounds{};
        LayerEpoch epoch{};
        std::uint32_t bytes{0};
        std::uint32_t command_count{0};
        std::uint16_t generation{0};
        bool occupied{false};
        bool stale{false};
    };

    constexpr std::size_t snapshot_pixel_bytes(PixelFormat format,
                                               int width,
                                               int height) noexcept {
        if (width <= 0 || height <= 0) return 0;
        const auto pixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        switch (format) {
        case PixelFormat::RGB565:
            return pixels * PixelTraits<PixelFormat::RGB565>::bytes_per_pixel;
        case PixelFormat::RGB888:
            return pixels * PixelTraits<PixelFormat::RGB888>::bytes_per_pixel;
        case PixelFormat::ARGB8888:
            return pixels * PixelTraits<PixelFormat::ARGB8888>::bytes_per_pixel;
        default:
            return pixels * PixelTraits<PixelFormat::ARGB8888>::bytes_per_pixel;
        }
    }

    template<std::size_t MaxSnapshots>
    class SnapshotStore {
    public:
        [[nodiscard]] constexpr std::size_t capacity() const noexcept {
            return MaxSnapshots;
        }

        [[nodiscard]] SnapshotHandle reserve(const SnapshotSpec& spec,
                                             const LayerEpoch& epoch) noexcept {
            for (std::size_t i = 0; i < slots_.size(); ++i) {
                auto& slot = slots_[i];
                if (!slot.occupied) {
                    slot.occupied = true;
                    slot.stale = false;
                    slot.kind = spec.preferred_kind;
                    slot.format = spec.preferred_format;
                    slot.bounds = spec.bounds;
                    slot.epoch = epoch;
                    slot.command_count = 0;
                    slot.bytes = snapshot_record_bytes(slot);
                    ++slot.generation;
                    if (slot.generation == 0) {
                        slot.generation = 1;
                    }
                    rebuild_stats();
                    return SnapshotHandle{
                        static_cast<std::uint16_t>(i),
                        slot.generation
                    };
                }
            }
            return {};
        }

        [[nodiscard]] bool release(SnapshotHandle handle) noexcept {
            auto* slot = mutable_record(handle);
            if (!slot) return false;
            const auto generation = slot->generation;
            *slot = {};
            slot->generation = generation;
            rebuild_stats();
            return true;
        }

        [[nodiscard]] const SnapshotRecord* record(SnapshotHandle handle) const noexcept {
            if (handle.slot >= slots_.size()) return nullptr;
            const auto& slot = slots_[handle.slot];
            if (!slot.occupied || slot.generation != handle.generation) return nullptr;
            return &slot;
        }

        [[nodiscard]] SnapshotRecord* mutable_record(SnapshotHandle handle) noexcept {
            if (handle.slot >= slots_.size()) return nullptr;
            auto& slot = slots_[handle.slot];
            if (!slot.occupied || slot.generation != handle.generation) return nullptr;
            return &slot;
        }

        [[nodiscard]] bool mark_stale(SnapshotHandle handle) noexcept {
            auto* slot = mutable_record(handle);
            if (!slot) return false;
            if (!slot->stale) {
                slot->stale = true;
                ++stats_.stale_snapshot_count;
            }
            return true;
        }

        [[nodiscard]] bool refresh_epoch(SnapshotHandle handle,
                                         const LayerEpoch& epoch) noexcept {
            auto* slot = mutable_record(handle);
            if (!slot) return false;
            slot->epoch = epoch;
            slot->stale = false;
            ++stats_.snapshot_rebuild_count;
            return true;
        }

        [[nodiscard]] bool update_command_snapshot(SnapshotHandle handle,
                                                   std::uint32_t command_count,
                                                   std::uint32_t command_bytes) noexcept {
            auto* slot = mutable_record(handle);
            if (!slot) return false;
            slot->kind = SnapshotKind::CommandBuffer;
            slot->command_count = command_count;
            slot->bytes = command_bytes;
            rebuild_stats();
            return true;
        }

        [[nodiscard]] bool update_pixel_snapshot(SnapshotHandle handle,
                                                 PixelFormat format,
                                                 std::uint32_t bytes) noexcept {
            auto* slot = mutable_record(handle);
            if (!slot) return false;
            slot->kind = SnapshotKind::PixelSurface;
            slot->format = format;
            slot->command_count = 0;
            slot->bytes = bytes;
            rebuild_stats();
            return true;
        }

        void note_composite_pixels(std::uint32_t pixels) noexcept {
            stats_.composite_pixels += pixels;
        }

        [[nodiscard]] LayerStats stats() const noexcept {
            return stats_;
        }

        void clear() noexcept {
            for (auto& slot : slots_) {
                slot = {};
            }
            stats_ = {};
        }

    private:
        static constexpr std::uint32_t snapshot_record_bytes(const SnapshotRecord& slot) noexcept {
            if (slot.kind != SnapshotKind::PixelSurface) return 0;
            return static_cast<std::uint32_t>(
                snapshot_pixel_bytes(slot.format, slot.bounds.w, slot.bounds.h));
        }

        void rebuild_stats() noexcept {
            const auto rebuilds = stats_.snapshot_rebuild_count;
            const auto stale = stats_.stale_snapshot_count;
            const auto composite = stats_.composite_pixels;
            stats_ = {};
            stats_.snapshot_rebuild_count = rebuilds;
            stats_.stale_snapshot_count = stale;
            stats_.composite_pixels = composite;
            for (const auto& slot : slots_) {
                if (!slot.occupied) continue;
                ++stats_.snapshot_count;
                stats_.layer_bytes += slot.bytes;
            }
        }

        std::array<SnapshotRecord, MaxSnapshots> slots_{};
        LayerStats stats_{};
    };

    using DefaultSnapshotStore =
        SnapshotStore<static_cast<std::size_t>(layer_cache_slots)>;
}
