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

    enum class LayerReplayStatus : std::uint8_t {
        Ok,
        InvalidPlan,
        UnsupportedKind,
        MissingSnapshot,
        StaleSnapshot,
        MissingPayload,
        ExecuteFailed,
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

    inline constexpr std::uint32_t kInvalidSnapshotPayloadSlot = 0xFFFFFFFFu;

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

    struct LayerComposeSpec {
        SnapshotHandle source{};
        LayerTransform transform{};
        Rect clip{};
        bool has_clip{false};
    };

    struct LayerComposeResult {
        bool ok{false};
        bool stale{false};
        SnapshotKind kind{SnapshotKind::EmptyFallback};
        Rect source_bounds{};
        Rect source_visible{};
        Rect target_bounds{};
        std::uint32_t composite_pixels{0};
        std::uint32_t source_bytes{0};
    };

    struct LayerComposePlan {
        bool valid{false};
        SnapshotHandle source{};
        SnapshotKind kind{SnapshotKind::EmptyFallback};
        Rect source_bounds{};
        Rect source_visible{};
        Rect target_bounds{};
        LayerTransform transform{};
        std::uint32_t composite_pixels{0};
        std::uint32_t source_bytes{0};
    };

    struct LayerBudget {
        std::uint32_t max_layer_bytes{0};
        std::uint32_t max_composite_pixels{0};
        std::uint32_t max_command_count{0};
    };

    struct LayerBudgetResult {
        bool ok{true};
        bool layer_bytes_over{false};
        bool composite_pixels_over{false};
        bool command_count_over{false};
    };

    struct SnapshotRecord {
        SnapshotKind kind{SnapshotKind::EmptyFallback};
        PixelFormat format{screen_pixel_format};
        Rect bounds{};
        LayerEpoch epoch{};
        std::uint32_t bytes{0};
        std::uint32_t command_count{0};
        std::uint32_t payload_slot{kInvalidSnapshotPayloadSlot};
        std::uint16_t generation{0};
        bool occupied{false};
        bool stale{false};
    };

    constexpr bool layer_rect_empty(const Rect& rect) noexcept {
        return rect.w <= 0 || rect.h <= 0;
    }

    constexpr Rect layer_translate_rect(Rect rect,
                                        const LayerTransform& transform) noexcept {
        rect.x += transform.x;
        rect.y += transform.y;
        return rect;
    }

    constexpr Rect layer_intersect_rect(const Rect& a, const Rect& b) noexcept {
        const int left = (a.x > b.x) ? a.x : b.x;
        const int top = (a.y > b.y) ? a.y : b.y;
        const int right_a = a.x + a.w;
        const int right_b = b.x + b.w;
        const int bottom_a = a.y + a.h;
        const int bottom_b = b.y + b.h;
        const int right = (right_a < right_b) ? right_a : right_b;
        const int bottom = (bottom_a < bottom_b) ? bottom_a : bottom_b;
        return {left, top, right - left, bottom - top};
    }

    constexpr Rect layer_inverse_translate_rect(Rect rect,
                                                const LayerTransform& transform) noexcept {
        rect.x -= transform.x;
        rect.y -= transform.y;
        return rect;
    }

    constexpr std::uint32_t layer_rect_area(const Rect& rect) noexcept {
        if (layer_rect_empty(rect)) return 0;
        return static_cast<std::uint32_t>(rect.w) * static_cast<std::uint32_t>(rect.h);
    }

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

        [[nodiscard]] LayerComposeResult compose_dry_run(const LayerComposeSpec& spec) noexcept {
            LayerComposeResult result{};
            const auto plan = make_compose_plan(spec);
            result.ok = plan.valid;
            result.kind = plan.kind;
            result.source_bounds = plan.source_bounds;
            result.source_visible = plan.source_visible;
            result.target_bounds = plan.target_bounds;
            result.composite_pixels = plan.composite_pixels;
            result.source_bytes = plan.source_bytes;
            if (result.ok) {
                note_composite_pixels(result.composite_pixels);
            } else if (const auto* slot = record(spec.source)) {
                result.stale = slot->stale;
            }
            return result;
        }

        [[nodiscard]] LayerComposePlan make_compose_plan(const LayerComposeSpec& spec) const noexcept {
            LayerComposePlan plan{};
            const auto* slot = record(spec.source);
            if (!slot || slot->stale) return plan;
            plan.source = spec.source;
            plan.kind = slot->kind;
            plan.source_bounds = slot->bounds;
            plan.transform = spec.transform;
            plan.source_bytes = slot->bytes;
            Rect target = layer_translate_rect(slot->bounds, spec.transform);
            if (spec.has_clip) {
                target = layer_intersect_rect(target, spec.clip);
            }
            plan.target_bounds = target;
            plan.composite_pixels = layer_rect_area(target);
            if (plan.composite_pixels == 0) return plan;
            plan.source_visible = layer_inverse_translate_rect(target, spec.transform);
            plan.valid = true;
            return plan;
        }

        [[nodiscard]] LayerBudgetResult check_budget(const LayerComposePlan& plan,
                                                     const LayerBudget& budget) const noexcept {
            LayerBudgetResult result{};
            const auto* slot = record(plan.source);
            if (budget.max_layer_bytes > 0 && stats_.layer_bytes > budget.max_layer_bytes) {
                result.layer_bytes_over = true;
            }
            if (budget.max_composite_pixels > 0 &&
                plan.composite_pixels > budget.max_composite_pixels) {
                result.composite_pixels_over = true;
            }
            if (slot && budget.max_command_count > 0 &&
                slot->command_count > budget.max_command_count) {
                result.command_count_over = true;
            }
            result.ok = !result.layer_bytes_over
                && !result.composite_pixels_over
                && !result.command_count_over;
            return result;
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
