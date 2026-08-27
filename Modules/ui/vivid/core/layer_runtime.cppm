module;

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

    using PageLayerState = LayerState;

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
        UnsupportedTransform,
    };

    enum class LayerCaptureStatus : std::uint8_t {
        Ok,
        NoSnapshotSlot,
        RecordFailed,
        StoreFailed,
        UnsupportedKind,
    };

    enum class LayerProfile : std::uint8_t {
        Rich,
        Cheap,
        Static,
        Eink,
        None,
    };

    enum class LayerFallbackReason : std::uint8_t {
        None,
        LayerBytesOver,
        CompositePixelsOver,
        CommandCountOver,
        AlphaUnsupported,
        PixelSurfaceUnsupported,
        DoubleSnapshotUnsupported,
        Disabled,
        StaleSnapshot,
    };

    enum class LayerAdmission : std::uint8_t {
        PixelDouble,
        PixelSingle,
        CommandSnapshot,
        StaticCut,
        Reject,
    };

    struct LayerProfileCaps {
        bool allow_pixel_surface{true};
        bool allow_command_snapshot{true};
        bool allow_opacity{true};
        bool allow_slide{true};
        bool allow_alpha_blend{true};
        bool allow_double_snapshot{true};
        std::uint8_t opacity_steps{255};
        std::uint16_t target_fps{60};
        std::uint32_t max_layer_bytes{0};
        std::uint32_t max_composite_pixels_per_frame{0};
        std::uint32_t max_command_count{0};
    };

    struct LayerProfileDecision {
        LayerProfile requested{LayerProfile::Rich};
        LayerProfile effective{LayerProfile::Rich};
        LayerFallbackReason reason{LayerFallbackReason::None};
    };

    struct SnapshotHandle {
        std::uint16_t slot{0xFFFF};
        std::uint16_t generation{0};

        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return slot != 0xFFFF;
        }

        [[nodiscard]] constexpr bool operator==(const SnapshotHandle&) const noexcept = default;
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
        std::uint32_t pixel_blit_count{0};
        std::uint32_t pixel_blit_pixels{0};
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

    struct LayerBudget {
        std::uint32_t max_layer_bytes{0};
        std::uint32_t max_composite_pixels{0};
        std::uint32_t max_command_count{0};
    };

    struct LayerAdmissionSpec {
        LayerProfile profile{LayerProfile::Rich};
        LayerBudget budget{};
        std::uint32_t pixel_snapshot_bytes{0};
        std::uint16_t cache_slots{0};
        bool need_double_snapshot{true};
        bool command_snapshot_enabled{snapshot_command_enabled};
        bool pixel_snapshot_enabled{snapshot_pixel_enabled};
    };

    struct LayerBudgetResult {
        bool ok{true};
        bool layer_bytes_over{false};
        bool composite_pixels_over{false};
        bool command_count_over{false};
    };

    constexpr const char* layer_profile_name(LayerProfile profile) noexcept {
        switch (profile) {
        case LayerProfile::Rich: return "rich";
        case LayerProfile::Cheap: return "cheap";
        case LayerProfile::Static: return "static";
        case LayerProfile::Eink: return "eink";
        case LayerProfile::None: return "none";
        }
        return "unknown";
    }

    constexpr const char* layer_fallback_reason_name(LayerFallbackReason reason) noexcept {
        switch (reason) {
        case LayerFallbackReason::None: return "none";
        case LayerFallbackReason::LayerBytesOver: return "layer_bytes";
        case LayerFallbackReason::CompositePixelsOver: return "composite_pixels";
        case LayerFallbackReason::CommandCountOver: return "command_count";
        case LayerFallbackReason::AlphaUnsupported: return "alpha_unsupported";
        case LayerFallbackReason::PixelSurfaceUnsupported: return "pixel_surface_unsupported";
        case LayerFallbackReason::DoubleSnapshotUnsupported: return "double_snapshot_unsupported";
        case LayerFallbackReason::Disabled: return "disabled";
        case LayerFallbackReason::StaleSnapshot: return "stale_snapshot";
        }
        return "unknown";
    }

    constexpr const char* layer_admission_name(LayerAdmission admission) noexcept {
        switch (admission) {
        case LayerAdmission::PixelDouble: return "pixel_double";
        case LayerAdmission::PixelSingle: return "pixel_single";
        case LayerAdmission::CommandSnapshot: return "command_snapshot";
        case LayerAdmission::StaticCut: return "static_cut";
        case LayerAdmission::Reject: return "reject";
        }
        return "unknown";
    }

    constexpr LayerProfileCaps layer_profile_caps(LayerProfile profile) noexcept {
        switch (profile) {
        case LayerProfile::Rich:
            return {
                .allow_pixel_surface = true,
                .allow_command_snapshot = true,
                .allow_opacity = true,
                .allow_slide = true,
                .allow_alpha_blend = true,
                .allow_double_snapshot = true,
                .opacity_steps = 255,
                .target_fps = 60,
                .max_layer_bytes = 0,
                .max_composite_pixels_per_frame = 0,
                .max_command_count = 0,
            };
        case LayerProfile::Cheap:
            return {
                .allow_pixel_surface = true,
                .allow_command_snapshot = true,
                .allow_opacity = true,
                .allow_slide = true,
                .allow_alpha_blend = true,
                .allow_double_snapshot = true,
                .opacity_steps = 4,
                .target_fps = 30,
                .max_layer_bytes = 0,
                .max_composite_pixels_per_frame = 0,
                .max_command_count = 0,
            };
        case LayerProfile::Static:
            return {
                .allow_pixel_surface = true,
                .allow_command_snapshot = true,
                .allow_opacity = false,
                .allow_slide = false,
                .allow_alpha_blend = false,
                .allow_double_snapshot = false,
                .opacity_steps = 2,
                .target_fps = 1,
                .max_layer_bytes = 0,
                .max_composite_pixels_per_frame = 0,
                .max_command_count = 0,
            };
        case LayerProfile::Eink:
            return {
                .allow_pixel_surface = false,
                .allow_command_snapshot = true,
                .allow_opacity = false,
                .allow_slide = false,
                .allow_alpha_blend = false,
                .allow_double_snapshot = false,
                .opacity_steps = 2,
                .target_fps = 1,
                .max_layer_bytes = 0,
                .max_composite_pixels_per_frame = 0,
                .max_command_count = 0,
            };
        case LayerProfile::None:
            return {
                .allow_pixel_surface = false,
                .allow_command_snapshot = false,
                .allow_opacity = false,
                .allow_slide = false,
                .allow_alpha_blend = false,
                .allow_double_snapshot = false,
                .opacity_steps = 1,
                .target_fps = 0,
                .max_layer_bytes = 0,
                .max_composite_pixels_per_frame = 0,
                .max_command_count = 0,
            };
        }
        return layer_profile_caps(LayerProfile::None);
    }

    constexpr LayerProfile degrade_layer_profile(LayerProfile profile) noexcept {
        switch (profile) {
        case LayerProfile::Rich: return LayerProfile::Cheap;
        case LayerProfile::Cheap: return LayerProfile::Static;
        case LayerProfile::Static: return LayerProfile::Eink;
        case LayerProfile::Eink: return LayerProfile::None;
        case LayerProfile::None: return LayerProfile::None;
        }
        return LayerProfile::None;
    }

    constexpr std::uint8_t resolve_layer_opacity(LayerProfile profile,
                                                std::uint8_t requested) noexcept {
        switch (profile) {
        case LayerProfile::Rich:
            return requested;
        case LayerProfile::Cheap:
            if (requested == 0 || requested == 255) return requested;
            return static_cast<std::uint8_t>(((requested + 42u) / 85u) * 85u);
        case LayerProfile::Static:
            return requested >= 128 ? 255 : 0;
        case LayerProfile::Eink:
        case LayerProfile::None:
            return requested == 255 ? 255 : 0;
        }
        return 0;
    }

    constexpr LayerFallbackReason layer_budget_fallback_reason(
        const LayerBudgetResult& budget) noexcept {
        if (budget.layer_bytes_over) return LayerFallbackReason::LayerBytesOver;
        if (budget.composite_pixels_over) return LayerFallbackReason::CompositePixelsOver;
        if (budget.command_count_over) return LayerFallbackReason::CommandCountOver;
        return LayerFallbackReason::None;
    }

    constexpr LayerProfileDecision decide_layer_profile(
        LayerProfile requested,
        const LayerBudgetResult& budget) noexcept {
        LayerProfileDecision decision{
            .requested = requested,
            .effective = requested,
            .reason = LayerFallbackReason::None,
        };
        const auto reason = layer_budget_fallback_reason(budget);
        if (reason != LayerFallbackReason::None) {
            decision.effective = degrade_layer_profile(requested);
            decision.reason = reason;
        }
        return decision;
    }

    constexpr LayerAdmission decide_layer_admission(
        const LayerAdmissionSpec& spec) noexcept {
        const auto caps = layer_profile_caps(spec.profile);
        const bool command_available =
            snapshot_command_enabled && spec.command_snapshot_enabled;
        const bool pixel_available =
            snapshot_pixel_enabled && spec.pixel_snapshot_enabled;
        if (spec.profile == LayerProfile::None) return LayerAdmission::Reject;
        if (spec.profile == LayerProfile::Static) return LayerAdmission::StaticCut;
        if (spec.profile == LayerProfile::Eink) {
            return caps.allow_command_snapshot
                    && command_available
                    && spec.cache_slots > 0
                ? LayerAdmission::CommandSnapshot
                : LayerAdmission::StaticCut;
        }

        const std::uint32_t requested_pixel_bytes =
            spec.pixel_snapshot_bytes * (spec.need_double_snapshot ? 2u : 1u);
        const bool has_budget = spec.budget.max_layer_bytes == 0
            || requested_pixel_bytes <= spec.budget.max_layer_bytes;
        const bool has_slots = spec.cache_slots >= (spec.need_double_snapshot ? 2u : 1u);
        if (caps.allow_pixel_surface
            && pixel_available
            && has_budget
            && has_slots) {
            return spec.need_double_snapshot
                ? LayerAdmission::PixelDouble
                : LayerAdmission::PixelSingle;
        }
        if (caps.allow_pixel_surface &&
            pixel_available &&
            spec.cache_slots >= 1u &&
            (spec.budget.max_layer_bytes == 0 ||
             spec.pixel_snapshot_bytes <= spec.budget.max_layer_bytes)) {
            return LayerAdmission::PixelSingle;
        }
        if (caps.allow_command_snapshot
            && command_available
            && spec.cache_slots > 0) {
            return LayerAdmission::CommandSnapshot;
        }
        return LayerAdmission::StaticCut;
    }

    static_assert(decide_layer_admission({
        .profile = LayerProfile::Rich,
        .cache_slots = 0,
    }) == LayerAdmission::StaticCut);
    static_assert(decide_layer_admission({
        .profile = LayerProfile::Eink,
        .cache_slots = 0,
    }) == LayerAdmission::StaticCut);
    static_assert(decide_layer_admission({
        .profile = LayerProfile::Rich,
        .cache_slots = 2,
        .command_snapshot_enabled = true,
        .pixel_snapshot_enabled = false,
    }) == (snapshot_command_enabled
               ? LayerAdmission::CommandSnapshot
               : LayerAdmission::StaticCut));
    static_assert(decide_layer_admission({
        .profile = LayerProfile::Rich,
        .cache_slots = 2,
        .command_snapshot_enabled = false,
        .pixel_snapshot_enabled = true,
    }) == (snapshot_pixel_enabled
               ? LayerAdmission::PixelDouble
               : LayerAdmission::StaticCut));
    static_assert(decide_layer_admission({
        .profile = LayerProfile::Rich,
        .cache_slots = 2,
        .command_snapshot_enabled = false,
        .pixel_snapshot_enabled = false,
    }) == LayerAdmission::StaticCut);

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

}
