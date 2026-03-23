module;

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <type_traits>

export module charm.gfx.draw_cmd;

export import charm.core.geometry;
export import charm.gfx.canvas;
export import charm.gfx.color;
export import charm.gfx.framebuffer;
export import charm.gfx.image;
export import charm.gfx.path;
export import charm.gfx.render_core;
export import charm.font;
export import charm.font.typography;
export import charm.gfx.text_box;
export import ui.render_backend;

import util.core;

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
    using ui::gfx::invalid_image_id;
    using ui::gfx::image_id_valid;
    using ui::gfx::image_register_reason_name;
    using ui::gfx::register_image;
    using ui::gfx::register_image_key;
    using ui::gfx::register_image_dedup;
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
        FontId font{FontId::Normal};
        TextAlignH align_h{TextAlignH::Left};
        TextAlignV align_v{TextAlignV::Top};
        TextWrap wrap{TextWrap::None};
        TextEllipsis ellipsis{TextEllipsis::None};
    };

    constexpr std::size_t kCmdAlign = alignof(std::uint32_t);
    static_assert(sizeof(CmdHeader) % kCmdAlign == 0);

    constexpr std::uint32_t kDrawCmdBinaryVersion = 2;

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

    struct DrawCmdTileConfig {
        int tile_width{64};
        int tile_height{64};
        rgba clear_color{0, 0, 0, 0};
        bool clear_tile{true};
    };

    struct DrawCmdTileStats {
        int tiles_total{0};
        int tiles_drawn{0};
        std::size_t cmd_count{0};
        std::size_t cmd_bytes{0};
        int tile_flush_count{0};
        std::size_t clip_push_overflow{0};
        std::size_t clip_pop_underflow{0};
        std::size_t clip_invalid{0};
        std::size_t dispatch_groups{0};
        std::size_t batch_flushes{0};
        std::size_t failed_cmds{0};
        std::size_t fail_text{0};
        std::size_t fail_image{0};
        std::size_t fail_blob{0};
        std::size_t fail_path{0};
        std::size_t fail_clip{0};
        std::size_t fail_other{0};
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
    };

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
            if (!payload_fits(header, sizeof(CmdImageP0))) return false;
            const auto payload = read_payload<CmdImageP0>(header);
            out.image = payload.image;
            out.p0 = payload.p0;
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
            if (!payload_fits(header, sizeof(CmdImageBatchP0))) return false;
            const auto payload = read_payload<CmdImageBatchP0>(header);
            out.image = payload.image;
            out.blob = payload.blob;
            out.p0 = payload.p0;
            out.p1 = payload.count;
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

    template <std::size_t Capacity>
    class BlobArena {
    public:
        void reset() noexcept {
            used_ = 0;
            overflowed_ = false;
        }

        [[nodiscard]] std::size_t used() const noexcept { return used_; }
        [[nodiscard]] std::size_t capacity() const noexcept { return Capacity; }
        [[nodiscard]] bool overflowed() const noexcept { return overflowed_; }
        [[nodiscard]] const std::byte* data() const noexcept { return buffer_.data(); }

        BlobRef add_bytes(const void* data, std::size_t len, std::size_t alignment) noexcept {
            if (!data || len == 0) return BlobRef{};
            const std::size_t offset = align_up(used_, alignment);
            const std::size_t next = offset + len;
            if (next > Capacity) {
                overflowed_ = true;
                return BlobRef{};
            }
            std::memcpy(buffer_.data() + offset, data, len);
            used_ = next;
            return BlobRef{static_cast<std::uint32_t>(offset), static_cast<std::uint32_t>(len)};
        }

        std::span<const std::byte> bytes(BlobRef ref) const noexcept {
            if (ref.length == 0) return {};
            const std::size_t end = static_cast<std::size_t>(ref.offset) + ref.length;
            if (end > used_) return {};
            return std::span<const std::byte>(buffer_.data() + ref.offset, ref.length);
        }

        bool load(const std::byte* data, std::size_t len) noexcept {
            reset();
            if (!data || len == 0) return true;
            if (len > Capacity) {
                overflowed_ = true;
                return false;
            }
            std::memcpy(buffer_.data(), data, len);
            used_ = len;
            return true;
        }

    private:
        std::array<std::byte, Capacity> buffer_{};
        std::size_t used_{0};
        bool overflowed_{false};
    };

    template <std::size_t MaxCmds, std::size_t TextBytes, std::size_t BlobBytes>
    class DrawCmdBuffer {
    public:
        static constexpr std::size_t kMaxCommands = MaxCmds;
        static constexpr std::size_t kTextCapacity = TextBytes;
        static constexpr std::size_t kBlobCapacity = BlobBytes;
        static constexpr std::size_t kCmdBytesCapacity = kMaxCommands * sizeof(DrawCmd);

        void clear() noexcept {
            count_ = 0;
            cmd_bytes_used_ = 0;
            cmd_overflowed_ = false;
            text_overflowed_ = false;
            text_used_ = 1;
            text_[0] = '\0';
            blob_.reset();
            batch_shrink_ = 0;
            batch_shrink_line_ = 0;
            batch_shrink_path_ = 0;
            batch_shrink_rect_ = 0;
            batch_shrink_round_ = 0;
            batch_shrink_image_ = 0;
            batch_shrink_focus_ = 0;
            offsets_valid_ = true;
        }

        [[nodiscard]] std::size_t size() const noexcept { return count_; }
        [[nodiscard]] const std::byte* cmd_data() const noexcept { return cmd_bytes_.data(); }
        [[nodiscard]] std::size_t cmd_bytes() const noexcept { return cmd_bytes_used_; }
        [[nodiscard]] const char* text_data() const noexcept { return text_.data(); }
        [[nodiscard]] std::size_t text_used() const noexcept { return text_used_; }
        [[nodiscard]] const std::byte* blob_data() const noexcept { return blob_.data(); }
        [[nodiscard]] std::size_t blob_used() const noexcept { return blob_.used(); }
        [[nodiscard]] bool cmd_overflowed() const noexcept { return cmd_overflowed_; }
        [[nodiscard]] bool text_overflowed() const noexcept { return text_overflowed_; }
        [[nodiscard]] bool blob_overflowed() const noexcept { return blob_.overflowed(); }
        [[nodiscard]] bool overflowed() const noexcept {
            return cmd_overflowed_ || text_overflowed_ || blob_.overflowed();
        }
        [[nodiscard]] const char* text_at(std::uint32_t offset) const noexcept {
            if (offset >= text_used_) return text_.data();
            return text_.data() + offset;
        }
        [[nodiscard]] std::span<const std::byte> blob_at(BlobRef ref) const noexcept {
            return blob_.bytes(ref);
        }
        [[nodiscard]] bool text_span_valid(TextSpan span) const noexcept {
            if (span.length == 0) return true;
            const std::size_t end = static_cast<std::size_t>(span.offset) + span.length;
            return end <= text_used_;
        }
        [[nodiscard]] const CmdHeader* cmd_header(std::size_t index) const noexcept {
            if (!offsets_valid_) return nullptr;
            if (index >= count_) return nullptr;
            const std::size_t offset = cmd_offsets_[index];
            if (offset + sizeof(CmdHeader) > cmd_bytes_used_) return nullptr;
            const auto* header = reinterpret_cast<const CmdHeader*>(cmd_bytes_.data() + offset);
            if (header->size < sizeof(CmdHeader)) return nullptr;
            const std::size_t stride = cmd_stride(header->size);
            if (offset + stride > cmd_bytes_used_) return nullptr;
            return header;
        }
        [[nodiscard]] bool read_cmd(std::size_t index, DrawCmd& out) const noexcept {
            const CmdHeader* header = cmd_header(index);
            if (!header) return false;
            return decode_cmd(header, out);
        }
        [[nodiscard]] bool read_cmd_at_offset(std::size_t offset,
                                              DrawCmd& out,
                                              std::size_t& stride) const noexcept {
            if (offset + sizeof(CmdHeader) > cmd_bytes_used_) return false;
            const auto* header = reinterpret_cast<const CmdHeader*>(cmd_bytes_.data() + offset);
            if (header->size < sizeof(CmdHeader)) return false;
            stride = cmd_stride(header->size);
            if (stride == 0 || offset + stride > cmd_bytes_used_) return false;
            return decode_cmd(header, out);
        }

        bool load(const std::byte* cmd_bytes,
                  std::size_t cmd_bytes_len,
                  std::size_t cmd_count,
                  const char* text,
                  std::size_t text_bytes,
                  const std::byte* blob,
                  std::size_t blob_bytes) noexcept {
            clear();
            if (!cmd_bytes && cmd_bytes_len != 0) {
                cmd_overflowed_ = true;
                return false;
            }
            if (cmd_bytes_len > kCmdBytesCapacity) {
                cmd_overflowed_ = true;
                cmd_bytes_len = kCmdBytesCapacity;
            }
            if (cmd_count > kMaxCommands) {
                cmd_overflowed_ = true;
                cmd_count = kMaxCommands;
            }
            if (cmd_bytes_len > 0) {
                std::memcpy(cmd_bytes_.data(), cmd_bytes, cmd_bytes_len);
            }
            cmd_bytes_used_ = cmd_bytes_len;
            count_ = cmd_count;
            if (cmd_bytes_used_ > 0 && count_ != 0) {
                if (!rebuild_offsets()) {
                    cmd_overflowed_ = true;
                    return false;
                }
            }
            offsets_valid_ = (count_ != 0);
            if (!text || text_bytes == 0) {
                text_[0] = '\0';
                text_used_ = 1;
            } else {
                if (text_bytes > kTextCapacity) {
                    text_overflowed_ = true;
                    text_bytes = kTextCapacity;
                }
                std::memcpy(text_.data(), text, text_bytes);
                text_used_ = text_bytes;
            }
            if (blob && blob_bytes > 0) {
                if (!blob_.load(blob, blob_bytes)) {
                    return false;
                }
            }
            return !overflowed();
        }

        DrawCmdStats stats() const noexcept {
            return DrawCmdStats{
                count_,
                kMaxCommands,
                cmd_bytes_used_,
                text_used_,
                kTextCapacity,
                blob_.used(),
                kBlobCapacity,
                batch_shrink_,
                batch_shrink_line_,
                batch_shrink_path_,
                batch_shrink_rect_,
                batch_shrink_round_,
                batch_shrink_image_,
                batch_shrink_focus_,
                cmd_overflowed_,
                text_overflowed_,
                blob_.overflowed()
            };
        }

        bool push_clip(const Rect& rect) noexcept { return push_cmd(make_cmd(CmdType::PushClip, rect)); }
        bool pop_clip() noexcept { return push_cmd(make_cmd(CmdType::PopClip, Rect{})); }

        bool draw_line(int x0, int y0, int x1, int y1, const rgba& color) noexcept {
            auto cmd = make_cmd(CmdType::DrawLine, Rect{x0, y0, x1, y1});
            cmd.color = color;
            return push_cmd(cmd);
        }

        bool draw_path(const Point* points, int count, bool closed, const rgba& color) noexcept {
            Rect bounds{};
            if (!ui::gfx::path::compute_bounds(points, count, bounds)) return false;
            auto cmd = make_cmd(CmdType::DrawPath, bounds);
            cmd.color = color;
            const BlobRef blob = blob_.add_bytes(points,
                                                 ui::gfx::path::point_bytes(count),
                                                 alignof(Point));
            if (blob.length == 0) return false;
            cmd.blob = blob;
            cmd.p0 = static_cast<std::int16_t>(count);
            cmd.p1 = closed ? 1 : 0;
            return push_cmd(cmd);
        }

        bool fill_rect(const Rect& rect, const rgba& color) noexcept {
            auto cmd = make_cmd(CmdType::FillRect, rect);
            cmd.color = color;
            return push_cmd(cmd);
        }

        bool stroke_rect(const Rect& rect, const rgba& color) noexcept {
            auto cmd = make_cmd(CmdType::StrokeRect, rect);
            cmd.color = color;
            return push_cmd(cmd);
        }

        bool fill_round_rect(const Rect& rect, int radius, const rgba& color) noexcept {
            auto cmd = make_cmd(CmdType::FillRoundRect, rect);
            cmd.color = color;
            cmd.p0 = static_cast<std::int16_t>(radius);
            return push_cmd(cmd);
        }

        bool stroke_round_rect(const Rect& rect, int radius, const rgba& color) noexcept {
            auto cmd = make_cmd(CmdType::StrokeRoundRect, rect);
            cmd.color = color;
            cmd.p0 = static_cast<std::int16_t>(radius);
            return push_cmd(cmd);
        }

        bool fill_circle(int cx, int cy, int radius, const rgba& color) noexcept {
            Rect rect{cx - radius, cy - radius, radius * 2, radius * 2};
            auto cmd = make_cmd(CmdType::FillCircle, rect);
            cmd.color = color;
            cmd.p0 = static_cast<std::int16_t>(radius);
            return push_cmd(cmd);
        }

        bool stroke_circle(int cx, int cy, int radius, const rgba& color) noexcept {
            Rect rect{cx - radius, cy - radius, radius * 2, radius * 2};
            auto cmd = make_cmd(CmdType::StrokeCircle, rect);
            cmd.color = color;
            cmd.p0 = static_cast<std::int16_t>(radius);
            return push_cmd(cmd);
        }

        bool draw_image(const Rect& rect, ImageId image) noexcept {
            if (!image_id_valid(image)) return false;
            auto cmd = make_cmd(CmdType::DrawImage, rect);
            cmd.image = image;
            return push_cmd(cmd);
        }
        bool draw_image_round_rect(const Rect& rect, ImageId image, int corner_radius) noexcept {
            if (!image_id_valid(image)) return false;
            auto cmd = make_cmd(CmdType::DrawImageRoundRect, rect);
            cmd.image = image;
            cmd.p0 = static_cast<std::int16_t>(corner_radius);
            return push_cmd(cmd);
        }

        bool draw_icon(const Rect& rect, ImageId image) noexcept {
            return draw_image(rect, image);
        }

        bool draw_image_nine_slice(const Rect& rect,
                                   ImageId image,
                                   int left,
                                   int top,
                                   int right,
                                   int bottom) noexcept {
            if (!image_id_valid(image)) return false;
            auto cmd = make_cmd(CmdType::DrawImageNineSlice, rect);
            cmd.image = image;
            cmd.p0 = static_cast<std::int16_t>(left);
            cmd.p1 = static_cast<std::int16_t>(top);
            cmd.p2 = static_cast<std::int16_t>(right);
            cmd.p3 = static_cast<std::int16_t>(bottom);
            return push_cmd(cmd);
        }

        bool draw_text_box(const Rect& rect,
                           const char* text,
                           const rgba& color,
                           const Font& font,
                           TextAlignH align_h,
                           TextAlignV align_v,
                           TextWrap wrap,
                           TextEllipsis ellipsis) noexcept {
            const TextSpan span = add_text(text);
            auto cmd = make_cmd(CmdType::DrawTextBox, rect);
            cmd.color = color;
            cmd.text = span;
            cmd.font = font_id_from_ptr(&font);
            cmd.align_h = align_h;
            cmd.align_v = align_v;
            cmd.wrap = wrap;
            cmd.ellipsis = ellipsis;
            return push_cmd(cmd);
        }

        bool focus_ring(const Rect& rect,
                        const rgba& color,
                        int corner_radius,
                        int inset,
                        int radius_override) noexcept {
            auto cmd = make_cmd(CmdType::FocusRing, rect);
            cmd.color = color;
            cmd.p0 = static_cast<std::int16_t>(corner_radius);
            cmd.p1 = static_cast<std::int16_t>(inset);
            cmd.p2 = static_cast<std::int16_t>(radius_override);
            return push_cmd(cmd);
        }

        bool any_draw_hits(const Rect& rect) const noexcept {
            Rect out{};
            DrawCmd cmd{};
            std::size_t offset = 0;
            while (offset < cmd_bytes_used_) {
                std::size_t stride = 0;
                if (!read_cmd_at_offset(offset, cmd, stride)) break;
                if (cmd.type == CmdType::PushClip || cmd.type == CmdType::PopClip) {
                    offset += stride;
                    continue;
                }
                if (cmd.type == CmdType::DrawLine) {
                    const Rect bounds = line_bounds(cmd.rect.x, cmd.rect.y, cmd.rect.w, cmd.rect.h);
                    if (rect_intersect(bounds, rect, out)) return true;
                    offset += stride;
                    continue;
                }
                if (cmd.type == CmdType::DrawLineBatch) {
                    const int count = cmd.p0;
                    if (count <= 0) {
                        offset += stride;
                        continue;
                    }
                    const auto blob = blob_.bytes(cmd.blob);
                    if (blob.size() < static_cast<std::size_t>(count) * sizeof(LineBatchItem)) {
                        offset += stride;
                        continue;
                    }
                    const auto items = std::span<const LineBatchItem>(
                        reinterpret_cast<const LineBatchItem*>(blob.data()), count);
                    for (const auto& item : items) {
                        const Rect bounds = line_bounds(item.x0, item.y0, item.x1, item.y1);
                        if (rect_intersect(bounds, rect, out)) return true;
                    }
                    offset += stride;
                    continue;
                }
                if (!rect_valid(cmd.rect)) {
                    offset += stride;
                    continue;
                }
                if (rect_intersect(cmd.rect, rect, out)) return true;
                offset += stride;
            }
            return false;
        }

        bool compact() noexcept {
            if (cmd_bytes_used_ == 0) return true;
            const std::size_t input_bytes = cmd_bytes_used_;
            std::array<std::uint32_t, kMaxCommands> output_offsets{};
            std::size_t out_bytes = 0;
            std::size_t out_count = 0;
            std::size_t offset = 0;
            bool ok = true;
            constexpr std::size_t kMaxBatchItems = 64;
            std::array<RectBatchItem, kMaxBatchItems> rect_items{};
            std::array<LineBatchItem, kMaxBatchItems> line_items{};
            std::array<PathBatchItem, kMaxBatchItems> path_items{};
            std::array<GlyphRunItem, kMaxBatchItems> text_items{};
            std::array<ImageBatchItem, kMaxBatchItems> image_items{};
            const bool allow_batch = !blob_.overflowed();
            batch_shrink_ = 0;
            batch_shrink_line_ = 0;
            batch_shrink_path_ = 0;
            batch_shrink_rect_ = 0;
            batch_shrink_round_ = 0;
            batch_shrink_image_ = 0;
            batch_shrink_focus_ = 0;
            const std::int64_t union_max_factor = static_cast<std::int64_t>(compaction_union_factor());

            auto read_cmd_at_offset = [&](std::size_t at, DrawCmd& out_cmd, std::size_t& stride) noexcept -> bool {
                if (at + sizeof(CmdHeader) > input_bytes) return false;
                const auto* header = reinterpret_cast<const CmdHeader*>(cmd_bytes_.data() + at);
                if (header->size < sizeof(CmdHeader)) return false;
                stride = cmd_stride(header->size);
                if (stride == 0 || at + stride > input_bytes) return false;
                return decode_cmd(header, out_cmd);
            };

            auto emit_cmd = [&](const DrawCmd& out_cmd) noexcept -> bool {
                if (!encode_cmd(out_cmd,
                                cmd_bytes_.data(),
                                kCmdBytesCapacity,
                                out_bytes,
                                out_count,
                                output_offsets.data())) {
                    cmd_overflowed_ = true;
                    return false;
                }
                return true;
            };

            auto can_merge_text = [](const DrawCmd& a, const DrawCmd& b) noexcept {
                return rgba_equal(a.color, b.color)
                    && a.font == b.font
                    && a.align_h == b.align_h
                    && a.align_v == b.align_v
                    && a.wrap == b.wrap
                    && a.ellipsis == b.ellipsis;
            };
            auto rect_area = [](const Rect& r) noexcept -> std::int64_t {
                const Rect n = rect_normalized(r);
                if (n.w <= 0 || n.h <= 0) return 0;
                return static_cast<std::int64_t>(n.w) * static_cast<std::int64_t>(n.h);
            };

            while (offset < input_bytes) {
                DrawCmd cmd{};
                std::size_t stride = 0;
                if (!read_cmd_at_offset(offset, cmd, stride)) {
                    ok = false;
                    break;
                }
                if (allow_batch && cmd.type == CmdType::DrawLine) {
                    std::size_t run = 1;
                    std::size_t scan_offset = offset + stride;
                    while (scan_offset < input_bytes) {
                        DrawCmd next{};
                        std::size_t next_stride = 0;
                        if (!read_cmd_at_offset(scan_offset, next, next_stride)) break;
                        if (next.type != CmdType::DrawLine) break;
                        if (!rgba_equal(next.color, cmd.color)) break;
                        scan_offset += next_stride;
                        ++run;
                    }
                    if (run >= 2) {
                        std::size_t batch = (run > kMaxBatchItems) ? kMaxBatchItems : run;
                        while (batch >= 2) {
                            Rect bounds{};
                            std::int64_t sum_area = 0;
                            std::size_t item_offset = offset;
                            for (std::size_t j = 0; j < batch; ++j) {
                                DrawCmd next{};
                                std::size_t item_stride = 0;
                                (void)read_cmd_at_offset(item_offset, next, item_stride);
                                line_items[j] = LineBatchItem{next.rect.x, next.rect.y, next.rect.w, next.rect.h};
                                const Rect line_rect = line_bounds(line_items[j].x0, line_items[j].y0,
                                                                   line_items[j].x1, line_items[j].y1);
                                if (j == 0) {
                                    bounds = line_rect;
                                } else {
                                    bounds = rect_union(bounds, line_rect);
                                }
                                sum_area += rect_area(line_rect);
                                item_offset += item_stride;
                            }
                            const std::int64_t union_area = rect_area(bounds);
                            const bool area_ok = (sum_area == 0)
                                ? true
                                : (union_area <= (sum_area * union_max_factor));
                            if (!area_ok) {
                                ++batch_shrink_;
                                ++batch_shrink_line_;
                                --batch;
                                continue;
                            }
                            const BlobRef blob = blob_.add_bytes(line_items.data(),
                                                                 batch * sizeof(LineBatchItem),
                                                                 alignof(LineBatchItem));
                            if (blob.length != 0) {
                                DrawCmd batch_cmd{};
                                batch_cmd.type = CmdType::DrawLineBatch;
                                batch_cmd.rect = bounds;
                                batch_cmd.color = cmd.color;
                                batch_cmd.blob = blob;
                                batch_cmd.p0 = static_cast<std::int16_t>(batch);
                                if (!emit_cmd(batch_cmd)) ok = false;
                                offset = item_offset;
                                break;
                            }
                            ok = false;
                            break;
                        }
                        if (batch >= 2) {
                            continue;
                        }
                    }
                } else if (allow_batch && cmd.type == CmdType::DrawPath) {
                    std::size_t run = 1;
                    std::size_t scan_offset = offset + stride;
                    while (scan_offset < input_bytes) {
                        DrawCmd next{};
                        std::size_t next_stride = 0;
                        if (!read_cmd_at_offset(scan_offset, next, next_stride)) break;
                        if (next.type != CmdType::DrawPath) break;
                        if (!rgba_equal(next.color, cmd.color)) break;
                        scan_offset += next_stride;
                        ++run;
                    }
                    if (run >= 2) {
                        std::size_t batch = (run > kMaxBatchItems) ? kMaxBatchItems : run;
                        while (batch >= 2) {
                            Rect bounds = cmd.rect;
                            std::int64_t sum_area = 0;
                            std::size_t item_offset = offset;
                            for (std::size_t j = 0; j < batch; ++j) {
                                DrawCmd next{};
                                std::size_t item_stride = 0;
                                (void)read_cmd_at_offset(item_offset, next, item_stride);
                                path_items[j].blob = next.blob;
                                path_items[j].count = next.p0;
                                path_items[j].closed = next.p1;
                                if (j == 0) {
                                    bounds = next.rect;
                                } else {
                                    bounds = rect_union(bounds, next.rect);
                                }
                                sum_area += rect_area(next.rect);
                                item_offset += item_stride;
                            }
                            const std::int64_t union_area = rect_area(bounds);
                            const bool area_ok = (sum_area == 0)
                                ? true
                                : (union_area <= (sum_area * union_max_factor));
                            if (!area_ok) {
                                ++batch_shrink_;
                                ++batch_shrink_path_;
                                --batch;
                                continue;
                            }
                            const BlobRef blob = blob_.add_bytes(path_items.data(),
                                                                 batch * sizeof(PathBatchItem),
                                                                 alignof(PathBatchItem));
                            if (blob.length != 0) {
                                DrawCmd batch_cmd{};
                                batch_cmd.type = CmdType::DrawPathBatch;
                                batch_cmd.rect = bounds;
                                batch_cmd.color = cmd.color;
                                batch_cmd.blob = blob;
                                batch_cmd.p0 = static_cast<std::int16_t>(batch);
                                if (!emit_cmd(batch_cmd)) ok = false;
                                offset = item_offset;
                                break;
                            }
                            ok = false;
                            break;
                        }
                        if (batch >= 2) {
                            continue;
                        }
                    }
                } else if (allow_batch && (cmd.type == CmdType::FillRect || cmd.type == CmdType::StrokeRect)) {
                    std::size_t run = 1;
                    std::size_t scan_offset = offset + stride;
                    while (scan_offset < input_bytes) {
                        DrawCmd next{};
                        std::size_t next_stride = 0;
                        if (!read_cmd_at_offset(scan_offset, next, next_stride)) break;
                        if (next.type != cmd.type || !rgba_equal(next.color, cmd.color)) break;
                        scan_offset += next_stride;
                        ++run;
                    }
                    if (run >= 2) {
                        std::size_t batch = (run > kMaxBatchItems) ? kMaxBatchItems : run;
                        while (batch >= 2) {
                            Rect bounds{};
                            std::int64_t sum_area = 0;
                            std::size_t item_offset = offset;
                            for (std::size_t j = 0; j < batch; ++j) {
                                DrawCmd next{};
                                std::size_t item_stride = 0;
                                (void)read_cmd_at_offset(item_offset, next, item_stride);
                                rect_items[j].rect = next.rect;
                                if (j == 0) {
                                    bounds = rect_items[j].rect;
                                } else {
                                    bounds = rect_union(bounds, rect_items[j].rect);
                                }
                                sum_area += rect_area(rect_items[j].rect);
                                item_offset += item_stride;
                            }
                            const std::int64_t union_area = rect_area(bounds);
                            const bool area_ok = (sum_area == 0)
                                ? true
                                : (union_area <= (sum_area * union_max_factor));
                            if (!area_ok) {
                                ++batch_shrink_;
                                ++batch_shrink_rect_;
                                --batch;
                                continue;
                            }
                            const BlobRef blob = blob_.add_bytes(rect_items.data(),
                                                                 batch * sizeof(RectBatchItem),
                                                                 alignof(RectBatchItem));
                            if (blob.length != 0) {
                                DrawCmd batch_cmd{};
                                batch_cmd.type = (cmd.type == CmdType::StrokeRect)
                                    ? CmdType::StrokeRectBatch
                                    : CmdType::FillRectBatch;
                                batch_cmd.rect = bounds;
                                batch_cmd.color = cmd.color;
                                batch_cmd.blob = blob;
                                batch_cmd.p0 = static_cast<std::int16_t>(batch);
                                if (!emit_cmd(batch_cmd)) ok = false;
                                offset = item_offset;
                                break;
                            }
                            ok = false;
                            break;
                        }
                        if (batch >= 2) {
                            continue;
                        }
                    }
                } else if (allow_batch
                    && (cmd.type == CmdType::FillRoundRect
                        || cmd.type == CmdType::StrokeRoundRect
                        || cmd.type == CmdType::FillCircle
                        || cmd.type == CmdType::StrokeCircle)) {
                    std::size_t run = 1;
                    std::size_t scan_offset = offset + stride;
                    while (scan_offset < input_bytes) {
                        DrawCmd next{};
                        std::size_t next_stride = 0;
                        if (!read_cmd_at_offset(scan_offset, next, next_stride)) break;
                        if (next.type != cmd.type) break;
                        if (!rgba_equal(next.color, cmd.color)) break;
                        if (next.p0 != cmd.p0) break;
                        scan_offset += next_stride;
                        ++run;
                    }
                    if (run >= 2) {
                        std::size_t batch = (run > kMaxBatchItems) ? kMaxBatchItems : run;
                        while (batch >= 2) {
                            Rect bounds{};
                            std::int64_t sum_area = 0;
                            std::size_t item_offset = offset;
                            for (std::size_t j = 0; j < batch; ++j) {
                                DrawCmd next{};
                                std::size_t item_stride = 0;
                                (void)read_cmd_at_offset(item_offset, next, item_stride);
                                rect_items[j].rect = next.rect;
                                if (j == 0) {
                                    bounds = rect_items[j].rect;
                                } else {
                                    bounds = rect_union(bounds, rect_items[j].rect);
                                }
                                sum_area += rect_area(rect_items[j].rect);
                                item_offset += item_stride;
                            }
                            const std::int64_t union_area = rect_area(bounds);
                            const bool area_ok = (sum_area == 0)
                                ? true
                                : (union_area <= (sum_area * union_max_factor));
                            if (!area_ok) {
                                ++batch_shrink_;
                                ++batch_shrink_round_;
                                --batch;
                                continue;
                            }
                            const BlobRef blob = blob_.add_bytes(rect_items.data(),
                                                                 batch * sizeof(RectBatchItem),
                                                                 alignof(RectBatchItem));
                            if (blob.length != 0) {
                                DrawCmd batch_cmd{};
                                switch (cmd.type) {
                                case CmdType::FillRoundRect:
                                    batch_cmd.type = CmdType::FillRoundRectBatch;
                                    break;
                                case CmdType::StrokeRoundRect:
                                    batch_cmd.type = CmdType::StrokeRoundRectBatch;
                                    break;
                                case CmdType::FillCircle:
                                    batch_cmd.type = CmdType::FillCircleBatch;
                                    break;
                                case CmdType::StrokeCircle:
                                    batch_cmd.type = CmdType::StrokeCircleBatch;
                                    break;
                                default:
                                    batch_cmd.type = CmdType::FillRoundRectBatch;
                                    break;
                                }
                                batch_cmd.rect = bounds;
                                batch_cmd.color = cmd.color;
                                batch_cmd.blob = blob;
                                batch_cmd.p0 = cmd.p0;
                                batch_cmd.p1 = static_cast<std::int16_t>(batch);
                                if (!emit_cmd(batch_cmd)) ok = false;
                                offset = item_offset;
                                break;
                            }
                            ok = false;
                            break;
                        }
                        if (batch >= 2) {
                            continue;
                        }
                    }
                } else if (allow_batch && cmd.type == CmdType::DrawTextBox) {
                    if (!text_span_valid(cmd.text)) {
                        if (!emit_cmd(cmd)) ok = false;
                        offset += stride;
                        continue;
                    }
                    std::size_t run = 1;
                    std::size_t scan_offset = offset + stride;
                    while (scan_offset < input_bytes) {
                        DrawCmd next{};
                        std::size_t next_stride = 0;
                        if (!read_cmd_at_offset(scan_offset, next, next_stride)) break;
                        if (next.type != CmdType::DrawTextBox) break;
                        if (!can_merge_text(cmd, next)) break;
                        if (!text_span_valid(next.text)) break;
                        scan_offset += next_stride;
                        ++run;
                    }
                    if (run >= 2) {
                        const std::size_t batch = (run > kMaxBatchItems) ? kMaxBatchItems : run;
                        Rect bounds = cmd.rect;
                        std::size_t item_offset = offset;
                        for (std::size_t j = 0; j < batch; ++j) {
                            DrawCmd next{};
                            std::size_t item_stride = 0;
                            (void)read_cmd_at_offset(item_offset, next, item_stride);
                            text_items[j].rect = next.rect;
                            text_items[j].text = next.text;
                            if (j == 0) {
                                bounds = text_items[j].rect;
                            } else {
                                bounds = rect_union(bounds, text_items[j].rect);
                            }
                            item_offset += item_stride;
                        }
                        const BlobRef blob = blob_.add_bytes(text_items.data(),
                                                             batch * sizeof(GlyphRunItem),
                                                             alignof(GlyphRunItem));
                        if (blob.length != 0) {
                            DrawCmd batch_cmd{};
                            batch_cmd.type = CmdType::GlyphRun;
                            batch_cmd.rect = bounds;
                            batch_cmd.color = cmd.color;
                            batch_cmd.font = cmd.font;
                            batch_cmd.align_h = cmd.align_h;
                            batch_cmd.align_v = cmd.align_v;
                            batch_cmd.wrap = cmd.wrap;
                            batch_cmd.ellipsis = cmd.ellipsis;
                            batch_cmd.blob = blob;
                            batch_cmd.p0 = static_cast<std::int16_t>(batch);
                            if (!emit_cmd(batch_cmd)) ok = false;
                            offset = item_offset;
                            continue;
                        }
                        ok = false;
                    }
                } else if (allow_batch && cmd.type == CmdType::DrawImage) {
                    if (!image_id_valid(cmd.image)) {
                        if (!emit_cmd(cmd)) ok = false;
                        offset += stride;
                        continue;
                    }
                    std::size_t run = 1;
                    std::size_t scan_offset = offset + stride;
                    while (scan_offset < input_bytes) {
                        DrawCmd next{};
                        std::size_t next_stride = 0;
                        if (!read_cmd_at_offset(scan_offset, next, next_stride)) break;
                        if (next.type != CmdType::DrawImage) break;
                        if (next.image != cmd.image) break;
                        scan_offset += next_stride;
                        ++run;
                    }
                    if (run >= 2) {
                        std::size_t batch = (run > kMaxBatchItems) ? kMaxBatchItems : run;
                        while (batch >= 2) {
                            Rect bounds{};
                            std::int64_t sum_area = 0;
                            std::size_t item_offset = offset;
                            for (std::size_t j = 0; j < batch; ++j) {
                                DrawCmd next{};
                                std::size_t item_stride = 0;
                                (void)read_cmd_at_offset(item_offset, next, item_stride);
                                image_items[j].rect = next.rect;
                                if (j == 0) {
                                    bounds = image_items[j].rect;
                                } else {
                                    bounds = rect_union(bounds, image_items[j].rect);
                                }
                                sum_area += rect_area(image_items[j].rect);
                                item_offset += item_stride;
                            }
                            const std::int64_t union_area = rect_area(bounds);
                            const bool area_ok = (sum_area == 0)
                                ? true
                                : (union_area <= (sum_area * union_max_factor));
                            if (!area_ok) {
                                ++batch_shrink_;
                                ++batch_shrink_image_;
                                --batch;
                                continue;
                            }
                            const BlobRef blob = blob_.add_bytes(image_items.data(),
                                                                 batch * sizeof(ImageBatchItem),
                                                                 alignof(ImageBatchItem));
                            if (blob.length != 0) {
                                DrawCmd batch_cmd{};
                                batch_cmd.type = CmdType::DrawImageBatch;
                                batch_cmd.rect = bounds;
                                batch_cmd.image = cmd.image;
                                batch_cmd.blob = blob;
                                batch_cmd.p0 = static_cast<std::int16_t>(batch);
                                if (!emit_cmd(batch_cmd)) ok = false;
                                offset = item_offset;
                                break;
                            }
                            ok = false;
                            break;
                        }
                        if (batch >= 2) {
                            continue;
                        }
                    }
                } else if (allow_batch && cmd.type == CmdType::DrawImageRoundRect) {
                    if (!image_id_valid(cmd.image)) {
                        if (!emit_cmd(cmd)) ok = false;
                        offset += stride;
                        continue;
                    }
                    std::size_t run = 1;
                    std::size_t scan_offset = offset + stride;
                    while (scan_offset < input_bytes) {
                        DrawCmd next{};
                        std::size_t next_stride = 0;
                        if (!read_cmd_at_offset(scan_offset, next, next_stride)) break;
                        if (next.type != CmdType::DrawImageRoundRect) break;
                        if (next.image != cmd.image) break;
                        if (next.p0 != cmd.p0) break;
                        scan_offset += next_stride;
                        ++run;
                    }
                    if (run >= 2) {
                        std::size_t batch = (run > kMaxBatchItems) ? kMaxBatchItems : run;
                        while (batch >= 2) {
                            Rect bounds{};
                            std::int64_t sum_area = 0;
                            std::size_t item_offset = offset;
                            for (std::size_t j = 0; j < batch; ++j) {
                                DrawCmd next{};
                                std::size_t item_stride = 0;
                                (void)read_cmd_at_offset(item_offset, next, item_stride);
                                image_items[j].rect = next.rect;
                                if (j == 0) {
                                    bounds = image_items[j].rect;
                                } else {
                                    bounds = rect_union(bounds, image_items[j].rect);
                                }
                                sum_area += rect_area(image_items[j].rect);
                                item_offset += item_stride;
                            }
                            const std::int64_t union_area = rect_area(bounds);
                            const bool area_ok = (sum_area == 0)
                                ? true
                                : (union_area <= (sum_area * union_max_factor));
                            if (!area_ok) {
                                ++batch_shrink_;
                                ++batch_shrink_image_;
                                --batch;
                                continue;
                            }
                            const BlobRef blob = blob_.add_bytes(image_items.data(),
                                                                 batch * sizeof(ImageBatchItem),
                                                                 alignof(ImageBatchItem));
                            if (blob.length != 0) {
                                DrawCmd batch_cmd{};
                                batch_cmd.type = CmdType::DrawImageRoundRectBatch;
                                batch_cmd.rect = bounds;
                                batch_cmd.image = cmd.image;
                                batch_cmd.blob = blob;
                                batch_cmd.p0 = cmd.p0;
                                batch_cmd.p1 = static_cast<std::int16_t>(batch);
                                if (!emit_cmd(batch_cmd)) ok = false;
                                offset = item_offset;
                                break;
                            }
                            ok = false;
                            break;
                        }
                        if (batch >= 2) {
                            continue;
                        }
                    }
                } else if (allow_batch && cmd.type == CmdType::DrawImageNineSlice) {
                    if (!image_id_valid(cmd.image)) {
                        if (!emit_cmd(cmd)) ok = false;
                        offset += stride;
                        continue;
                    }
                    std::size_t run = 1;
                    std::size_t scan_offset = offset + stride;
                    while (scan_offset < input_bytes) {
                        DrawCmd next{};
                        std::size_t next_stride = 0;
                        if (!read_cmd_at_offset(scan_offset, next, next_stride)) break;
                        if (next.type != CmdType::DrawImageNineSlice) break;
                        if (next.image != cmd.image) break;
                        if (next.p0 != cmd.p0 || next.p1 != cmd.p1
                            || next.p2 != cmd.p2 || next.p3 != cmd.p3) break;
                        scan_offset += next_stride;
                        ++run;
                    }
                    if (run >= 2) {
                        std::size_t batch = (run > kMaxBatchItems) ? kMaxBatchItems : run;
                        while (batch >= 2) {
                            Rect bounds{};
                            std::int64_t sum_area = 0;
                            std::size_t item_offset = offset;
                            for (std::size_t j = 0; j < batch; ++j) {
                                DrawCmd next{};
                                std::size_t item_stride = 0;
                                (void)read_cmd_at_offset(item_offset, next, item_stride);
                                image_items[j].rect = next.rect;
                                if (j == 0) {
                                    bounds = image_items[j].rect;
                                } else {
                                    bounds = rect_union(bounds, image_items[j].rect);
                                }
                                sum_area += rect_area(image_items[j].rect);
                                item_offset += item_stride;
                            }
                            const std::int64_t union_area = rect_area(bounds);
                            const bool area_ok = (sum_area == 0)
                                ? true
                                : (union_area <= (sum_area * union_max_factor));
                            if (!area_ok) {
                                ++batch_shrink_;
                                ++batch_shrink_image_;
                                --batch;
                                continue;
                            }
                            const BlobRef blob = blob_.add_bytes(image_items.data(),
                                                                 batch * sizeof(ImageBatchItem),
                                                                 alignof(ImageBatchItem));
                            if (blob.length != 0) {
                                DrawCmd batch_cmd{};
                                batch_cmd.type = CmdType::DrawImageNineSliceBatch;
                                batch_cmd.rect = bounds;
                                batch_cmd.image = cmd.image;
                                batch_cmd.blob = blob;
                                batch_cmd.p0 = cmd.p0;
                                batch_cmd.p1 = cmd.p1;
                                batch_cmd.p2 = cmd.p2;
                                batch_cmd.p3 = cmd.p3;
                                if (!emit_cmd(batch_cmd)) ok = false;
                                offset = item_offset;
                                break;
                            }
                            ok = false;
                            break;
                        }
                        if (batch >= 2) {
                            continue;
                        }
                    }
                } else if (allow_batch && cmd.type == CmdType::FocusRing) {
                    std::size_t run = 1;
                    std::size_t scan_offset = offset + stride;
                    while (scan_offset < input_bytes) {
                        DrawCmd next{};
                        std::size_t next_stride = 0;
                        if (!read_cmd_at_offset(scan_offset, next, next_stride)) break;
                        if (next.type != CmdType::FocusRing) break;
                        if (!rgba_equal(next.color, cmd.color)) break;
                        if (next.p0 != cmd.p0 || next.p1 != cmd.p1 || next.p2 != cmd.p2) break;
                        scan_offset += next_stride;
                        ++run;
                    }
                    if (run >= 2) {
                        std::size_t batch = (run > kMaxBatchItems) ? kMaxBatchItems : run;
                        while (batch >= 2) {
                            Rect bounds{};
                            std::int64_t sum_area = 0;
                            std::size_t item_offset = offset;
                            for (std::size_t j = 0; j < batch; ++j) {
                                DrawCmd next{};
                                std::size_t item_stride = 0;
                                (void)read_cmd_at_offset(item_offset, next, item_stride);
                                rect_items[j].rect = next.rect;
                                if (j == 0) {
                                    bounds = rect_items[j].rect;
                                } else {
                                    bounds = rect_union(bounds, rect_items[j].rect);
                                }
                                sum_area += rect_area(rect_items[j].rect);
                                item_offset += item_stride;
                            }
                            const std::int64_t union_area = rect_area(bounds);
                            const bool area_ok = (sum_area == 0)
                                ? true
                                : (union_area <= (sum_area * union_max_factor));
                            if (!area_ok) {
                                ++batch_shrink_;
                                ++batch_shrink_focus_;
                                --batch;
                                continue;
                            }
                            const BlobRef blob = blob_.add_bytes(rect_items.data(),
                                                                 batch * sizeof(RectBatchItem),
                                                                 alignof(RectBatchItem));
                            if (blob.length != 0) {
                                DrawCmd batch_cmd{};
                                batch_cmd.type = CmdType::FocusRingBatch;
                                batch_cmd.rect = bounds;
                                batch_cmd.color = cmd.color;
                                batch_cmd.blob = blob;
                                batch_cmd.p0 = cmd.p0;
                                batch_cmd.p1 = cmd.p1;
                                batch_cmd.p2 = cmd.p2;
                                batch_cmd.p3 = static_cast<std::int16_t>(batch);
                                if (!emit_cmd(batch_cmd)) ok = false;
                                offset = item_offset;
                                break;
                            }
                            ok = false;
                            break;
                        }
                        if (batch >= 2) {
                            continue;
                        }
                    }
                }
                if (!emit_cmd(cmd)) ok = false;
                offset += stride;
            }

            if (out_count == 0) {
                count_ = 0;
                cmd_bytes_used_ = 0;
                offsets_valid_ = false;
            } else {
                count_ = out_count;
                cmd_bytes_used_ = out_bytes;
                std::memcpy(cmd_offsets_.data(),
                            output_offsets.data(),
                            out_count * sizeof(std::uint32_t));
                offsets_valid_ = true;
            }
            return ok && !blob_.overflowed();
        }

    private:
        DrawCmd make_cmd(CmdType type, const Rect& rect) noexcept {
            DrawCmd cmd{};
            cmd.type = type;
            cmd.rect = rect;
            return cmd;
        }

        TextSpan add_text(const char* text) noexcept {
            if (!text) text = "";
            const std::size_t len = std::strlen(text);
            if (len + 1 > (kTextCapacity - text_used_)) {
                text_overflowed_ = true;
                return TextSpan{0, 0};
            }
            const std::uint32_t offset = static_cast<std::uint32_t>(text_used_);
            std::memcpy(text_.data() + text_used_, text, len);
            text_[text_used_ + len] = '\0';
            text_used_ += len + 1;
            return TextSpan{offset, static_cast<std::uint16_t>(len)};
        }

        bool push_cmd(const DrawCmd& cmd) noexcept {
            if (!encode_cmd(cmd,
                            cmd_bytes_.data(),
                            kCmdBytesCapacity,
                            cmd_bytes_used_,
                            count_,
                            cmd_offsets_.data())) {
                cmd_overflowed_ = true;
                return false;
            }
            return true;
        }

        bool rebuild_offsets() noexcept {
            std::size_t offset = 0;
            for (std::size_t i = 0; i < count_; ++i) {
                if (offset + sizeof(CmdHeader) > cmd_bytes_used_) return false;
                const auto* header = reinterpret_cast<const CmdHeader*>(cmd_bytes_.data() + offset);
                if (header->size < sizeof(CmdHeader)) return false;
                cmd_offsets_[i] = static_cast<std::uint32_t>(offset);
                const std::size_t stride = cmd_stride(header->size);
                if (stride == 0) return false;
                if (offset + stride > cmd_bytes_used_) return false;
                offset += stride;
            }
            offsets_valid_ = true;
            return true;
        }

        bool rebuild_offsets_from_bytes() noexcept {
            std::size_t offset = 0;
            std::size_t count = 0;
            while (offset < cmd_bytes_used_) {
                if (offset + sizeof(CmdHeader) > cmd_bytes_used_) return false;
                if (count >= kMaxCommands) {
                    cmd_overflowed_ = true;
                    return false;
                }
                const auto* header = reinterpret_cast<const CmdHeader*>(cmd_bytes_.data() + offset);
                if (header->size < sizeof(CmdHeader)) return false;
                cmd_offsets_[count] = static_cast<std::uint32_t>(offset);
                const std::size_t stride = cmd_stride(header->size);
                if (stride == 0) return false;
                if (offset + stride > cmd_bytes_used_) return false;
                offset += stride;
                ++count;
            }
            count_ = count;
            offsets_valid_ = true;
            return true;
        }

        static bool write_cmd(std::byte* base,
                              std::size_t capacity,
                              std::size_t& out_bytes,
                              std::size_t& out_count,
                              std::uint32_t* offsets,
                              CmdType type,
                              const Rect& rect,
                              const void* payload,
                              std::size_t payload_size) noexcept {
            if (out_count >= kMaxCommands) return false;
            const std::size_t cmd_size = sizeof(CmdHeader) + payload_size;
            if (cmd_size > std::numeric_limits<std::uint16_t>::max()) return false;
            const std::size_t stride = cmd_stride(cmd_size);
            if ((out_bytes + stride) > capacity) return false;
            offsets[out_count] = static_cast<std::uint32_t>(out_bytes);
            CmdHeader header{};
            header.type = type;
            header.size = static_cast<std::uint16_t>(cmd_size);
            header.rect = rect;
            std::byte* dst = base + out_bytes;
            std::memcpy(dst, &header, sizeof(CmdHeader));
            if (payload_size > 0 && payload) {
                std::memcpy(dst + sizeof(CmdHeader), payload, payload_size);
            }
            out_bytes += stride;
            ++out_count;
            return true;
        }

        static bool encode_cmd(const DrawCmd& cmd,
                               std::byte* base,
                               std::size_t capacity,
                               std::size_t& out_bytes,
                               std::size_t& out_count,
                               std::uint32_t* offsets) noexcept {
            switch (cmd.type) {
            case CmdType::PushClip:
            case CmdType::PopClip:
                return write_cmd(base, capacity, out_bytes, out_count, offsets,
                                 cmd.type, cmd.rect, nullptr, 0);
            case CmdType::DrawLine: {
                CmdColor payload{cmd.color};
                return write_cmd(base, capacity, out_bytes, out_count, offsets,
                                 cmd.type, cmd.rect, &payload, sizeof(payload));
            }
            case CmdType::DrawPath: {
                CmdPath payload{};
                payload.color = cmd.color;
                payload.blob = cmd.blob;
                payload.count = cmd.p0;
                payload.closed = cmd.p1;
                return write_cmd(base, capacity, out_bytes, out_count, offsets,
                                 cmd.type, cmd.rect, &payload, sizeof(payload));
            }
            case CmdType::FillRect:
            case CmdType::StrokeRect: {
                CmdColor payload{cmd.color};
                return write_cmd(base, capacity, out_bytes, out_count, offsets,
                                 cmd.type, cmd.rect, &payload, sizeof(payload));
            }
            case CmdType::FillRoundRect:
            case CmdType::StrokeRoundRect:
            case CmdType::FillCircle:
            case CmdType::StrokeCircle: {
                CmdColorP0 payload{};
                payload.color = cmd.color;
                payload.p0 = cmd.p0;
                return write_cmd(base, capacity, out_bytes, out_count, offsets,
                                 cmd.type, cmd.rect, &payload, sizeof(payload));
            }
            case CmdType::DrawImage: {
                CmdImage payload{cmd.image};
                return write_cmd(base, capacity, out_bytes, out_count, offsets,
                                 cmd.type, cmd.rect, &payload, sizeof(payload));
            }
            case CmdType::DrawImageRoundRect: {
                CmdImageP0 payload{};
                payload.image = cmd.image;
                payload.p0 = cmd.p0;
                return write_cmd(base, capacity, out_bytes, out_count, offsets,
                                 cmd.type, cmd.rect, &payload, sizeof(payload));
            }
            case CmdType::DrawImageNineSlice: {
                CmdImageP0P1P2P3 payload{};
                payload.image = cmd.image;
                payload.p0 = cmd.p0;
                payload.p1 = cmd.p1;
                payload.p2 = cmd.p2;
                payload.p3 = cmd.p3;
                return write_cmd(base, capacity, out_bytes, out_count, offsets,
                                 cmd.type, cmd.rect, &payload, sizeof(payload));
            }
            case CmdType::DrawTextBox: {
                CmdTextBox payload{};
                payload.color = cmd.color;
                payload.text = cmd.text;
                payload.font = cmd.font;
                payload.align_h = cmd.align_h;
                payload.align_v = cmd.align_v;
                payload.wrap = cmd.wrap;
                payload.ellipsis = cmd.ellipsis;
                return write_cmd(base, capacity, out_bytes, out_count, offsets,
                                 cmd.type, cmd.rect, &payload, sizeof(payload));
            }
            case CmdType::FocusRing: {
                CmdColorP0P1P2 payload{};
                payload.color = cmd.color;
                payload.p0 = cmd.p0;
                payload.p1 = cmd.p1;
                payload.p2 = cmd.p2;
                return write_cmd(base, capacity, out_bytes, out_count, offsets,
                                 cmd.type, cmd.rect, &payload, sizeof(payload));
            }
            case CmdType::FillRectBatch:
            case CmdType::StrokeRectBatch: {
                CmdBatchRect payload{};
                payload.color = cmd.color;
                payload.blob = cmd.blob;
                payload.count = cmd.p0;
                return write_cmd(base, capacity, out_bytes, out_count, offsets,
                                 cmd.type, cmd.rect, &payload, sizeof(payload));
            }
            case CmdType::DrawLineBatch: {
                CmdBatchLine payload{};
                payload.color = cmd.color;
                payload.blob = cmd.blob;
                payload.count = cmd.p0;
                return write_cmd(base, capacity, out_bytes, out_count, offsets,
                                 cmd.type, cmd.rect, &payload, sizeof(payload));
            }
            case CmdType::DrawPathBatch: {
                CmdBatchPath payload{};
                payload.color = cmd.color;
                payload.blob = cmd.blob;
                payload.count = cmd.p0;
                return write_cmd(base, capacity, out_bytes, out_count, offsets,
                                 cmd.type, cmd.rect, &payload, sizeof(payload));
            }
            case CmdType::FocusRingBatch: {
                CmdBatchRectP0P1P2 payload{};
                payload.color = cmd.color;
                payload.blob = cmd.blob;
                payload.p0 = cmd.p0;
                payload.p1 = cmd.p1;
                payload.p2 = cmd.p2;
                payload.count = cmd.p3;
                return write_cmd(base, capacity, out_bytes, out_count, offsets,
                                 cmd.type, cmd.rect, &payload, sizeof(payload));
            }
            case CmdType::FillRoundRectBatch:
            case CmdType::StrokeRoundRectBatch:
            case CmdType::FillCircleBatch:
            case CmdType::StrokeCircleBatch: {
                CmdBatchRectP0 payload{};
                payload.color = cmd.color;
                payload.blob = cmd.blob;
                payload.p0 = cmd.p0;
                payload.count = cmd.p1;
                return write_cmd(base, capacity, out_bytes, out_count, offsets,
                                 cmd.type, cmd.rect, &payload, sizeof(payload));
            }
            case CmdType::GlyphRun: {
                CmdGlyphRun payload{};
                payload.color = cmd.color;
                payload.blob = cmd.blob;
                payload.font = cmd.font;
                payload.align_h = cmd.align_h;
                payload.align_v = cmd.align_v;
                payload.wrap = cmd.wrap;
                payload.ellipsis = cmd.ellipsis;
                payload.count = static_cast<std::uint16_t>(cmd.p0);
                return write_cmd(base, capacity, out_bytes, out_count, offsets,
                                 cmd.type, cmd.rect, &payload, sizeof(payload));
            }
            case CmdType::DrawImageBatch: {
                CmdImageBatch payload{};
                payload.image = cmd.image;
                payload.blob = cmd.blob;
                payload.count = cmd.p0;
                return write_cmd(base, capacity, out_bytes, out_count, offsets,
                                 cmd.type, cmd.rect, &payload, sizeof(payload));
            }
            case CmdType::DrawImageRoundRectBatch: {
                CmdImageBatchP0 payload{};
                payload.image = cmd.image;
                payload.blob = cmd.blob;
                payload.p0 = cmd.p0;
                payload.count = cmd.p1;
                return write_cmd(base, capacity, out_bytes, out_count, offsets,
                                 cmd.type, cmd.rect, &payload, sizeof(payload));
            }
            case CmdType::DrawImageNineSliceBatch: {
                CmdImageBatchP0P1P2P3 payload{};
                payload.image = cmd.image;
                payload.blob = cmd.blob;
                payload.p0 = cmd.p0;
                payload.p1 = cmd.p1;
                payload.p2 = cmd.p2;
                payload.p3 = cmd.p3;
                return write_cmd(base, capacity, out_bytes, out_count, offsets,
                                 cmd.type, cmd.rect, &payload, sizeof(payload));
            }
            default:
                return false;
            }
        }

        alignas(kCmdAlign) std::array<std::byte, kCmdBytesCapacity> cmd_bytes_{};
        std::array<std::uint32_t, kMaxCommands> cmd_offsets_{};
        std::array<char, kTextCapacity> text_{};
        BlobArena<kBlobCapacity> blob_{};
        std::size_t count_{0};
        std::size_t cmd_bytes_used_{0};
        std::size_t text_used_{1};
        std::size_t batch_shrink_{0};
        std::size_t batch_shrink_line_{0};
        std::size_t batch_shrink_path_{0};
        std::size_t batch_shrink_rect_{0};
        std::size_t batch_shrink_round_{0};
        std::size_t batch_shrink_image_{0};
        std::size_t batch_shrink_focus_{0};
        bool cmd_overflowed_{false};
        bool text_overflowed_{false};
        bool offsets_valid_{true};
    };

    constexpr std::size_t kDefaultCmdCapacity = 1024;
    constexpr std::size_t kDefaultTextCapacity = 4096;
    constexpr std::size_t kDefaultBlobCapacity = 2048;
    using DefaultDrawCmdBuffer = DrawCmdBuffer<kDefaultCmdCapacity, kDefaultTextCapacity, kDefaultBlobCapacity>;

    class DrawCmdExecutor {
    public:
        template <class Buffer>
        DrawCmdExecStats execute(CanvasBase& canvas,
                                 const Buffer& buf,
                                 const Rect* initial_clip = nullptr) noexcept {
            DrawCmdExecStats stats{};
            stats.cmd_count = buf.size();
            stats.cmd_bytes = buf.cmd_bytes();
            stats.overflowed = buf.overflowed();

            std::array<CanvasBase::ClipState, 64> clip_stack{};
            std::size_t sp = 0;
            const auto base_clip = canvas.save_clip();
            if (initial_clip) {
                canvas.set_clip(*initial_clip);
            }

            const std::size_t cmd_bytes = buf.cmd_bytes();
            std::size_t offset = 0;
            auto read_cmd_at_offset = [&](std::size_t at, DrawCmd& out, std::size_t& stride) noexcept -> bool {
                return buf.read_cmd_at_offset(at, out, stride);
            };
            auto fail_text = [&]() noexcept {
                stats.failed_cmds++;
                stats.fail_text++;
            };
            auto fail_image = [&]() noexcept {
                stats.failed_cmds++;
                stats.fail_image++;
            };
            auto fail_blob = [&]() noexcept {
                stats.failed_cmds++;
                stats.fail_blob++;
            };
            auto fail_path = [&]() noexcept {
                stats.failed_cmds++;
                stats.fail_path++;
            };
            auto fail_clip = [&]() noexcept {
                stats.failed_cmds++;
                stats.fail_clip++;
            };
            auto fail_other = [&]() noexcept {
                stats.failed_cmds++;
                stats.fail_other++;
            };
            auto exec_rect_like = [&](const DrawCmd& cur) noexcept {
                switch (cur.type) {
                case CmdType::FillRect:
                    ui::render::draw_rect(canvas, cur.rect.x, cur.rect.y, cur.rect.w, cur.rect.h, cur.color, true);
                    break;
                case CmdType::StrokeRect:
                    ui::render::draw_rect(canvas, cur.rect.x, cur.rect.y, cur.rect.w, cur.rect.h, cur.color, false);
                    break;
                case CmdType::FillRoundRect:
                    ui::render::draw_round_rect(canvas, cur.rect.x, cur.rect.y, cur.rect.w, cur.rect.h, cur.p0, cur.color, true);
                    break;
                case CmdType::StrokeRoundRect:
                    ui::render::draw_round_rect(canvas, cur.rect.x, cur.rect.y, cur.rect.w, cur.rect.h, cur.p0, cur.color, false);
                    break;
                case CmdType::FillCircle: {
                    const int radius = cur.p0;
                    const int cx = cur.rect.x + cur.rect.w / 2;
                    const int cy = cur.rect.y + cur.rect.h / 2;
                    ui::render::draw_circle(canvas, cx, cy, radius, cur.color, true);
                    break;
                }
                case CmdType::StrokeCircle: {
                    const int radius = cur.p0;
                    const int cx = cur.rect.x + cur.rect.w / 2;
                    const int cy = cur.rect.y + cur.rect.h / 2;
                    ui::render::draw_circle(canvas, cx, cy, radius, cur.color, false);
                    break;
                }
                default:
                    break;
                }
            };
            auto rectlike_batchable = [](CmdType type) noexcept {
                switch (type) {
                case CmdType::FillRect:
                case CmdType::StrokeRect:
                case CmdType::FillRoundRect:
                case CmdType::StrokeRoundRect:
                case CmdType::FillCircle:
                case CmdType::StrokeCircle:
                case CmdType::FocusRing:
                    return true;
                default:
                    return false;
                }
            };
            auto rectlike_params_match = [&](const DrawCmd& a, const DrawCmd& b) noexcept {
                if (a.type != b.type) return false;
                if (!rgba_equal(a.color, b.color)) return false;
                switch (a.type) {
                case CmdType::FillRect:
                case CmdType::StrokeRect:
                    return true;
                case CmdType::FillRoundRect:
                case CmdType::StrokeRoundRect:
                case CmdType::FillCircle:
                case CmdType::StrokeCircle:
                    return a.p0 == b.p0;
                case CmdType::FocusRing:
                    return a.p0 == b.p0 && a.p1 == b.p1 && a.p2 == b.p2;
                default:
                    return false;
                }
            };
            auto exec_rectlike_item = [&](const DrawCmd& cur, const Rect& rect) noexcept {
                switch (cur.type) {
                case CmdType::FillRect:
                    ui::render::draw_rect(canvas, rect.x, rect.y, rect.w, rect.h, cur.color, true);
                    break;
                case CmdType::StrokeRect:
                    ui::render::draw_rect(canvas, rect.x, rect.y, rect.w, rect.h, cur.color, false);
                    break;
                case CmdType::FillRoundRect:
                    ui::render::draw_round_rect(canvas, rect.x, rect.y, rect.w, rect.h, cur.p0, cur.color, true);
                    break;
                case CmdType::StrokeRoundRect:
                    ui::render::draw_round_rect(canvas, rect.x, rect.y, rect.w, rect.h, cur.p0, cur.color, false);
                    break;
                case CmdType::FillCircle: {
                    const int radius = cur.p0;
                    const int cx = rect.x + rect.w / 2;
                    const int cy = rect.y + rect.h / 2;
                    ui::render::draw_circle(canvas, cx, cy, radius, cur.color, true);
                    break;
                }
                case CmdType::StrokeCircle: {
                    const int radius = cur.p0;
                    const int cx = rect.x + rect.w / 2;
                    const int cy = rect.y + rect.h / 2;
                    ui::render::draw_circle(canvas, cx, cy, radius, cur.color, false);
                    break;
                }
                case CmdType::FocusRing:
                    ui::render::draw_focus_ring(canvas, rect, cur.color, cur.p0, true, cur.p1, cur.p2);
                    break;
                default:
                    break;
                }
            };

            auto exec_image_like = [&](const DrawCmd& cur) noexcept {
                switch (cur.type) {
                case CmdType::DrawImage: {
                    const auto* image = resolve_image(cur.image);
                    if (!image || !(*image)) {
                        fail_image();
                        break;
                    }
                    if (cur.rect.w > 0 && cur.rect.h > 0) {
                        ui::render::draw_image_scaled(canvas, cur.rect.x, cur.rect.y,
                                                      cur.rect.w, cur.rect.h, *image);
                    } else {
                        ui::render::draw_image(canvas, cur.rect.x, cur.rect.y, *image);
                    }
                    break;
                }
                case CmdType::DrawImageRoundRect: {
                    const auto* image = resolve_image(cur.image);
                    if (!image || !(*image)) {
                        fail_image();
                        break;
                    }
                    if (cur.rect.w > 0 && cur.rect.h > 0) {
                        ui::render::draw_image_scaled_round_rect(canvas, cur.rect.x, cur.rect.y,
                                                                  cur.rect.w, cur.rect.h, *image, cur.p0);
                    } else {
                        ui::render::draw_image_round_rect(canvas, cur.rect.x, cur.rect.y,
                                                          *image, cur.p0);
                    }
                    break;
                }
                case CmdType::DrawImageNineSlice: {
                    const auto* image = resolve_image(cur.image);
                    if (!image || !(*image)) {
                        fail_image();
                        break;
                    }
                    ui::render::draw_image_nine_slice(canvas,
                                                      cur.rect.x, cur.rect.y,
                                                      cur.rect.w, cur.rect.h,
                                                      *image,
                                                      cur.p0, cur.p1, cur.p2, cur.p3);
                    break;
                }
                default:
                    break;
                }
            };
            auto imagelike_batchable = [](CmdType type) noexcept {
                switch (type) {
                case CmdType::DrawImage:
                case CmdType::DrawImageRoundRect:
                case CmdType::DrawImageNineSlice:
                    return true;
                default:
                    return false;
                }
            };
            auto imagelike_params_match = [&](const DrawCmd& a, const DrawCmd& b) noexcept {
                if (a.type != b.type) return false;
                if (a.image != b.image) return false;
                switch (a.type) {
                case CmdType::DrawImage:
                    return true;
                case CmdType::DrawImageRoundRect:
                    return a.p0 == b.p0;
                case CmdType::DrawImageNineSlice:
                    return a.p0 == b.p0 && a.p1 == b.p1 && a.p2 == b.p2 && a.p3 == b.p3;
                default:
                    return false;
                }
            };
            auto exec_imagelike_item = [&](const DrawCmd& cur, const Rect& rect) noexcept {
                switch (cur.type) {
                case CmdType::DrawImage: {
                    const auto* image = resolve_image(cur.image);
                    if (!image || !(*image)) {
                        fail_image();
                        break;
                    }
                    if (rect.w > 0 && rect.h > 0) {
                        ui::render::draw_image_scaled(canvas, rect.x, rect.y, rect.w, rect.h, *image);
                    } else {
                        ui::render::draw_image(canvas, rect.x, rect.y, *image);
                    }
                    break;
                }
                case CmdType::DrawImageRoundRect: {
                    const auto* image = resolve_image(cur.image);
                    if (!image || !(*image)) {
                        fail_image();
                        break;
                    }
                    if (rect.w > 0 && rect.h > 0) {
                        ui::render::draw_image_scaled_round_rect(canvas, rect.x, rect.y,
                                                                  rect.w, rect.h, *image, cur.p0);
                    } else {
                        ui::render::draw_image_round_rect(canvas, rect.x, rect.y,
                                                          *image, cur.p0);
                    }
                    break;
                }
                case CmdType::DrawImageNineSlice: {
                    const auto* image = resolve_image(cur.image);
                    if (!image || !(*image)) {
                        fail_image();
                        break;
                    }
                    ui::render::draw_image_nine_slice(canvas,
                                                      rect.x, rect.y,
                                                      rect.w, rect.h,
                                                      *image,
                                                      cur.p0, cur.p1, cur.p2, cur.p3);
                    break;
                }
                default:
                    break;
                }
            };

            auto exec_draw_line = [&](const DrawCmd& cur) noexcept {
                ui::render::draw_line(canvas,
                                      cur.rect.x,
                                      cur.rect.y,
                                      cur.rect.w,
                                      cur.rect.h,
                                      cur.color);
            };

            auto exec_draw_path = [&](const DrawCmd& cur) noexcept {
                const int count = cur.p0;
                if (count < 2) {
                    fail_path();
                    return;
                }
                const auto blob = buf.blob_at(cur.blob);
                const auto points = ui::gfx::path::decode_points(blob, count);
                if (points.empty()) {
                    fail_path();
                    return;
                }
                const bool closed = (cur.p1 != 0);
                ui::gfx::path::stroke_path(canvas, points, closed, cur.color);
            };

            auto exec_line_batch = [&](const DrawCmd& cur) noexcept {
                const int count = cur.p0;
                if (count <= 0) {
                    fail_other();
                    return;
                }
                const auto blob = buf.blob_at(cur.blob);
                if (blob.size() < static_cast<std::size_t>(count) * sizeof(LineBatchItem)) {
                    fail_blob();
                    return;
                }
                const auto items = std::span<const LineBatchItem>(
                    reinterpret_cast<const LineBatchItem*>(blob.data()), count);
                for (const auto& item : items) {
                    ui::render::draw_line(canvas, item.x0, item.y0, item.x1, item.y1, cur.color);
                }
            };

            auto exec_path_batch = [&](const DrawCmd& cur) noexcept {
                const int count = cur.p0;
                if (count <= 0) {
                    fail_other();
                    return;
                }
                const auto blob = buf.blob_at(cur.blob);
                if (blob.size() < static_cast<std::size_t>(count) * sizeof(PathBatchItem)) {
                    fail_blob();
                    return;
                }
                const auto items = std::span<const PathBatchItem>(
                    reinterpret_cast<const PathBatchItem*>(blob.data()), count);
                for (const auto& item : items) {
                    if (item.count < 2) {
                        fail_path();
                        continue;
                    }
                    const auto path_blob = buf.blob_at(item.blob);
                    const auto points = ui::gfx::path::decode_points(path_blob, item.count);
                    if (points.empty()) {
                        fail_path();
                        continue;
                    }
                    const bool closed = (item.closed != 0);
                    ui::gfx::path::stroke_path(canvas, points, closed, cur.color);
                }
            };

            auto exec_rect_batch = [&](const DrawCmd& cur, bool fill) noexcept {
                const int count = cur.p0;
                if (count <= 0) {
                    fail_other();
                    return;
                }
                const auto blob = buf.blob_at(cur.blob);
                if (blob.size() < static_cast<std::size_t>(count) * sizeof(RectBatchItem)) {
                    fail_blob();
                    return;
                }
                const auto items = std::span<const RectBatchItem>(
                    reinterpret_cast<const RectBatchItem*>(blob.data()), count);
                for (const auto& item : items) {
                    ui::render::draw_rect(canvas,
                                          item.rect.x, item.rect.y,
                                          item.rect.w, item.rect.h,
                                          cur.color, fill);
                }
            };

            auto exec_round_batch = [&](const DrawCmd& cur, bool fill) noexcept {
                const int count = cur.p1;
                if (count <= 0) {
                    fail_other();
                    return;
                }
                const auto blob = buf.blob_at(cur.blob);
                if (blob.size() < static_cast<std::size_t>(count) * sizeof(RectBatchItem)) {
                    fail_blob();
                    return;
                }
                const auto items = std::span<const RectBatchItem>(
                    reinterpret_cast<const RectBatchItem*>(blob.data()), count);
                for (const auto& item : items) {
                    ui::render::draw_round_rect(canvas,
                                                item.rect.x, item.rect.y,
                                                item.rect.w, item.rect.h,
                                                cur.p0,
                                                cur.color,
                                                fill);
                }
            };

            auto exec_circle_batch = [&](const DrawCmd& cur, bool fill) noexcept {
                const int count = cur.p1;
                if (count <= 0) {
                    fail_other();
                    return;
                }
                const auto blob = buf.blob_at(cur.blob);
                if (blob.size() < static_cast<std::size_t>(count) * sizeof(RectBatchItem)) {
                    fail_blob();
                    return;
                }
                const auto items = std::span<const RectBatchItem>(
                    reinterpret_cast<const RectBatchItem*>(blob.data()), count);
                const int radius = cur.p0;
                for (const auto& item : items) {
                    const int cx = item.rect.x + item.rect.w / 2;
                    const int cy = item.rect.y + item.rect.h / 2;
                    ui::render::draw_circle(canvas, cx, cy, radius, cur.color, fill);
                }
            };

            auto exec_focus_batch = [&](const DrawCmd& cur) noexcept {
                const int count = cur.p3;
                if (count <= 0) {
                    fail_other();
                    return;
                }
                const auto blob = buf.blob_at(cur.blob);
                if (blob.size() < static_cast<std::size_t>(count) * sizeof(RectBatchItem)) {
                    fail_blob();
                    return;
                }
                const auto items = std::span<const RectBatchItem>(
                    reinterpret_cast<const RectBatchItem*>(blob.data()), count);
                for (const auto& item : items) {
                    ui::render::draw_focus_ring(canvas, item.rect, cur.color, cur.p0, true, cur.p1, cur.p2);
                }
            };

            auto exec_glyph_run = [&](const DrawCmd& cur) noexcept {
                const int count = cur.p0;
                if (count <= 0) {
                    fail_other();
                    return;
                }
                const auto blob = buf.blob_at(cur.blob);
                if (blob.size() < static_cast<std::size_t>(count) * sizeof(GlyphRunItem)) {
                    fail_blob();
                    return;
                }
                const auto items = std::span<const GlyphRunItem>(
                    reinterpret_cast<const GlyphRunItem*>(blob.data()), count);
                const Font& font = get_font(cur.font);
                for (const auto& item : items) {
                    if (!buf.text_span_valid(item.text)) {
                        fail_text();
                        continue;
                    }
                    const char* text = buf.text_at(item.text.offset);
                    draw_text_box(canvas, item.rect, text, cur.color, font,
                                  cur.align_h, cur.align_v, cur.wrap, cur.ellipsis);
                }
            };

            auto exec_image_batch = [&](const DrawCmd& cur) noexcept {
                const int count = cur.p0;
                if (count <= 0) {
                    fail_other();
                    return;
                }
                const auto* image = resolve_image(cur.image);
                if (!image || !(*image)) {
                    fail_image();
                    return;
                }
                const auto blob = buf.blob_at(cur.blob);
                if (blob.size() < static_cast<std::size_t>(count) * sizeof(ImageBatchItem)) {
                    fail_blob();
                    return;
                }
                const auto items = std::span<const ImageBatchItem>(
                    reinterpret_cast<const ImageBatchItem*>(blob.data()), count);
                for (const auto& item : items) {
                    if (item.rect.w > 0 && item.rect.h > 0) {
                        ui::render::draw_image_scaled(canvas, item.rect.x, item.rect.y,
                                                      item.rect.w, item.rect.h, *image);
                    } else {
                        ui::render::draw_image(canvas, item.rect.x, item.rect.y, *image);
                    }
                }
            };

            auto exec_image_round_batch = [&](const DrawCmd& cur) noexcept {
                const int count = cur.p1;
                if (count <= 0) {
                    fail_other();
                    return;
                }
                const auto* image = resolve_image(cur.image);
                if (!image || !(*image)) {
                    fail_image();
                    return;
                }
                const auto blob = buf.blob_at(cur.blob);
                if (blob.size() < static_cast<std::size_t>(count) * sizeof(ImageBatchItem)) {
                    fail_blob();
                    return;
                }
                const auto items = std::span<const ImageBatchItem>(
                    reinterpret_cast<const ImageBatchItem*>(blob.data()), count);
                for (const auto& item : items) {
                    if (item.rect.w > 0 && item.rect.h > 0) {
                        ui::render::draw_image_scaled_round_rect(canvas, item.rect.x, item.rect.y,
                                                                 item.rect.w, item.rect.h, *image, cur.p0);
                    } else {
                        ui::render::draw_image_round_rect(canvas, item.rect.x, item.rect.y,
                                                          *image, cur.p0);
                    }
                }
            };

            auto exec_image_nine_batch = [&](const DrawCmd& cur) noexcept {
                const auto* image = resolve_image(cur.image);
                if (!image || !(*image)) {
                    fail_image();
                    return;
                }
                const auto blob = buf.blob_at(cur.blob);
                if (blob.size() < sizeof(ImageBatchItem)
                    || (blob.size() % sizeof(ImageBatchItem)) != 0) {
                    fail_blob();
                    return;
                }
                const auto count = static_cast<int>(blob.size() / sizeof(ImageBatchItem));
                const auto items = std::span<const ImageBatchItem>(
                    reinterpret_cast<const ImageBatchItem*>(blob.data()), count);
                for (const auto& item : items) {
                    ui::render::draw_image_nine_slice(canvas,
                                                      item.rect.x, item.rect.y,
                                                      item.rect.w, item.rect.h,
                                                      *image,
                                                      cur.p0, cur.p1, cur.p2, cur.p3);
                }
            };

            enum class GroupKind : std::uint8_t {
                None,
                RectLike,
                TextBox,
                ImageLike,
                DrawLine,
                DrawPath
            };

            auto group_kind = [](CmdType type) noexcept -> GroupKind {
                switch (type) {
                case CmdType::FillRect:
                case CmdType::StrokeRect:
                case CmdType::FillRoundRect:
                case CmdType::StrokeRoundRect:
                case CmdType::FillCircle:
                case CmdType::StrokeCircle:
                case CmdType::FillRectBatch:
                case CmdType::StrokeRectBatch:
                case CmdType::FillRoundRectBatch:
                case CmdType::StrokeRoundRectBatch:
                case CmdType::FillCircleBatch:
                case CmdType::StrokeCircleBatch:
                case CmdType::FocusRing:
                case CmdType::FocusRingBatch:
                    return GroupKind::RectLike;
                case CmdType::DrawTextBox:
                case CmdType::GlyphRun:
                    return GroupKind::TextBox;
                case CmdType::DrawImage:
                case CmdType::DrawImageRoundRect:
                case CmdType::DrawImageNineSlice:
                case CmdType::DrawImageBatch:
                case CmdType::DrawImageRoundRectBatch:
                case CmdType::DrawImageNineSliceBatch:
                    return GroupKind::ImageLike;
                case CmdType::DrawLine:
                case CmdType::DrawLineBatch:
                    return GroupKind::DrawLine;
                case CmdType::DrawPath:
                case CmdType::DrawPathBatch:
                    return GroupKind::DrawPath;
                default:
                    return GroupKind::None;
                }
            };

            auto exec_group_cmd = [&](const DrawCmd& cur, GroupKind kind) noexcept {
                switch (kind) {
                case GroupKind::RectLike:
                    switch (cur.type) {
                    case CmdType::FillRect:
                    case CmdType::StrokeRect:
                    case CmdType::FillRoundRect:
                    case CmdType::StrokeRoundRect:
                    case CmdType::FillCircle:
                    case CmdType::StrokeCircle:
                        exec_rect_like(cur);
                        break;
                    case CmdType::FillRectBatch:
                        exec_rect_batch(cur, true);
                        break;
                    case CmdType::StrokeRectBatch:
                        exec_rect_batch(cur, false);
                        break;
                    case CmdType::FillRoundRectBatch:
                        exec_round_batch(cur, true);
                        break;
                    case CmdType::StrokeRoundRectBatch:
                        exec_round_batch(cur, false);
                        break;
                    case CmdType::FillCircleBatch:
                        exec_circle_batch(cur, true);
                        break;
                    case CmdType::StrokeCircleBatch:
                        exec_circle_batch(cur, false);
                        break;
                    case CmdType::FocusRing:
                        ui::render::draw_focus_ring(canvas, cur.rect, cur.color, cur.p0, true, cur.p1, cur.p2);
                        break;
                    case CmdType::FocusRingBatch:
                        exec_focus_batch(cur);
                        break;
                    default:
                        break;
                    }
                    break;
                case GroupKind::TextBox: {
                    if (cur.type == CmdType::GlyphRun) {
                        exec_glyph_run(cur);
                        break;
                    }
                    if (!buf.text_span_valid(cur.text)) {
                        fail_text();
                        return;
                    }
                    const char* text = buf.text_at(cur.text.offset);
                    const Font& font = get_font(cur.font);
                    draw_text_box(canvas, cur.rect, text, cur.color, font,
                                  cur.align_h, cur.align_v, cur.wrap, cur.ellipsis);
                    break;
                }
                case GroupKind::ImageLike:
                    switch (cur.type) {
                    case CmdType::DrawImage:
                    case CmdType::DrawImageRoundRect:
                    case CmdType::DrawImageNineSlice:
                        exec_image_like(cur);
                        break;
                    case CmdType::DrawImageBatch:
                        exec_image_batch(cur);
                        break;
                    case CmdType::DrawImageRoundRectBatch:
                        exec_image_round_batch(cur);
                        break;
                    case CmdType::DrawImageNineSliceBatch:
                        exec_image_nine_batch(cur);
                        break;
                    default:
                        break;
                    }
                    break;
                case GroupKind::DrawLine:
                    if (cur.type == CmdType::DrawLineBatch) {
                        exec_line_batch(cur);
                    } else {
                        exec_draw_line(cur);
                    }
                    break;
                case GroupKind::DrawPath:
                    if (cur.type == CmdType::DrawPathBatch) {
                        exec_path_batch(cur);
                    } else {
                        exec_draw_path(cur);
                    }
                    break;
                case GroupKind::None:
                default:
                    break;
                }
            };

            auto count_group = [&](GroupKind kind) noexcept {
                switch (kind) {
                case GroupKind::RectLike:
                    stats.group_rect++;
                    break;
                case GroupKind::TextBox:
                    stats.group_text++;
                    break;
                case GroupKind::ImageLike:
                    stats.group_image++;
                    break;
                case GroupKind::DrawLine:
                    stats.group_line++;
                    break;
                case GroupKind::DrawPath:
                    stats.group_path++;
                    break;
                case GroupKind::None:
                default:
                    stats.group_other++;
                    break;
                }
            };

            auto count_cmd = [&](GroupKind kind) noexcept {
                switch (kind) {
                case GroupKind::RectLike:
                    stats.cmd_rect++;
                    break;
                case GroupKind::TextBox:
                    stats.cmd_text++;
                    break;
                case GroupKind::ImageLike:
                    stats.cmd_image++;
                    break;
                case GroupKind::DrawLine:
                    stats.cmd_line++;
                    break;
                case GroupKind::DrawPath:
                    stats.cmd_path++;
                    break;
                case GroupKind::None:
                default:
                    stats.cmd_other++;
                    break;
                }
            };
            auto text_params_match = [&](const DrawCmd& a, const DrawCmd& b) noexcept {
                return rgba_equal(a.color, b.color)
                    && a.font == b.font
                    && a.align_h == b.align_h
                    && a.align_v == b.align_v
                    && a.wrap == b.wrap
                    && a.ellipsis == b.ellipsis;
            };

            while (offset < cmd_bytes) {
                DrawCmd cmd{};
                std::size_t stride = 0;
                if (!read_cmd_at_offset(offset, cmd, stride)) {
                    fail_other();
                    continue;
                }

                const auto kind = group_kind(cmd.type);
                if (kind != GroupKind::None) {
                    stats.dispatch_groups++;
                    count_group(kind);
                    if (kind == GroupKind::RectLike) {
                        constexpr std::size_t kMaxExecBatchItems = 64;
                        std::array<RectBatchItem, kMaxExecBatchItems> rect_items{};
                        while (true) {
                            DrawCmd cur = cmd;
                            std::size_t cur_stride = stride;
                            if (rectlike_batchable(cur.type)) {
                                std::size_t run = 1;
                                rect_items[0].rect = cur.rect;
                                std::size_t scan_offset = offset + cur_stride;
                                while (scan_offset < cmd_bytes && run < kMaxExecBatchItems) {
                                    DrawCmd next{};
                                    std::size_t next_stride = 0;
                                    if (!read_cmd_at_offset(scan_offset, next, next_stride)) {
                                        fail_other();
                                        break;
                                    }
                                    if (group_kind(next.type) != kind) break;
                                    if (!rectlike_batchable(next.type)) break;
                                    if (!rectlike_params_match(cur, next)) break;
                                    rect_items[run].rect = next.rect;
                                    scan_offset += next_stride;
                                    ++run;
                                }
                                for (std::size_t j = 0; j < run; ++j) {
                                    exec_rectlike_item(cur, rect_items[j].rect);
                                    count_cmd(kind);
                                }
                                offset = scan_offset;
                            } else {
                                exec_group_cmd(cur, kind);
                                count_cmd(kind);
                                offset += cur_stride;
                            }
                            if (offset >= cmd_bytes) break;
                            if (!read_cmd_at_offset(offset, cmd, stride)) {
                                fail_other();
                                offset = cmd_bytes;
                                continue;
                            }
                            if (group_kind(cmd.type) != kind) break;
                        }
                        stats.batch_flushes++;
                        continue;
                    }
                    if (kind == GroupKind::ImageLike) {
                        constexpr std::size_t kMaxExecBatchItems = 64;
                        std::array<RectBatchItem, kMaxExecBatchItems> rect_items{};
                        while (true) {
                            DrawCmd cur = cmd;
                            std::size_t cur_stride = stride;
                            if (imagelike_batchable(cur.type)) {
                                std::size_t run = 1;
                                rect_items[0].rect = cur.rect;
                                std::size_t scan_offset = offset + cur_stride;
                                while (scan_offset < cmd_bytes && run < kMaxExecBatchItems) {
                                    DrawCmd next{};
                                    std::size_t next_stride = 0;
                                    if (!read_cmd_at_offset(scan_offset, next, next_stride)) {
                                        fail_other();
                                        break;
                                    }
                                    if (group_kind(next.type) != kind) break;
                                    if (!imagelike_batchable(next.type)) break;
                                    if (!imagelike_params_match(cur, next)) break;
                                    rect_items[run].rect = next.rect;
                                    scan_offset += next_stride;
                                    ++run;
                                }
                                for (std::size_t j = 0; j < run; ++j) {
                                    exec_imagelike_item(cur, rect_items[j].rect);
                                    count_cmd(kind);
                                }
                                offset = scan_offset;
                            } else {
                                exec_group_cmd(cur, kind);
                                count_cmd(kind);
                                offset += cur_stride;
                            }
                            if (offset >= cmd_bytes) break;
                            if (!read_cmd_at_offset(offset, cmd, stride)) {
                                fail_other();
                                offset = cmd_bytes;
                                continue;
                            }
                            if (group_kind(cmd.type) != kind) break;
                        }
                        stats.batch_flushes++;
                        continue;
                    }
                    if (kind == GroupKind::TextBox) {
                        constexpr std::size_t kMaxExecBatchItems = 64;
                        while (true) {
                            DrawCmd cur = cmd;
                            std::size_t cur_stride = stride;
                            if (cur.type == CmdType::GlyphRun) {
                                exec_group_cmd(cur, kind);
                                count_cmd(kind);
                                offset += cur_stride;
                            } else if (cur.type == CmdType::DrawTextBox) {
                                if (!buf.text_span_valid(cur.text)) {
                                    fail_text();
                                    offset += cur_stride;
                                } else {
                                    std::size_t run = 1;
                                    std::size_t scan_offset = offset + cur_stride;
                                    while (scan_offset < cmd_bytes && run < kMaxExecBatchItems) {
                                        DrawCmd next{};
                                        std::size_t next_stride = 0;
                                        if (!read_cmd_at_offset(scan_offset, next, next_stride)) {
                                            fail_other();
                                            break;
                                        }
                                        if (group_kind(next.type) != kind) break;
                                        if (next.type != CmdType::DrawTextBox) break;
                                        if (!buf.text_span_valid(next.text)) break;
                                        if (!text_params_match(cur, next)) break;
                                        scan_offset += next_stride;
                                        ++run;
                                    }
                                    std::size_t item_offset = offset;
                                    for (std::size_t j = 0; j < run; ++j) {
                                        DrawCmd item{};
                                        std::size_t item_stride = 0;
                                        if (!read_cmd_at_offset(item_offset, item, item_stride)) {
                                            fail_other();
                                            break;
                                        }
                                        if (!buf.text_span_valid(item.text)) {
                                            fail_text();
                                            item_offset += item_stride;
                                            continue;
                                        }
                                        const char* text = buf.text_at(item.text.offset);
                                        const Font& font = get_font(item.font);
                                        draw_text_box(canvas, item.rect, text, item.color, font,
                                                      item.align_h, item.align_v, item.wrap, item.ellipsis);
                                        count_cmd(kind);
                                        item_offset += item_stride;
                                    }
                                    offset = scan_offset;
                                }
                            } else {
                                exec_group_cmd(cur, kind);
                                count_cmd(kind);
                                offset += cur_stride;
                            }
                            if (offset >= cmd_bytes) break;
                            if (!read_cmd_at_offset(offset, cmd, stride)) {
                                fail_other();
                                offset = cmd_bytes;
                                continue;
                            }
                            if (group_kind(cmd.type) != kind) break;
                        }
                        stats.batch_flushes++;
                        continue;
                    }
                    if (kind == GroupKind::DrawLine) {
                        constexpr std::size_t kMaxExecBatchItems = 64;
                        std::array<DrawCmd, kMaxExecBatchItems> line_items{};
                        while (true) {
                            DrawCmd cur = cmd;
                            std::size_t cur_stride = stride;
                            if (cur.type == CmdType::DrawLineBatch) {
                                exec_line_batch(cur);
                                count_cmd(kind);
                                offset += cur_stride;
                            } else if (cur.type == CmdType::DrawLine) {
                                std::size_t run = 1;
                                line_items[0] = cur;
                                std::size_t scan_offset = offset + cur_stride;
                                while (scan_offset < cmd_bytes && run < kMaxExecBatchItems) {
                                    DrawCmd next{};
                                    std::size_t next_stride = 0;
                                    if (!read_cmd_at_offset(scan_offset, next, next_stride)) {
                                        fail_other();
                                        break;
                                    }
                                    if (group_kind(next.type) != kind) break;
                                    if (next.type != CmdType::DrawLine) break;
                                    if (!rgba_equal(next.color, cur.color)) break;
                                    line_items[run] = next;
                                    scan_offset += next_stride;
                                    ++run;
                                }
                                for (std::size_t j = 0; j < run; ++j) {
                                    exec_draw_line(line_items[j]);
                                    count_cmd(kind);
                                }
                                offset = scan_offset;
                            } else {
                                exec_group_cmd(cur, kind);
                                count_cmd(kind);
                                offset += cur_stride;
                            }
                            if (offset >= cmd_bytes) break;
                            if (!read_cmd_at_offset(offset, cmd, stride)) {
                                fail_other();
                                offset = cmd_bytes;
                                continue;
                            }
                            if (group_kind(cmd.type) != kind) break;
                        }
                        stats.batch_flushes++;
                        continue;
                    }
                    if (kind == GroupKind::DrawPath) {
                        constexpr std::size_t kMaxExecBatchItems = 64;
                        std::array<DrawCmd, kMaxExecBatchItems> path_items{};
                        while (true) {
                            DrawCmd cur = cmd;
                            std::size_t cur_stride = stride;
                            if (cur.type == CmdType::DrawPathBatch) {
                                exec_path_batch(cur);
                                count_cmd(kind);
                                offset += cur_stride;
                            } else if (cur.type == CmdType::DrawPath) {
                                std::size_t run = 1;
                                path_items[0] = cur;
                                std::size_t scan_offset = offset + cur_stride;
                                while (scan_offset < cmd_bytes && run < kMaxExecBatchItems) {
                                    DrawCmd next{};
                                    std::size_t next_stride = 0;
                                    if (!read_cmd_at_offset(scan_offset, next, next_stride)) {
                                        fail_other();
                                        break;
                                    }
                                    if (group_kind(next.type) != kind) break;
                                    if (next.type != CmdType::DrawPath) break;
                                    if (!rgba_equal(next.color, cur.color)) break;
                                    path_items[run] = next;
                                    scan_offset += next_stride;
                                    ++run;
                                }
                                for (std::size_t j = 0; j < run; ++j) {
                                    exec_draw_path(path_items[j]);
                                    count_cmd(kind);
                                }
                                offset = scan_offset;
                            } else {
                                exec_group_cmd(cur, kind);
                                count_cmd(kind);
                                offset += cur_stride;
                            }
                            if (offset >= cmd_bytes) break;
                            if (!read_cmd_at_offset(offset, cmd, stride)) {
                                fail_other();
                                offset = cmd_bytes;
                                continue;
                            }
                            if (group_kind(cmd.type) != kind) break;
                        }
                        stats.batch_flushes++;
                        continue;
                    }
                    exec_group_cmd(cmd, kind);
                    count_cmd(kind);
                    offset += stride;
                    while (offset < cmd_bytes) {
                        DrawCmd cur{};
                        std::size_t cur_stride = 0;
                        if (!read_cmd_at_offset(offset, cur, cur_stride)) {
                            fail_other();
                            offset = cmd_bytes;
                            break;
                        }
                        if (group_kind(cur.type) != kind) break;
                        exec_group_cmd(cur, kind);
                        count_cmd(kind);
                        offset += cur_stride;
                    }
                    stats.batch_flushes++;
                    continue;
                }

                stats.dispatch_groups++;
                count_group(GroupKind::None);
                switch (cmd.type) {
                case CmdType::PushClip:
                    if (cmd.rect.w <= 0 || cmd.rect.h <= 0) {
                        stats.clip_invalid++;
                        fail_clip();
                        break;
                    }
                    if (sp < clip_stack.size()) {
                        clip_stack[sp++] = canvas.save_clip();
                        canvas.set_clip(cmd.rect);
                        stats.clip_pushes++;
                    } else {
                        stats.clip_push_overflow++;
                        fail_clip();
                    }
                    break;
                case CmdType::PopClip:
                    if (sp > 0) {
                        canvas.restore_clip(clip_stack[--sp]);
                        stats.clip_pops++;
                    } else {
                        stats.clip_pop_underflow++;
                        fail_clip();
                    }
                    break;
                }
                count_cmd(GroupKind::None);
                stats.batch_flushes++;
                offset += stride;
            }

            if (initial_clip) {
                canvas.restore_clip(base_clip);
            }
            return stats;
        }

        template <ui::RenderBackend Backend, class Buffer>
        DrawCmdTileStats execute_tiles(Backend& backend,
                                       const FrameBufferView& tile_buffer,
                                       const Buffer& buf,
                                       const DrawCmdTileConfig& config) noexcept {
            DrawCmdTileStats stats{};
            stats.cmd_count = buf.size();
            stats.cmd_bytes = buf.cmd_bytes();
            if (!tile_buffer.data) return stats;
            if (config.tile_width <= 0 || config.tile_height <= 0) return stats;

            const int screen_w = backend.width();
            const int screen_h = backend.height();
            const int buffer_w = static_cast<int>(tile_buffer.width);
            const int buffer_h = static_cast<int>(tile_buffer.height);
            if (buffer_w <= 0 || buffer_h <= 0) return stats;

            const int tile_w = (config.tile_width < buffer_w) ? config.tile_width : buffer_w;
            const int tile_h = (config.tile_height < buffer_h) ? config.tile_height : buffer_h;
            const std::size_t stride = (tile_buffer.stride_bytes != 0)
                ? tile_buffer.stride_bytes
                : static_cast<std::size_t>(buffer_w) * bytes_per_pixel(tile_buffer.format);

            RuntimeCanvas tile_canvas(tile_buffer.data,
                                      buffer_w,
                                      buffer_h,
                                      tile_buffer.format,
                                      stride);

            const int tiles_x = (screen_w + tile_w - 1) / tile_w;
            const int tiles_y = (screen_h + tile_h - 1) / tile_h;
            constexpr std::size_t kMaxTileHitEntries = 1024;
            const std::size_t tile_count = static_cast<std::size_t>(tiles_x) * static_cast<std::size_t>(tiles_y);
            std::array<std::uint8_t, kMaxTileHitEntries> tile_hits{};
            const bool use_hit_cache = (tile_count <= kMaxTileHitEntries);
            if (use_hit_cache) {
                Rect screen_rect{0, 0, screen_w, screen_h};
                Rect clipped{};
                auto mark_bounds = [&](const Rect& bounds) noexcept {
                    if (!rect_valid(bounds)) return;
                    if (!rect_intersect(bounds, screen_rect, clipped)) return;
                    int tx0 = clipped.x / tile_w;
                    int ty0 = clipped.y / tile_h;
                    int tx1 = (clipped.x + clipped.w - 1) / tile_w;
                    int ty1 = (clipped.y + clipped.h - 1) / tile_h;
                    if (tx0 < 0) tx0 = 0;
                    if (ty0 < 0) ty0 = 0;
                    if (tx1 >= tiles_x) tx1 = tiles_x - 1;
                    if (ty1 >= tiles_y) ty1 = tiles_y - 1;
                    for (int ty = ty0; ty <= ty1; ++ty) {
                        const std::size_t row = static_cast<std::size_t>(ty) * static_cast<std::size_t>(tiles_x);
                        for (int tx = tx0; tx <= tx1; ++tx) {
                            tile_hits[row + static_cast<std::size_t>(tx)] = 1;
                        }
                    }
                };
                const std::size_t cmd_bytes = buf.cmd_bytes();
                std::size_t offset = 0;
                DrawCmd cmd{};
                while (offset < cmd_bytes) {
                    std::size_t stride = 0;
                    if (!buf.read_cmd_at_offset(offset, cmd, stride)) {
                        break;
                    }
                    if (cmd.type == CmdType::PushClip || cmd.type == CmdType::PopClip) {
                        offset += stride;
                        continue;
                    }
                    if (cmd.type == CmdType::DrawLineBatch) {
                        const int count = cmd.p0;
                        if (count > 0) {
                            const auto blob = buf.blob_at(cmd.blob);
                            if (blob.size() >= static_cast<std::size_t>(count) * sizeof(LineBatchItem)) {
                                const auto items = std::span<const LineBatchItem>(
                                    reinterpret_cast<const LineBatchItem*>(blob.data()), count);
                                for (const auto& item : items) {
                                    mark_bounds(line_bounds(item.x0, item.y0, item.x1, item.y1));
                                }
                            }
                        }
                        offset += stride;
                        continue;
                    }
                    if (cmd.type == CmdType::DrawPathBatch) {
                        const int count = cmd.p0;
                        if (count > 0) {
                            const auto blob = buf.blob_at(cmd.blob);
                            if (blob.size() >= static_cast<std::size_t>(count) * sizeof(PathBatchItem)) {
                                const auto items = std::span<const PathBatchItem>(
                                    reinterpret_cast<const PathBatchItem*>(blob.data()), count);
                                for (const auto& item : items) {
                                    const auto path_blob = buf.blob_at(item.blob);
                                    if (path_blob.empty()) continue;
                                    const auto points = std::span<const Point>(
                                        reinterpret_cast<const Point*>(path_blob.data()),
                                        static_cast<std::size_t>(item.count));
                                    Rect bounds{};
                                    if (ui::gfx::path::compute_bounds(points.data(), item.count, bounds)) {
                                        mark_bounds(bounds);
                                    }
                                }
                            }
                        }
                        offset += stride;
                        continue;
                    }
                    if (cmd.type == CmdType::FocusRingBatch) {
                        const int count = cmd.p3;
                        if (count > 0) {
                            const auto blob = buf.blob_at(cmd.blob);
                            if (blob.size() >= static_cast<std::size_t>(count) * sizeof(RectBatchItem)) {
                                const auto items = std::span<const RectBatchItem>(
                                    reinterpret_cast<const RectBatchItem*>(blob.data()), count);
                                for (const auto& item : items) {
                                    mark_bounds(item.rect);
                                }
                            }
                        }
                        offset += stride;
                        continue;
                    }
                    Rect bounds = cmd.rect;
                    if (cmd.type == CmdType::DrawLine) {
                        bounds = line_bounds(cmd.rect.x, cmd.rect.y, cmd.rect.w, cmd.rect.h);
                    }
                    mark_bounds(bounds);
                    offset += stride;
                }
            }

            backend.begin_frame();
            for (int ty = 0; ty < tiles_y; ++ty) {
                const int y = ty * tile_h;
                for (int tx = 0; tx < tiles_x; ++tx) {
                    const int x = tx * tile_w;
                    const int w = ((x + tile_w) <= screen_w) ? tile_w : (screen_w - x);
                    const int h = ((y + tile_h) <= screen_h) ? tile_h : (screen_h - y);
                    if (w <= 0 || h <= 0) continue;
                    Rect tile_rect{x, y, w, h};
                    stats.tiles_total++;
                    if (use_hit_cache) {
                        const std::size_t idx = static_cast<std::size_t>(ty) * static_cast<std::size_t>(tiles_x)
                            + static_cast<std::size_t>(tx);
                        if (idx >= tile_hits.size() || tile_hits[idx] == 0) continue;
                    } else {
                        if (!buf.any_draw_hits(tile_rect)) continue;
                    }

                    if (config.clear_tile) {
                        tile_canvas.clear(config.clear_color);
                    }
                    tile_canvas.set_origin(-tile_rect.x, -tile_rect.y);
                    const auto exec_stats = execute(tile_canvas, buf, &tile_rect);
                    stats.dispatch_groups += exec_stats.dispatch_groups;
                    stats.batch_flushes += exec_stats.batch_flushes;
                    stats.failed_cmds += exec_stats.failed_cmds;
                    stats.clip_push_overflow += exec_stats.clip_push_overflow;
                    stats.clip_pop_underflow += exec_stats.clip_pop_underflow;
                    stats.clip_invalid += exec_stats.clip_invalid;
                    stats.fail_text += exec_stats.fail_text;
                    stats.fail_image += exec_stats.fail_image;
                    stats.fail_blob += exec_stats.fail_blob;
                    stats.fail_path += exec_stats.fail_path;
                    stats.fail_clip += exec_stats.fail_clip;
                    stats.fail_other += exec_stats.fail_other;
                    stats.group_rect += exec_stats.group_rect;
                    stats.group_text += exec_stats.group_text;
                    stats.group_image += exec_stats.group_image;
                    stats.group_line += exec_stats.group_line;
                    stats.group_path += exec_stats.group_path;
                    stats.group_other += exec_stats.group_other;
                    stats.cmd_rect += exec_stats.cmd_rect;
                    stats.cmd_text += exec_stats.cmd_text;
                    stats.cmd_image += exec_stats.cmd_image;
                    stats.cmd_line += exec_stats.cmd_line;
                    stats.cmd_path += exec_stats.cmd_path;
                    stats.cmd_other += exec_stats.cmd_other;
                    tile_canvas.clear_origin();

                    const std::size_t row_bytes = static_cast<std::size_t>(w)
                        * bytes_per_pixel(tile_buffer.format);
                    for (int row = 0; row < h; ++row) {
                        const std::byte* src = tile_buffer.data + static_cast<std::size_t>(row) * stride;
                        backend.blit_span(x, y + row, src, row_bytes);
                    }
                    backend.mark_dirty(x, y, w, h);
                    stats.tiles_drawn++;
                    stats.tile_flush_count++;
                }
            }
            backend.end_frame();
            return stats;
        }
    };
}
