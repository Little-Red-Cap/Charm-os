module;

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <array>
#include <type_traits>

#include "vivid_features.generated.hpp"

export module charm.gfx.draw_cmd:schema;

import charm.core.geometry;
import charm.gfx.canvas;
import charm.gfx.color;
import charm.gfx.framebuffer;
import charm.gfx.image;
import charm.gfx.path;
import charm.gfx.render_core;
import charm.font;
import charm.font.typography;
import charm.gfx.text_box;
import ui.render_backend;

export namespace ui::draw_cmd {
    constexpr std::size_t bytes_per_pixel(PixelFormat fmt) noexcept {
        switch (fmt) {
            case PixelFormat::RGB565: return PixelTraits<PixelFormat::RGB565>::bytes_per_pixel;
            case PixelFormat::RGB888: return PixelTraits<PixelFormat::RGB888>::bytes_per_pixel;
            case PixelFormat::ARGB8888: return PixelTraits<PixelFormat::ARGB8888>::bytes_per_pixel;
            default: return PixelTraits<PixelFormat::ARGB8888>::bytes_per_pixel;
        }
    }

    enum class CmdType : std::uint8_t {
        PushClip,
        PopClip,
        DrawLine,
        DrawPath,
        FillRect,
        FillLinearGradientRect,
        StrokeRect,
        FillRoundRect,
        StrokeRoundRect,
        FillCircle,
        StrokeCircle,
        DrawImage,
        DrawImageRoundRect,
        DrawImageNineSlice,
        DrawTextBox,
        FocusRing,
        FillRectBatch,
        StrokeRectBatch,
        FillRoundRectBatch,
        StrokeRoundRectBatch,
        FillCircleBatch,
        StrokeCircleBatch,
        GlyphRun,
        DrawImageBatch,
        DrawImageRoundRectBatch,
        DrawImageNineSliceBatch,
        DrawLineBatch,
        DrawPathBatch,
        FocusRingBatch,
    };

    inline constexpr std::size_t kCmdTypeCount =
        static_cast<std::size_t>(CmdType::FocusRingBatch) + 1;

    inline constexpr std::uint16_t kDrawScopeDefault = 0;
    inline constexpr std::size_t kDrawScopeDetailCapacity = 32;

    struct DrawScope {
        std::uint16_t id{kDrawScopeDefault};
    };

    constexpr DrawScope draw_scope_default() noexcept {
        return DrawScope{kDrawScopeDefault};
    }

    constexpr bool draw_scope_equal(DrawScope a, DrawScope b) noexcept {
        return a.id == b.id;
    }

    constexpr const char* cmd_type_name(CmdType type) noexcept {
        switch (type) {
        case CmdType::PushClip: return "PushClip";
        case CmdType::PopClip: return "PopClip";
        case CmdType::DrawLine: return "DrawLine";
        case CmdType::DrawPath: return "DrawPath";
        case CmdType::FillRect: return "FillRect";
        case CmdType::FillLinearGradientRect: return "FillLinearGradientRect";
        case CmdType::StrokeRect: return "StrokeRect";
        case CmdType::FillRoundRect: return "FillRoundRect";
        case CmdType::StrokeRoundRect: return "StrokeRoundRect";
        case CmdType::FillCircle: return "FillCircle";
        case CmdType::StrokeCircle: return "StrokeCircle";
        case CmdType::DrawImage: return "DrawImage";
        case CmdType::DrawImageRoundRect: return "DrawImageRoundRect";
        case CmdType::DrawImageNineSlice: return "DrawImageNineSlice";
        case CmdType::DrawTextBox: return "DrawTextBox";
        case CmdType::FocusRing: return "FocusRing";
        case CmdType::FillRectBatch: return "FillRectBatch";
        case CmdType::StrokeRectBatch: return "StrokeRectBatch";
        case CmdType::FillRoundRectBatch: return "FillRoundRectBatch";
        case CmdType::StrokeRoundRectBatch: return "StrokeRoundRectBatch";
        case CmdType::FillCircleBatch: return "FillCircleBatch";
        case CmdType::StrokeCircleBatch: return "StrokeCircleBatch";
        case CmdType::GlyphRun: return "GlyphRun";
        case CmdType::DrawImageBatch: return "DrawImageBatch";
        case CmdType::DrawImageRoundRectBatch: return "DrawImageRoundRectBatch";
        case CmdType::DrawImageNineSliceBatch: return "DrawImageNineSliceBatch";
        case CmdType::DrawLineBatch: return "DrawLineBatch";
        case CmdType::DrawPathBatch: return "DrawPathBatch";
        case CmdType::FocusRingBatch: return "FocusRingBatch";
        }
        return "Unknown";
    }

    struct TextSpan {
        std::uint32_t offset{0};
        std::uint16_t length{0};
    };

    struct BlobRef {
        std::uint32_t offset{0};
        std::uint32_t length{0};
    };

    using ImageId = ui::gfx::ImageId;
    using ImageRegisterReason = ui::gfx::ImageRegisterReason;
    using ImageRegistryStats = ui::gfx::ImageRegistryStats;
    using ImageRegistryEntry = ui::gfx::ImageRegistryEntry;
    using ImageAsset = ui::gfx::ImageAsset;
    using ImageBundleResult = ui::gfx::ImageBundleResult;
    using ui::gfx::invalid_image_id;
    using ui::gfx::image_id_valid;
    using ui::gfx::image_register_reason_name;
    using ui::gfx::register_image;
    using ui::gfx::register_image_key;
    using ui::gfx::register_image_dedup;
    using ui::gfx::register_image_bundle;
    using ui::gfx::unregister_image;
    using ui::gfx::resolve_image;
    using ui::gfx::clear_image_registry;
    using ui::gfx::register_image_with_id;
    using ui::gfx::image_registry_stats;
    using ui::gfx::set_image_registry_locked;
    using ui::gfx::image_registry_locked;
    using ui::gfx::image_registry_capacity;
    using ui::gfx::image_registry_entry;
    using ui::gfx::image_registry_register_after_lock;
    using ui::gfx::image_registry_first_after_lock_tag;
    using ui::gfx::image_registry_first_after_lock_reason;
    using ui::gfx::ImageRegistryLockGuard;
    using ui::gfx::ImageRegistryPhaseGuard;

    struct CmdHeader {
        CmdType type{CmdType::FillRect};
        std::uint8_t flags{0};
        std::uint16_t size{0};
        Rect rect{};
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
        std::uint16_t draw_scope{kDrawScopeDefault};
        std::uint16_t reserved{0};
#endif
    };

    struct CmdColor {
        rgba color{};
    };

    struct CmdColorP0 {
        rgba color{};
        std::int16_t p0{0};
        std::int16_t pad{0};
    };

    struct CmdColorP0P1P2 {
        rgba color{};
        std::int16_t p0{0};
        std::int16_t p1{0};
        std::int16_t p2{0};
        std::int16_t pad{0};
    };

    struct CmdColorP0P1P2P3 {
        rgba color{};
        std::int16_t p0{0};
        std::int16_t p1{0};
        std::int16_t p2{0};
        std::int16_t p3{0};
    };

    struct CmdTextBox {
        rgba color{};
        TextSpan text{};
        const Font* font_ptr{nullptr};
        FontId font{FontId::Normal};
        TextAlignH align_h{TextAlignH::Left};
        TextAlignV align_v{TextAlignV::Top};
        TextWrap wrap{TextWrap::None};
        TextEllipsis ellipsis{TextEllipsis::None};
    };

    struct CmdPath {
        rgba color{};
        BlobRef blob{};
        std::int16_t count{0};
        std::int16_t closed{0};
    };

    struct CmdImage {
        ImageId image{};
    };

    struct CmdImageP0 {
        ImageId image{};
        std::int16_t p0{0};
        std::int16_t pad{0};
    };

    struct CmdImageP0P1 {
        ImageId image{};
        std::int16_t p0{0};
        std::int16_t p1{0};
    };

    struct CmdImageP0P1P2 {
        ImageId image{};
        std::int16_t p0{0};
        std::int16_t p1{0};
        std::int16_t p2{0};
        std::int16_t pad{0};
    };

    struct CmdImageP0P1P2P3 {
        ImageId image{};
        std::int16_t p0{0};
        std::int16_t p1{0};
        std::int16_t p2{0};
        std::int16_t p3{0};
    };

    struct CmdBatchRect {
        rgba color{};
        BlobRef blob{};
        std::int16_t count{0};
        std::int16_t pad{0};
    };

    struct CmdBatchLine {
        rgba color{};
        BlobRef blob{};
        std::int16_t count{0};
        std::int16_t pad{0};
    };

    struct CmdBatchPath {
        rgba color{};
        BlobRef blob{};
        std::int16_t count{0};
        std::int16_t pad{0};
    };

    struct CmdBatchRectP0 {
        rgba color{};
        BlobRef blob{};
        std::int16_t p0{0};
        std::int16_t count{0};
    };

    struct CmdBatchRectP0P1P2 {
        rgba color{};
        BlobRef blob{};
        std::int16_t p0{0};
        std::int16_t p1{0};
        std::int16_t p2{0};
        std::int16_t count{0};
    };

    struct CmdGlyphRun {
        rgba color{};
        BlobRef blob{};
        const Font* font_ptr{nullptr};
        FontId font{FontId::Normal};
        TextAlignH align_h{TextAlignH::Left};
        TextAlignV align_v{TextAlignV::Top};
        TextWrap wrap{TextWrap::None};
        TextEllipsis ellipsis{TextEllipsis::None};
        std::uint16_t count{0};
        std::uint16_t pad{0};
    };

    struct CmdImageBatch {
        ImageId image{};
        BlobRef blob{};
        std::int16_t count{0};
        std::int16_t pad{0};
    };

    struct CmdImageBatchP0 {
        ImageId image{};
        BlobRef blob{};
        std::int16_t p0{0};
        std::int16_t count{0};
    };

    struct CmdImageBatchP0P1 {
        ImageId image{};
        BlobRef blob{};
        std::int16_t p0{0};
        std::int16_t p1{0};
        std::int16_t count{0};
        std::int16_t pad{0};
    };

    struct CmdImageBatchP0P1P2 {
        ImageId image{};
        BlobRef blob{};
        std::int16_t p0{0};
        std::int16_t p1{0};
        std::int16_t p2{0};
        std::int16_t count{0};
    };

    struct CmdImageBatchP0P1P2P3 {
        ImageId image{};
        BlobRef blob{};
        std::int16_t p0{0};
        std::int16_t p1{0};
        std::int16_t p2{0};
        std::int16_t p3{0};
    };


    struct DrawCmd {
        CmdType type{CmdType::FillRect};
        Rect rect{};
        rgba color{};
        std::int16_t p0{0};
        std::int16_t p1{0};
        std::int16_t p2{0};
        std::int16_t p3{0};
        TextSpan text{};
        BlobRef blob{};
        ImageId image{};
        const Font* font_ptr{nullptr};
        FontId font{FontId::Normal};
        TextAlignH align_h{TextAlignH::Left};
        TextAlignV align_v{TextAlignV::Top};
        TextWrap wrap{TextWrap::None};
        TextEllipsis ellipsis{TextEllipsis::None};
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
        DrawScope draw_scope{};
#endif
    };

    constexpr bool draw_cmd_scope_equal(const DrawCmd& a, const DrawCmd& b) noexcept {
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
        return draw_scope_equal(a.draw_scope, b.draw_scope);
#else
        (void)a;
        (void)b;
        return true;
#endif
    }

    constexpr std::size_t kCmdAlign = alignof(std::uint32_t);
    static_assert(sizeof(CmdHeader) % kCmdAlign == 0);

    constexpr std::uint32_t kDrawCmdBinaryVersion = 4;

    constexpr std::uint32_t draw_cmd_binary_size() noexcept {
        return static_cast<std::uint32_t>(sizeof(CmdHeader));
    }

    static_assert(sizeof(ImageId) == 4);
    static_assert(std::is_trivially_copyable_v<DrawCmd>);

    struct DrawCmdStats {
        std::size_t cmd_count{0};
        std::size_t cmd_capacity{0};
        std::size_t cmd_bytes{0};
        std::size_t text_used{0};
        std::size_t text_capacity{0};
        std::size_t blob_used{0};
        std::size_t blob_capacity{0};
        std::size_t batch_shrink{0};
        std::size_t batch_shrink_line{0};
        std::size_t batch_shrink_path{0};
        std::size_t batch_shrink_rect{0};
        std::size_t batch_shrink_round{0};
        std::size_t batch_shrink_image{0};
        std::size_t batch_shrink_focus{0};
        bool cmd_overflowed{false};
        bool text_overflowed{false};
        bool blob_overflowed{false};
    };

    struct DrawCmdExecStats {
        std::size_t cmd_count{0};
        std::size_t cmd_bytes{0};
        std::size_t clip_pushes{0};
        std::size_t clip_pops{0};
        std::size_t clip_push_overflow{0};
        std::size_t clip_pop_underflow{0};
        std::size_t clip_invalid{0};
        std::size_t failed_cmds{0};
        std::size_t fail_text{0};
        std::size_t fail_image{0};
        std::size_t fail_blob{0};
        std::size_t fail_path{0};
        std::size_t fail_clip{0};
        std::size_t fail_other{0};
        std::size_t dispatch_groups{0};
        std::size_t batch_flushes{0};
        std::size_t group_rect{0};
        std::size_t group_text{0};
        std::size_t group_image{0};
        std::size_t group_line{0};
        std::size_t group_path{0};
        std::size_t group_other{0};
        std::size_t cmd_rect{0};
        std::size_t cmd_text{0};
        std::size_t cmd_image{0};
        std::size_t cmd_line{0};
        std::size_t cmd_path{0};
        std::size_t cmd_other{0};
        bool overflowed{false};
    };

#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
    struct DrawCmdTypeDetail {
        std::uint32_t count{0};
        std::uint64_t rect_area{0};
        std::uint64_t actual_alpha_pixels{0};
        std::uint32_t max_rect_area{0};
    };

    struct DrawScopeDetail {
        std::uint16_t scope_id{kDrawScopeDefault};
        std::uint16_t active{0};
        std::uint32_t cmd_count{0};
        std::uint64_t rect_area{0};
        std::uint64_t actual_alpha_pixels{0};
    };

    using DrawCmdTypeDetailTable = std::array<DrawCmdTypeDetail, kCmdTypeCount>;
    using DrawScopeDetailTable = std::array<DrawScopeDetail, kDrawScopeDetailCapacity>;

    struct DrawCmdDetailStats {
        DrawCmdTypeDetailTable types{};
        DrawScopeDetailTable scopes{};
        std::uint32_t scope_overflow{0};
    };
#else
    struct DrawCmdDetailStats {};
#endif

    struct RectBatchItem {
        Rect rect{};
    };

    struct LineBatchItem {
        int x0{0};
        int y0{0};
        int x1{0};
        int y1{0};
    };

    struct PathBatchItem {
        BlobRef blob{};
        std::int16_t count{0};
        std::int16_t closed{0};
    };

    struct GlyphRunItem {
        Rect rect{};
        TextSpan text{};
    };

    struct ImageBatchItem {
        Rect rect{};
    };

    constexpr Rect rect_union(const Rect& a, const Rect& b) noexcept {
        const Rect ra = rect_normalized(a);
        const Rect rb = rect_normalized(b);
        const int left = (ra.x < rb.x) ? ra.x : rb.x;
        const int top = (ra.y < rb.y) ? ra.y : rb.y;
        const int right = ((ra.x + ra.w) > (rb.x + rb.w)) ? (ra.x + ra.w) : (rb.x + rb.w);
        const int bottom = ((ra.y + ra.h) > (rb.y + rb.h)) ? (ra.y + ra.h) : (rb.y + rb.h);
        return Rect{left, top, right - left, bottom - top};
    }

    constexpr Rect line_bounds(int x0, int y0, int x1, int y1) noexcept {
        const int left = (x0 < x1) ? x0 : x1;
        const int top = (y0 < y1) ? y0 : y1;
        const int w = (x0 < x1) ? (x1 - x0 + 1) : (x0 - x1 + 1);
        const int h = (y0 < y1) ? (y1 - y0 + 1) : (y0 - y1 + 1);
        return Rect{left, top, w, h};
    }

    constexpr bool rgba_equal(const rgba& a, const rgba& b) noexcept {
        return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
    }

    inline std::int32_t g_compaction_union_max_factor = 8;

    [[nodiscard]] inline std::int32_t clamp_union_factor(std::int32_t value) noexcept {
        return (value < 1) ? 1 : value;
    }

    inline void set_compaction_union_factor(std::int32_t factor) noexcept {
        g_compaction_union_max_factor = clamp_union_factor(factor);
    }

    [[nodiscard]] inline std::int32_t compaction_union_factor() noexcept {
        return g_compaction_union_max_factor;
    }

    constexpr std::size_t align_up(std::size_t value, std::size_t alignment) noexcept {
        if (alignment == 0) return value;
        const std::size_t mask = alignment - 1;
        return (value + mask) & ~mask;
    }

    constexpr std::size_t cmd_stride(std::size_t size) noexcept {
        return align_up(size, kCmdAlign);
    }

    inline bool payload_fits(const CmdHeader* header, std::size_t payload_size) noexcept {
        if (!header) return false;
        const std::size_t required = sizeof(CmdHeader) + payload_size;
        return header->size >= required;
    }

    template <typename Payload>
    inline Payload read_payload(const CmdHeader* header) noexcept {
        Payload payload{};
        if (!header) return payload;
        const auto* base = reinterpret_cast<const std::byte*>(header);
        std::memcpy(&payload, base + sizeof(CmdHeader), sizeof(Payload));
        return payload;
    }

    inline bool decode_cmd(const CmdHeader* header, DrawCmd& out) noexcept {
        if (!header) return false;
        out = DrawCmd{};
        out.type = header->type;
        out.rect = header->rect;
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
        out.draw_scope = DrawScope{header->draw_scope};
#endif
        switch (header->type) {
        case CmdType::PushClip:
        case CmdType::PopClip:
            return true;
        case CmdType::DrawLine: {
            if (!payload_fits(header, sizeof(CmdColor))) return false;
            const auto payload = read_payload<CmdColor>(header);
            out.color = payload.color;
            return true;
        }
        case CmdType::DrawPath: {
            if (!payload_fits(header, sizeof(CmdPath))) return false;
            const auto payload = read_payload<CmdPath>(header);
            out.color = payload.color;
            out.blob = payload.blob;
            out.p0 = payload.count;
            out.p1 = payload.closed;
            return true;
        }
        case CmdType::FillRect:
        case CmdType::StrokeRect: {
            if (!payload_fits(header, sizeof(CmdColor))) return false;
            const auto payload = read_payload<CmdColor>(header);
            out.color = payload.color;
            return true;
        }
        case CmdType::FillLinearGradientRect: {
            if (!payload_fits(header, sizeof(CmdColorP0P1P2P3))) return false;
            const auto payload = read_payload<CmdColorP0P1P2P3>(header);
            out.color = payload.color;
            out.p0 = payload.p0;
            out.p1 = payload.p1;
            out.p2 = payload.p2;
            out.p3 = payload.p3;
            return true;
        }
        case CmdType::FillRoundRect:
        case CmdType::StrokeRoundRect:
        case CmdType::FillCircle:
        case CmdType::StrokeCircle: {
            if (!payload_fits(header, sizeof(CmdColorP0))) return false;
            const auto payload = read_payload<CmdColorP0>(header);
            out.color = payload.color;
            out.p0 = payload.p0;
            return true;
        }
        case CmdType::DrawImage: {
            if (!payload_fits(header, sizeof(CmdImage))) return false;
            const auto payload = read_payload<CmdImage>(header);
            out.image = payload.image;
            return true;
        }
        case CmdType::DrawImageRoundRect: {
            if (!payload_fits(header, sizeof(CmdImageP0P1P2))) return false;
            const auto payload = read_payload<CmdImageP0P1P2>(header);
            out.image = payload.image;
            out.p0 = payload.p0;
            out.p1 = payload.p1;
            out.p2 = payload.p2;
            return true;
        }
        case CmdType::DrawImageNineSlice: {
            if (!payload_fits(header, sizeof(CmdImageP0P1P2P3))) return false;
            const auto payload = read_payload<CmdImageP0P1P2P3>(header);
            out.image = payload.image;
            out.p0 = payload.p0;
            out.p1 = payload.p1;
            out.p2 = payload.p2;
            out.p3 = payload.p3;
            return true;
        }
        case CmdType::DrawTextBox: {
            if (!payload_fits(header, sizeof(CmdTextBox))) return false;
            const auto payload = read_payload<CmdTextBox>(header);
            out.color = payload.color;
            out.text = payload.text;
            out.font_ptr = payload.font_ptr;
            out.font = payload.font;
            out.align_h = payload.align_h;
            out.align_v = payload.align_v;
            out.wrap = payload.wrap;
            out.ellipsis = payload.ellipsis;
            return true;
        }
        case CmdType::FocusRing: {
            if (!payload_fits(header, sizeof(CmdColorP0P1P2))) return false;
            const auto payload = read_payload<CmdColorP0P1P2>(header);
            out.color = payload.color;
            out.p0 = payload.p0;
            out.p1 = payload.p1;
            out.p2 = payload.p2;
            return true;
        }
        case CmdType::FillRectBatch:
        case CmdType::StrokeRectBatch: {
            if (!payload_fits(header, sizeof(CmdBatchRect))) return false;
            const auto payload = read_payload<CmdBatchRect>(header);
            out.color = payload.color;
            out.blob = payload.blob;
            out.p0 = payload.count;
            return true;
        }
        case CmdType::DrawLineBatch: {
            if (!payload_fits(header, sizeof(CmdBatchLine))) return false;
            const auto payload = read_payload<CmdBatchLine>(header);
            out.color = payload.color;
            out.blob = payload.blob;
            out.p0 = payload.count;
            return true;
        }
        case CmdType::DrawPathBatch: {
            if (!payload_fits(header, sizeof(CmdBatchPath))) return false;
            const auto payload = read_payload<CmdBatchPath>(header);
            out.color = payload.color;
            out.blob = payload.blob;
            out.p0 = payload.count;
            return true;
        }
        case CmdType::FocusRingBatch: {
            if (!payload_fits(header, sizeof(CmdBatchRectP0P1P2))) return false;
            const auto payload = read_payload<CmdBatchRectP0P1P2>(header);
            out.color = payload.color;
            out.blob = payload.blob;
            out.p0 = payload.p0;
            out.p1 = payload.p1;
            out.p2 = payload.p2;
            out.p3 = payload.count;
            return true;
        }
        case CmdType::FillRoundRectBatch:
        case CmdType::StrokeRoundRectBatch:
        case CmdType::FillCircleBatch:
        case CmdType::StrokeCircleBatch: {
            if (!payload_fits(header, sizeof(CmdBatchRectP0))) return false;
            const auto payload = read_payload<CmdBatchRectP0>(header);
            out.color = payload.color;
            out.blob = payload.blob;
            out.p0 = payload.p0;
            out.p1 = payload.count;
            return true;
        }
        case CmdType::GlyphRun: {
            if (!payload_fits(header, sizeof(CmdGlyphRun))) return false;
            const auto payload = read_payload<CmdGlyphRun>(header);
            out.color = payload.color;
            out.blob = payload.blob;
            out.font_ptr = payload.font_ptr;
            out.font = payload.font;
            out.align_h = payload.align_h;
            out.align_v = payload.align_v;
            out.wrap = payload.wrap;
            out.ellipsis = payload.ellipsis;
            out.p0 = static_cast<std::int16_t>(payload.count);
            return true;
        }
        case CmdType::DrawImageBatch: {
            if (!payload_fits(header, sizeof(CmdImageBatch))) return false;
            const auto payload = read_payload<CmdImageBatch>(header);
            out.image = payload.image;
            out.blob = payload.blob;
            out.p0 = payload.count;
            return true;
        }
        case CmdType::DrawImageRoundRectBatch: {
            if (!payload_fits(header, sizeof(CmdImageBatchP0P1P2))) return false;
            const auto payload = read_payload<CmdImageBatchP0P1P2>(header);
            out.image = payload.image;
            out.blob = payload.blob;
            out.p0 = payload.p0;
            out.p1 = payload.p1;
            out.p2 = payload.p2;
            out.p3 = payload.count;
            return true;
        }
        case CmdType::DrawImageNineSliceBatch: {
            if (!payload_fits(header, sizeof(CmdImageBatchP0P1P2P3))) return false;
            const auto payload = read_payload<CmdImageBatchP0P1P2P3>(header);
            out.image = payload.image;
            out.blob = payload.blob;
            out.p0 = payload.p0;
            out.p1 = payload.p1;
            out.p2 = payload.p2;
            out.p3 = payload.p3;
            return true;
        }
        default:
            return false;
        }
    }
}

