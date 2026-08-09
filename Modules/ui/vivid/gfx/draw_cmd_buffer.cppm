module;

#include "vivid_features.generated.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <type_traits>

export module charm.gfx.draw_cmd:buffer;

import :schema;
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

namespace ui::draw_cmd::buffer_detail {
    inline constexpr std::size_t max_encoded_payload_bytes = sizeof(CmdGlyphRun);
    inline constexpr std::size_t max_encoded_cmd_stride =
        cmd_stride(sizeof(CmdHeader) + max_encoded_payload_bytes);

    static_assert(sizeof(CmdColor) <= max_encoded_payload_bytes);
    static_assert(sizeof(CmdColorP0) <= max_encoded_payload_bytes);
    static_assert(sizeof(CmdColorP0P1P2) <= max_encoded_payload_bytes);
    static_assert(sizeof(CmdColorP0P1P2P3) <= max_encoded_payload_bytes);
    static_assert(sizeof(CmdTextBox) <= max_encoded_payload_bytes);
    static_assert(sizeof(CmdPath) <= max_encoded_payload_bytes);
    static_assert(sizeof(CmdImage) <= max_encoded_payload_bytes);
    static_assert(sizeof(CmdImageP0) <= max_encoded_payload_bytes);
    static_assert(sizeof(CmdImageP0P1) <= max_encoded_payload_bytes);
    static_assert(sizeof(CmdImageP0P1P2) <= max_encoded_payload_bytes);
    static_assert(sizeof(CmdImageP0P1P2P3) <= max_encoded_payload_bytes);
    static_assert(sizeof(CmdBatchRect) <= max_encoded_payload_bytes);
    static_assert(sizeof(CmdBatchLine) <= max_encoded_payload_bytes);
    static_assert(sizeof(CmdBatchPath) <= max_encoded_payload_bytes);
    static_assert(sizeof(CmdBatchRectP0) <= max_encoded_payload_bytes);
    static_assert(sizeof(CmdBatchRectP0P1P2) <= max_encoded_payload_bytes);
    static_assert(sizeof(CmdImageBatch) <= max_encoded_payload_bytes);
    static_assert(sizeof(CmdImageBatchP0) <= max_encoded_payload_bytes);
    static_assert(sizeof(CmdImageBatchP0P1) <= max_encoded_payload_bytes);
    static_assert(sizeof(CmdImageBatchP0P1P2) <= max_encoded_payload_bytes);
    static_assert(sizeof(CmdImageBatchP0P1P2P3) <= max_encoded_payload_bytes);
    static_assert(max_encoded_cmd_stride <= CHARM_VIVID_DRAW_CMD_RECORD_UPPER_BYTES);

    template <std::size_t MaxCmds>
    inline constexpr std::size_t cmd_bytes_capacity = MaxCmds * max_encoded_cmd_stride;
}

export namespace ui::draw_cmd {
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
        [[nodiscard]] bool can_add_bytes(std::size_t len, std::size_t alignment) const noexcept {
            if (len == 0) return false;
            const std::size_t offset = align_up(used_, alignment);
            return (offset + len) <= Capacity;
        }

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
            if (!data && len != 0) {
                overflowed_ = true;
                return false;
            }
            if (len == 0) return true;
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

    struct DrawCmdCompactionWorkspace {
        static constexpr std::size_t kMaxBatchItems = 64;
        static constexpr std::size_t kBatchItemBytes = sizeof(GlyphRunItem);
        static constexpr std::size_t kBatchItemAlignment = alignof(GlyphRunItem);
        static constexpr std::size_t kBatchStorageBytes = kMaxBatchItems * kBatchItemBytes;

        static_assert(sizeof(RectBatchItem) <= kBatchItemBytes);
        static_assert(sizeof(LineBatchItem) <= kBatchItemBytes);
        static_assert(sizeof(PathBatchItem) <= kBatchItemBytes);
        static_assert(sizeof(ImageBatchItem) <= kBatchItemBytes);
        static_assert(alignof(RectBatchItem) <= kBatchItemAlignment);
        static_assert(alignof(LineBatchItem) <= kBatchItemAlignment);
        static_assert(alignof(PathBatchItem) <= kBatchItemAlignment);
        static_assert(alignof(ImageBatchItem) <= kBatchItemAlignment);

        alignas(kBatchItemAlignment) std::array<std::byte, kBatchStorageBytes> batch_item_storage{};
        DrawCmd batch_command{};

        template <typename Item>
        void write_batch_item(std::size_t index, const Item& item) noexcept {
            static_assert(std::is_trivially_copyable_v<Item>);
            static_assert(std::has_unique_object_representations_v<Item>);
            static_assert(sizeof(Item) <= kBatchItemBytes);
            static_assert(alignof(Item) <= kBatchItemAlignment);
            assert(index < kMaxBatchItems);
            std::memcpy(batch_item_storage.data() + index * sizeof(Item), &item, sizeof(Item));
        }

        template <typename Item>
        [[nodiscard]] const std::byte* batch_item_data() const noexcept {
            static_assert(std::is_trivially_copyable_v<Item>);
            static_assert(std::has_unique_object_representations_v<Item>);
            static_assert(sizeof(Item) <= kBatchItemBytes);
            return batch_item_storage.data();
        }

        [[nodiscard]] DrawCmd& reset_batch_command() noexcept {
            batch_command = DrawCmd{};
            return batch_command;
        }
    };

    template <std::size_t MaxCmds, std::size_t TextBytes, std::size_t BlobBytes>
    class DrawCmdBuffer {
    public:
        using CompactionWorkspace = DrawCmdCompactionWorkspace;
        static constexpr std::size_t kMaxCommands = MaxCmds;
        static constexpr std::size_t kTextCapacity = TextBytes;
        static constexpr std::size_t kBlobCapacity = BlobBytes;
        static constexpr std::size_t kCmdBytesCapacity =
            buffer_detail::cmd_bytes_capacity<MaxCmds>;
        static constexpr std::size_t kCompactionWorkspaceUpperBytes = 2048;
        static_assert(sizeof(CompactionWorkspace) <= kCompactionWorkspaceUpperBytes);

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
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
            current_draw_scope_ = {};
#endif
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
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
        void set_draw_scope(DrawScope scope) noexcept { current_draw_scope_ = scope; }
        [[nodiscard]] DrawScope draw_scope() const noexcept { return current_draw_scope_; }
#else
        void set_draw_scope(DrawScope) noexcept {}
        [[nodiscard]] DrawScope draw_scope() const noexcept { return {}; }
#endif
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
        [[nodiscard]] bool read_cmd_at_offset(std::size_t offset,
                                              DrawCmd& out,
                                              std::size_t& stride) const noexcept {
            if (offset + sizeof(CmdHeader) > cmd_bytes_used_) return false;
            const auto* header = reinterpret_cast<const CmdHeader*>(cmd_bytes_.data() + offset);
            if (header->size < sizeof(CmdHeader)) return false;
            stride = cmd_stride(header->size);
            if (stride == 0 || stride > buffer_detail::max_encoded_cmd_stride
                || offset + stride > cmd_bytes_used_) return false;
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
                return false;
            }
            if (cmd_count > kMaxCommands) {
                cmd_overflowed_ = true;
                return false;
            }
            if (cmd_bytes_len == 0 && cmd_count != 0) {
                cmd_overflowed_ = true;
                return false;
            }
            if (cmd_bytes_len > 0) {
                std::memcpy(cmd_bytes_.data(), cmd_bytes, cmd_bytes_len);
            }
            cmd_bytes_used_ = cmd_bytes_len;
            count_ = cmd_count;
            if (cmd_bytes_used_ > 0) {
                if (!validate_cmd_stream(count_)) {
                    discard_loaded_stream();
                    cmd_overflowed_ = true;
                    return false;
                }
            } else {
                count_ = 0;
            }
            if (!text && text_bytes != 0) {
                discard_loaded_stream();
                text_overflowed_ = true;
                return false;
            }
            if (text_bytes > kTextCapacity) {
                discard_loaded_stream();
                text_overflowed_ = true;
                return false;
            }
            if (text_bytes == 0) {
                text_[0] = '\0';
                text_used_ = 1;
            } else {
                std::memcpy(text_.data(), text, text_bytes);
                text_used_ = text_bytes;
            }
            if (!blob_.load(blob, blob_bytes)) {
                discard_loaded_stream();
                return false;
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
            if (color.a == 0) return true;
            auto cmd = make_cmd(CmdType::FillRect, rect);
            cmd.color = color;
            return push_cmd(cmd);
        }

        bool fill_linear_gradient_rect(const Rect& rect,
                                       const rgba& start,
                                       const rgba& end,
                                       int radius,
                                       bool vertical) noexcept {
            if (start.a == 0 && end.a == 0) return true;
            auto cmd = make_cmd(CmdType::FillLinearGradientRect, rect);
            cmd.color = start;
            cmd.p0 = static_cast<std::int16_t>(radius);
            cmd.p1 = static_cast<std::int16_t>(vertical ? 1 : 0);
            cmd.p2 = static_cast<std::int16_t>((static_cast<std::uint16_t>(end.r) << 8) | end.g);
            cmd.p3 = static_cast<std::int16_t>((static_cast<std::uint16_t>(end.b) << 8) | end.a);
            return push_cmd(cmd);
        }

        bool stroke_rect(const Rect& rect, const rgba& color) noexcept {
            if (color.a == 0) return true;
            auto cmd = make_cmd(CmdType::StrokeRect, rect);
            cmd.color = color;
            return push_cmd(cmd);
        }

        bool fill_round_rect(const Rect& rect, int radius, const rgba& color) noexcept {
            if (color.a == 0) return true;
            auto cmd = make_cmd(CmdType::FillRoundRect, rect);
            cmd.color = color;
            cmd.p0 = static_cast<std::int16_t>(radius);
            return push_cmd(cmd);
        }

        bool stroke_round_rect(const Rect& rect, int radius, const rgba& color) noexcept {
            if (color.a == 0) return true;
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
            return draw_image_shaped(rect, image, corner_radius, ui::render::ImageShapeKind::RoundRect);
        }
        bool draw_image_shaped(const Rect& rect,
                               ImageId image,
                               int extent,
                               ui::render::ImageShapeKind shape,
                               int rotation_deg = 0) noexcept {
            if (!image_id_valid(image)) return false;
            auto cmd = make_cmd(CmdType::DrawImageRoundRect, rect);
            cmd.image = image;
            cmd.p0 = static_cast<std::int16_t>(extent);
            cmd.p1 = static_cast<std::int16_t>(shape);
            cmd.p2 = static_cast<std::int16_t>(rotation_deg);
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
            cmd.font_ptr = &font;
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
                if (!read_cmd_at_offset(offset, cmd, stride)) return true;
                if (cmd.type == CmdType::PushClip || cmd.type == CmdType::PopClip) {
                    offset += stride;
                    continue;
                }
                if (cmd.type == CmdType::DrawLineBatch) {
                    const int count = cmd.p0;
                    if (count <= 0) return true;
                    const auto blob = blob_.bytes(cmd.blob);
                    if (blob.size() < static_cast<std::size_t>(count) * sizeof(LineBatchItem)) {
                        return true;
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
                const Rect bounds = draw_cmd_bounds(cmd);
                if (!rect_valid(bounds)) {
                    offset += stride;
                    continue;
                }
                if (rect_intersect(bounds, rect, out)) return true;
                offset += stride;
            }
            return false;
        }

        bool compact(CompactionWorkspace& workspace) noexcept {
            if (cmd_bytes_used_ == 0) return true;
            const std::size_t input_bytes = cmd_bytes_used_;
            std::size_t out_bytes = 0;
            std::size_t out_count = 0;
            std::size_t offset = 0;
            bool ok = true;
            constexpr std::size_t kMaxBatchItems = CompactionWorkspace::kMaxBatchItems;
            bool allow_batch = !blob_.overflowed();
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
                if (stride == 0 || stride > buffer_detail::max_encoded_cmd_stride
                    || at + stride > input_bytes) return false;
                return decode_cmd(header, out_cmd);
            };

            auto emit_cmd = [&](const DrawCmd& out_cmd) noexcept -> bool {
                if (!encode_cmd(out_cmd,
                                cmd_bytes_.data(),
                                kCmdBytesCapacity,
                                out_bytes,
                                out_count)) {
                    cmd_overflowed_ = true;
                    return false;
                }
                return true;
            };

            auto can_merge_text = [](const DrawCmd& a, const DrawCmd& b) noexcept {
                return rgba_equal(a.color, b.color)
                    && draw_cmd_scope_equal(a, b)
                    && a.font_ptr == b.font_ptr
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
                        if (!draw_cmd_scope_equal(cmd, next)) break;
                        if (!rgba_equal(next.color, cmd.color)) break;
                        scan_offset += next_stride;
                        ++run;
                    }
                    if (run >= 2) {
                        bool merged = false;
                        std::size_t batch = (run > kMaxBatchItems) ? kMaxBatchItems : run;
                        while (batch >= 2) {
                            Rect bounds{};
                            std::int64_t sum_area = 0;
                            std::size_t item_offset = offset;
                            for (std::size_t j = 0; j < batch; ++j) {
                                DrawCmd next{};
                                std::size_t item_stride = 0;
                                (void)read_cmd_at_offset(item_offset, next, item_stride);
                                const LineBatchItem item{
                                    next.rect.x, next.rect.y, next.rect.w, next.rect.h};
                                workspace.write_batch_item(j, item);
                                const Rect line_rect = line_bounds(
                                    item.x0, item.y0, item.x1, item.y1);
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
                            const std::size_t blob_bytes = batch * sizeof(LineBatchItem);
                            if (!blob_.can_add_bytes(blob_bytes, alignof(LineBatchItem))) {
                                --batch;
                                continue;
                            }
                            const BlobRef blob = blob_.add_bytes(
                                workspace.template batch_item_data<LineBatchItem>(),
                                blob_bytes,
                                alignof(LineBatchItem));
                            if (blob.length != 0) {
                                auto& batch_cmd = workspace.reset_batch_command();
                                batch_cmd.type = CmdType::DrawLineBatch;
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                                batch_cmd.draw_scope = cmd.draw_scope;
#endif
                                batch_cmd.rect = bounds;
                                batch_cmd.color = cmd.color;
                                batch_cmd.blob = blob;
                                batch_cmd.p0 = static_cast<std::int16_t>(batch);
                                if (!emit_cmd(batch_cmd)) ok = false;
                                offset = item_offset;
                                merged = true;
                                break;
                            }
                            ok = false;
                            allow_batch = false;
                            break;
                        }
                        if (merged) {
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
                        if (!draw_cmd_scope_equal(cmd, next)) break;
                        if (!rgba_equal(next.color, cmd.color)) break;
                        scan_offset += next_stride;
                        ++run;
                    }
                    if (run >= 2) {
                        bool merged = false;
                        std::size_t batch = (run > kMaxBatchItems) ? kMaxBatchItems : run;
                        while (batch >= 2) {
                            Rect bounds = cmd.rect;
                            std::int64_t sum_area = 0;
                            std::size_t item_offset = offset;
                            for (std::size_t j = 0; j < batch; ++j) {
                                DrawCmd next{};
                                std::size_t item_stride = 0;
                                (void)read_cmd_at_offset(item_offset, next, item_stride);
                                workspace.write_batch_item(
                                    j,
                                    PathBatchItem{next.blob, next.p0, next.p1});
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
                            const std::size_t blob_bytes = batch * sizeof(PathBatchItem);
                            if (!blob_.can_add_bytes(blob_bytes, alignof(PathBatchItem))) {
                                --batch;
                                continue;
                            }
                            const BlobRef blob = blob_.add_bytes(
                                workspace.template batch_item_data<PathBatchItem>(),
                                blob_bytes,
                                alignof(PathBatchItem));
                            if (blob.length != 0) {
                                auto& batch_cmd = workspace.reset_batch_command();
                                batch_cmd.type = CmdType::DrawPathBatch;
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                                batch_cmd.draw_scope = cmd.draw_scope;
#endif
                                batch_cmd.rect = bounds;
                                batch_cmd.color = cmd.color;
                                batch_cmd.blob = blob;
                                batch_cmd.p0 = static_cast<std::int16_t>(batch);
                                if (!emit_cmd(batch_cmd)) ok = false;
                                offset = item_offset;
                                merged = true;
                                break;
                            }
                            ok = false;
                            allow_batch = false;
                            break;
                        }
                        if (merged) {
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
                        if (!draw_cmd_scope_equal(cmd, next)) break;
                        scan_offset += next_stride;
                        ++run;
                    }
                    if (run >= 2) {
                        bool merged = false;
                        std::size_t batch = (run > kMaxBatchItems) ? kMaxBatchItems : run;
                        while (batch >= 2) {
                            Rect bounds{};
                            std::int64_t sum_area = 0;
                            std::size_t item_offset = offset;
                            for (std::size_t j = 0; j < batch; ++j) {
                                DrawCmd next{};
                                std::size_t item_stride = 0;
                                (void)read_cmd_at_offset(item_offset, next, item_stride);
                                workspace.write_batch_item(j, RectBatchItem{next.rect});
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
                                ++batch_shrink_rect_;
                                --batch;
                                continue;
                            }
                            const std::size_t blob_bytes = batch * sizeof(RectBatchItem);
                            if (!blob_.can_add_bytes(blob_bytes, alignof(RectBatchItem))) {
                                --batch;
                                continue;
                            }
                            const BlobRef blob = blob_.add_bytes(
                                workspace.template batch_item_data<RectBatchItem>(),
                                blob_bytes,
                                alignof(RectBatchItem));
                            if (blob.length != 0) {
                                auto& batch_cmd = workspace.reset_batch_command();
                                batch_cmd.type = (cmd.type == CmdType::StrokeRect)
                                    ? CmdType::StrokeRectBatch
                                    : CmdType::FillRectBatch;
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                                batch_cmd.draw_scope = cmd.draw_scope;
#endif
                                batch_cmd.rect = bounds;
                                batch_cmd.color = cmd.color;
                                batch_cmd.blob = blob;
                                batch_cmd.p0 = static_cast<std::int16_t>(batch);
                                if (!emit_cmd(batch_cmd)) ok = false;
                                offset = item_offset;
                                merged = true;
                                break;
                            }
                            ok = false;
                            allow_batch = false;
                            break;
                        }
                        if (merged) {
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
                        if (!draw_cmd_scope_equal(cmd, next)) break;
                        if (!rgba_equal(next.color, cmd.color)) break;
                        if (next.p0 != cmd.p0) break;
                        scan_offset += next_stride;
                        ++run;
                    }
                    if (run >= 2) {
                        bool merged = false;
                        std::size_t batch = (run > kMaxBatchItems) ? kMaxBatchItems : run;
                        while (batch >= 2) {
                            Rect bounds{};
                            std::int64_t sum_area = 0;
                            std::size_t item_offset = offset;
                            for (std::size_t j = 0; j < batch; ++j) {
                                DrawCmd next{};
                                std::size_t item_stride = 0;
                                (void)read_cmd_at_offset(item_offset, next, item_stride);
                                workspace.write_batch_item(j, RectBatchItem{next.rect});
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
                                ++batch_shrink_round_;
                                --batch;
                                continue;
                            }
                            const std::size_t blob_bytes = batch * sizeof(RectBatchItem);
                            if (!blob_.can_add_bytes(blob_bytes, alignof(RectBatchItem))) {
                                --batch;
                                continue;
                            }
                            const BlobRef blob = blob_.add_bytes(
                                workspace.template batch_item_data<RectBatchItem>(),
                                blob_bytes,
                                alignof(RectBatchItem));
                            if (blob.length != 0) {
                                auto& batch_cmd = workspace.reset_batch_command();
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
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                                batch_cmd.draw_scope = cmd.draw_scope;
#endif
                                batch_cmd.rect = bounds;
                                batch_cmd.color = cmd.color;
                                batch_cmd.blob = blob;
                                batch_cmd.p0 = cmd.p0;
                                batch_cmd.p1 = static_cast<std::int16_t>(batch);
                                if (!emit_cmd(batch_cmd)) ok = false;
                                offset = item_offset;
                                merged = true;
                                break;
                            }
                            ok = false;
                            allow_batch = false;
                            break;
                        }
                        if (merged) {
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
                        if (!draw_cmd_scope_equal(cmd, next)) break;
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
                            workspace.write_batch_item(
                                j,
                                GlyphRunItem{next.rect, next.text});
                            if (j == 0) {
                                bounds = next.rect;
                            } else {
                                bounds = rect_union(bounds, next.rect);
                            }
                            item_offset += item_stride;
                        }
                        const std::size_t blob_bytes = batch * sizeof(GlyphRunItem);
                        if (blob_.can_add_bytes(blob_bytes, alignof(GlyphRunItem))) {
                            const BlobRef blob = blob_.add_bytes(
                                workspace.template batch_item_data<GlyphRunItem>(),
                                blob_bytes,
                                alignof(GlyphRunItem));
                            if (blob.length != 0) {
                                auto& batch_cmd = workspace.reset_batch_command();
                                batch_cmd.type = CmdType::GlyphRun;
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                                batch_cmd.draw_scope = cmd.draw_scope;
#endif
                                batch_cmd.rect = bounds;
                                batch_cmd.color = cmd.color;
                                batch_cmd.font_ptr = cmd.font_ptr;
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
                            allow_batch = false;
                        } else {
                            allow_batch = false;
                        }
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
                        if (!draw_cmd_scope_equal(cmd, next)) break;
                        if (next.image != cmd.image) break;
                        scan_offset += next_stride;
                        ++run;
                    }
                    if (run >= 2) {
                        bool merged = false;
                        std::size_t batch = (run > kMaxBatchItems) ? kMaxBatchItems : run;
                        while (batch >= 2) {
                            Rect bounds{};
                            std::int64_t sum_area = 0;
                            std::size_t item_offset = offset;
                            for (std::size_t j = 0; j < batch; ++j) {
                                DrawCmd next{};
                                std::size_t item_stride = 0;
                                (void)read_cmd_at_offset(item_offset, next, item_stride);
                                workspace.write_batch_item(j, ImageBatchItem{next.rect});
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
                                ++batch_shrink_image_;
                                --batch;
                                continue;
                            }
                            const std::size_t blob_bytes = batch * sizeof(ImageBatchItem);
                            if (!blob_.can_add_bytes(blob_bytes, alignof(ImageBatchItem))) {
                                --batch;
                                continue;
                            }
                            const BlobRef blob = blob_.add_bytes(
                                workspace.template batch_item_data<ImageBatchItem>(),
                                blob_bytes,
                                alignof(ImageBatchItem));
                            if (blob.length != 0) {
                                auto& batch_cmd = workspace.reset_batch_command();
                                batch_cmd.type = CmdType::DrawImageBatch;
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                                batch_cmd.draw_scope = cmd.draw_scope;
#endif
                                batch_cmd.rect = bounds;
                                batch_cmd.image = cmd.image;
                                batch_cmd.blob = blob;
                                batch_cmd.p0 = static_cast<std::int16_t>(batch);
                                if (!emit_cmd(batch_cmd)) ok = false;
                                offset = item_offset;
                                merged = true;
                                break;
                            }
                            ok = false;
                            allow_batch = false;
                            break;
                        }
                        if (merged) {
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
                        if (!draw_cmd_scope_equal(cmd, next)) break;
                        if (next.image != cmd.image) break;
                        if (next.p0 != cmd.p0) break;
                        if (next.p1 != cmd.p1) break;
                        if (next.p2 != cmd.p2) break;
                        scan_offset += next_stride;
                        ++run;
                    }
                    if (run >= 2) {
                        bool merged = false;
                        std::size_t batch = (run > kMaxBatchItems) ? kMaxBatchItems : run;
                        while (batch >= 2) {
                            Rect bounds{};
                            std::int64_t sum_area = 0;
                            std::size_t item_offset = offset;
                            for (std::size_t j = 0; j < batch; ++j) {
                                DrawCmd next{};
                                std::size_t item_stride = 0;
                                (void)read_cmd_at_offset(item_offset, next, item_stride);
                                workspace.write_batch_item(j, ImageBatchItem{next.rect});
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
                                ++batch_shrink_image_;
                                --batch;
                                continue;
                            }
                            const std::size_t blob_bytes = batch * sizeof(ImageBatchItem);
                            if (!blob_.can_add_bytes(blob_bytes, alignof(ImageBatchItem))) {
                                --batch;
                                continue;
                            }
                            const BlobRef blob = blob_.add_bytes(
                                workspace.template batch_item_data<ImageBatchItem>(),
                                blob_bytes,
                                alignof(ImageBatchItem));
                            if (blob.length != 0) {
                                auto& batch_cmd = workspace.reset_batch_command();
                                batch_cmd.type = CmdType::DrawImageRoundRectBatch;
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                                batch_cmd.draw_scope = cmd.draw_scope;
#endif
                                batch_cmd.rect = bounds;
                                batch_cmd.image = cmd.image;
                                batch_cmd.blob = blob;
                                batch_cmd.p0 = cmd.p0;
                                batch_cmd.p1 = cmd.p1;
                                batch_cmd.p2 = cmd.p2;
                                batch_cmd.p3 = static_cast<std::int16_t>(batch);
                                if (!emit_cmd(batch_cmd)) ok = false;
                                offset = item_offset;
                                merged = true;
                                break;
                            }
                            ok = false;
                            allow_batch = false;
                            break;
                        }
                        if (merged) {
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
                        if (!draw_cmd_scope_equal(cmd, next)) break;
                        if (next.image != cmd.image) break;
                        if (next.p0 != cmd.p0 || next.p1 != cmd.p1
                            || next.p2 != cmd.p2 || next.p3 != cmd.p3) break;
                        scan_offset += next_stride;
                        ++run;
                    }
                    if (run >= 2) {
                        bool merged = false;
                        std::size_t batch = (run > kMaxBatchItems) ? kMaxBatchItems : run;
                        while (batch >= 2) {
                            Rect bounds{};
                            std::int64_t sum_area = 0;
                            std::size_t item_offset = offset;
                            for (std::size_t j = 0; j < batch; ++j) {
                                DrawCmd next{};
                                std::size_t item_stride = 0;
                                (void)read_cmd_at_offset(item_offset, next, item_stride);
                                workspace.write_batch_item(j, ImageBatchItem{next.rect});
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
                                ++batch_shrink_image_;
                                --batch;
                                continue;
                            }
                            const std::size_t blob_bytes = batch * sizeof(ImageBatchItem);
                            if (!blob_.can_add_bytes(blob_bytes, alignof(ImageBatchItem))) {
                                --batch;
                                continue;
                            }
                            const BlobRef blob = blob_.add_bytes(
                                workspace.template batch_item_data<ImageBatchItem>(),
                                blob_bytes,
                                alignof(ImageBatchItem));
                            if (blob.length != 0) {
                                auto& batch_cmd = workspace.reset_batch_command();
                                batch_cmd.type = CmdType::DrawImageNineSliceBatch;
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                                batch_cmd.draw_scope = cmd.draw_scope;
#endif
                                batch_cmd.rect = bounds;
                                batch_cmd.image = cmd.image;
                                batch_cmd.blob = blob;
                                batch_cmd.p0 = cmd.p0;
                                batch_cmd.p1 = cmd.p1;
                                batch_cmd.p2 = cmd.p2;
                                batch_cmd.p3 = cmd.p3;
                                if (!emit_cmd(batch_cmd)) ok = false;
                                offset = item_offset;
                                merged = true;
                                break;
                            }
                            ok = false;
                            allow_batch = false;
                            break;
                        }
                        if (merged) {
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
                        if (!draw_cmd_scope_equal(cmd, next)) break;
                        if (!rgba_equal(next.color, cmd.color)) break;
                        if (next.p0 != cmd.p0 || next.p1 != cmd.p1 || next.p2 != cmd.p2) break;
                        scan_offset += next_stride;
                        ++run;
                    }
                    if (run >= 2) {
                        bool merged = false;
                        std::size_t batch = (run > kMaxBatchItems) ? kMaxBatchItems : run;
                        while (batch >= 2) {
                            Rect bounds{};
                            std::int64_t sum_area = 0;
                            std::size_t item_offset = offset;
                            for (std::size_t j = 0; j < batch; ++j) {
                                DrawCmd next{};
                                std::size_t item_stride = 0;
                                (void)read_cmd_at_offset(item_offset, next, item_stride);
                                workspace.write_batch_item(j, RectBatchItem{next.rect});
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
                                ++batch_shrink_focus_;
                                --batch;
                                continue;
                            }
                            const std::size_t blob_bytes = batch * sizeof(RectBatchItem);
                            if (!blob_.can_add_bytes(blob_bytes, alignof(RectBatchItem))) {
                                --batch;
                                continue;
                            }
                            const BlobRef blob = blob_.add_bytes(
                                workspace.template batch_item_data<RectBatchItem>(),
                                blob_bytes,
                                alignof(RectBatchItem));
                            if (blob.length != 0) {
                                auto& batch_cmd = workspace.reset_batch_command();
                                batch_cmd.type = CmdType::FocusRingBatch;
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                                batch_cmd.draw_scope = cmd.draw_scope;
#endif
                                batch_cmd.rect = bounds;
                                batch_cmd.color = cmd.color;
                                batch_cmd.blob = blob;
                                batch_cmd.p0 = cmd.p0;
                                batch_cmd.p1 = cmd.p1;
                                batch_cmd.p2 = cmd.p2;
                                batch_cmd.p3 = static_cast<std::int16_t>(batch);
                                if (!emit_cmd(batch_cmd)) ok = false;
                                offset = item_offset;
                                merged = true;
                                break;
                            }
                            ok = false;
                            allow_batch = false;
                            break;
                        }
                        if (merged) {
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
            } else {
                count_ = out_count;
                cmd_bytes_used_ = out_bytes;
            }
            return ok && !blob_.overflowed();
        }

    private:
        DrawCmd make_cmd(CmdType type, const Rect& rect) noexcept {
            DrawCmd cmd{};
            cmd.type = type;
            cmd.rect = rect;
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
            cmd.draw_scope = current_draw_scope_;
#endif
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
                            count_)) {
                cmd_overflowed_ = true;
                return false;
            }
            return true;
        }

        void discard_loaded_stream() noexcept {
            count_ = 0;
            cmd_bytes_used_ = 0;
            text_[0] = '\0';
            text_used_ = 1;
        }

        bool validate_cmd_stream(std::size_t expected_count) noexcept {
            std::size_t offset = 0;
            std::size_t actual_count = 0;
            DrawCmd decoded{};
            while (offset < cmd_bytes_used_) {
                if (offset + sizeof(CmdHeader) > cmd_bytes_used_) return false;
                if (actual_count >= kMaxCommands) return false;
                const auto* header = reinterpret_cast<const CmdHeader*>(cmd_bytes_.data() + offset);
                if (header->size < sizeof(CmdHeader)) return false;
                const std::size_t stride = cmd_stride(header->size);
                if (stride == 0 || stride > buffer_detail::max_encoded_cmd_stride) return false;
                if (offset + stride > cmd_bytes_used_) return false;
                if (!decode_cmd(header, decoded)) return false;
                offset += stride;
                ++actual_count;
            }
            if (expected_count != 0 && actual_count != expected_count) return false;
            count_ = actual_count;
            return true;
        }

        static bool write_cmd(std::byte* base,
                              std::size_t capacity,
                              std::size_t& out_bytes,
                              std::size_t& out_count,
                              CmdType type,
                              const Rect& rect,
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                              DrawScope draw_scope,
#endif
                              const void* payload,
                              std::size_t payload_size) noexcept {
            if (out_count >= kMaxCommands) return false;
            const std::size_t cmd_size = sizeof(CmdHeader) + payload_size;
            if (cmd_size > std::numeric_limits<std::uint16_t>::max()) return false;
            const std::size_t stride = cmd_stride(cmd_size);
            if (stride > buffer_detail::max_encoded_cmd_stride) return false;
            if ((out_bytes + stride) > capacity) return false;
            CmdHeader header{};
            header.type = type;
            header.size = static_cast<std::uint16_t>(cmd_size);
            header.rect = rect;
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
            header.draw_scope = draw_scope.id;
#endif
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
                               std::size_t& out_count) noexcept {
            switch (cmd.type) {
            case CmdType::PushClip:
            case CmdType::PopClip:
                return write_cmd(base, capacity, out_bytes, out_count,
                                 cmd.type, cmd.rect,
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                                 cmd.draw_scope,
#endif
                                 nullptr, 0);
            case CmdType::DrawLine: {
                CmdColor payload{cmd.color};
                return write_cmd(base, capacity, out_bytes, out_count,
                                 cmd.type, cmd.rect,
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                                 cmd.draw_scope,
#endif
                                 &payload, sizeof(payload));
            }
            case CmdType::DrawPath: {
                CmdPath payload{};
                payload.color = cmd.color;
                payload.blob = cmd.blob;
                payload.count = cmd.p0;
                payload.closed = cmd.p1;
                return write_cmd(base, capacity, out_bytes, out_count,
                                 cmd.type, cmd.rect,
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                                 cmd.draw_scope,
#endif
                                 &payload, sizeof(payload));
            }
            case CmdType::FillRect:
            case CmdType::StrokeRect: {
                CmdColor payload{cmd.color};
                return write_cmd(base, capacity, out_bytes, out_count,
                                 cmd.type, cmd.rect,
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                                 cmd.draw_scope,
#endif
                                 &payload, sizeof(payload));
            }
            case CmdType::FillLinearGradientRect: {
                CmdColorP0P1P2P3 payload{};
                payload.color = cmd.color;
                payload.p0 = cmd.p0;
                payload.p1 = cmd.p1;
                payload.p2 = cmd.p2;
                payload.p3 = cmd.p3;
                return write_cmd(base, capacity, out_bytes, out_count,
                                 cmd.type, cmd.rect,
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                                 cmd.draw_scope,
#endif
                                 &payload, sizeof(payload));
            }
            case CmdType::FillRoundRect:
            case CmdType::StrokeRoundRect:
            case CmdType::FillCircle:
            case CmdType::StrokeCircle: {
                CmdColorP0 payload{};
                payload.color = cmd.color;
                payload.p0 = cmd.p0;
                return write_cmd(base, capacity, out_bytes, out_count,
                                 cmd.type, cmd.rect,
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                                 cmd.draw_scope,
#endif
                                 &payload, sizeof(payload));
            }
            case CmdType::DrawImage: {
                CmdImage payload{cmd.image};
                return write_cmd(base, capacity, out_bytes, out_count,
                                 cmd.type, cmd.rect,
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                                 cmd.draw_scope,
#endif
                                 &payload, sizeof(payload));
            }
            case CmdType::DrawImageRoundRect: {
                CmdImageP0P1P2 payload{};
                payload.image = cmd.image;
                payload.p0 = cmd.p0;
                payload.p1 = cmd.p1;
                payload.p2 = cmd.p2;
                return write_cmd(base, capacity, out_bytes, out_count,
                                 cmd.type, cmd.rect,
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                                 cmd.draw_scope,
#endif
                                 &payload, sizeof(payload));
            }
            case CmdType::DrawImageNineSlice: {
                CmdImageP0P1P2P3 payload{};
                payload.image = cmd.image;
                payload.p0 = cmd.p0;
                payload.p1 = cmd.p1;
                payload.p2 = cmd.p2;
                payload.p3 = cmd.p3;
                return write_cmd(base, capacity, out_bytes, out_count,
                                 cmd.type, cmd.rect,
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                                 cmd.draw_scope,
#endif
                                 &payload, sizeof(payload));
            }
            case CmdType::DrawTextBox: {
                CmdTextBox payload{};
                payload.color = cmd.color;
                payload.text = cmd.text;
                payload.font_ptr = cmd.font_ptr;
                payload.font = cmd.font;
                payload.align_h = cmd.align_h;
                payload.align_v = cmd.align_v;
                payload.wrap = cmd.wrap;
                payload.ellipsis = cmd.ellipsis;
                return write_cmd(base, capacity, out_bytes, out_count,
                                 cmd.type, cmd.rect,
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                                 cmd.draw_scope,
#endif
                                 &payload, sizeof(payload));
            }
            case CmdType::FocusRing: {
                CmdColorP0P1P2 payload{};
                payload.color = cmd.color;
                payload.p0 = cmd.p0;
                payload.p1 = cmd.p1;
                payload.p2 = cmd.p2;
                return write_cmd(base, capacity, out_bytes, out_count,
                                 cmd.type, cmd.rect,
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                                 cmd.draw_scope,
#endif
                                 &payload, sizeof(payload));
            }
            case CmdType::FillRectBatch:
            case CmdType::StrokeRectBatch: {
                CmdBatchRect payload{};
                payload.color = cmd.color;
                payload.blob = cmd.blob;
                payload.count = cmd.p0;
                return write_cmd(base, capacity, out_bytes, out_count,
                                 cmd.type, cmd.rect,
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                                 cmd.draw_scope,
#endif
                                 &payload, sizeof(payload));
            }
            case CmdType::DrawLineBatch: {
                CmdBatchLine payload{};
                payload.color = cmd.color;
                payload.blob = cmd.blob;
                payload.count = cmd.p0;
                return write_cmd(base, capacity, out_bytes, out_count,
                                 cmd.type, cmd.rect,
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                                 cmd.draw_scope,
#endif
                                 &payload, sizeof(payload));
            }
            case CmdType::DrawPathBatch: {
                CmdBatchPath payload{};
                payload.color = cmd.color;
                payload.blob = cmd.blob;
                payload.count = cmd.p0;
                return write_cmd(base, capacity, out_bytes, out_count,
                                 cmd.type, cmd.rect,
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                                 cmd.draw_scope,
#endif
                                 &payload, sizeof(payload));
            }
            case CmdType::FocusRingBatch: {
                CmdBatchRectP0P1P2 payload{};
                payload.color = cmd.color;
                payload.blob = cmd.blob;
                payload.p0 = cmd.p0;
                payload.p1 = cmd.p1;
                payload.p2 = cmd.p2;
                payload.count = cmd.p3;
                return write_cmd(base, capacity, out_bytes, out_count,
                                 cmd.type, cmd.rect,
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                                 cmd.draw_scope,
#endif
                                 &payload, sizeof(payload));
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
                return write_cmd(base, capacity, out_bytes, out_count,
                                 cmd.type, cmd.rect,
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                                 cmd.draw_scope,
#endif
                                 &payload, sizeof(payload));
            }
            case CmdType::GlyphRun: {
                CmdGlyphRun payload{};
                payload.color = cmd.color;
                payload.blob = cmd.blob;
                payload.font_ptr = cmd.font_ptr;
                payload.font = cmd.font;
                payload.align_h = cmd.align_h;
                payload.align_v = cmd.align_v;
                payload.wrap = cmd.wrap;
                payload.ellipsis = cmd.ellipsis;
                payload.count = static_cast<std::uint16_t>(cmd.p0);
                return write_cmd(base, capacity, out_bytes, out_count,
                                 cmd.type, cmd.rect,
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                                 cmd.draw_scope,
#endif
                                 &payload, sizeof(payload));
            }
            case CmdType::DrawImageBatch: {
                CmdImageBatch payload{};
                payload.image = cmd.image;
                payload.blob = cmd.blob;
                payload.count = cmd.p0;
                return write_cmd(base, capacity, out_bytes, out_count,
                                 cmd.type, cmd.rect,
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                                 cmd.draw_scope,
#endif
                                 &payload, sizeof(payload));
            }
            case CmdType::DrawImageRoundRectBatch: {
                CmdImageBatchP0P1P2 payload{};
                payload.image = cmd.image;
                payload.blob = cmd.blob;
                payload.p0 = cmd.p0;
                payload.p1 = cmd.p1;
                payload.p2 = cmd.p2;
                payload.count = cmd.p3;
                return write_cmd(base, capacity, out_bytes, out_count,
                                 cmd.type, cmd.rect,
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                                 cmd.draw_scope,
#endif
                                 &payload, sizeof(payload));
            }
            case CmdType::DrawImageNineSliceBatch: {
                CmdImageBatchP0P1P2P3 payload{};
                payload.image = cmd.image;
                payload.blob = cmd.blob;
                payload.p0 = cmd.p0;
                payload.p1 = cmd.p1;
                payload.p2 = cmd.p2;
                payload.p3 = cmd.p3;
                return write_cmd(base, capacity, out_bytes, out_count,
                                 cmd.type, cmd.rect,
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                                 cmd.draw_scope,
#endif
                                 &payload, sizeof(payload));
            }
            default:
                return false;
            }
        }

        alignas(kCmdAlign) std::array<std::byte, kCmdBytesCapacity> cmd_bytes_{};
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
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
        DrawScope current_draw_scope_{};
#endif
    };

    constexpr std::size_t kDefaultCmdCapacity = CHARM_VIVID_DRAW_CMD_MAX_COMMANDS;
    constexpr std::size_t kDefaultTextCapacity = CHARM_VIVID_DRAW_CMD_TEXT_BYTES;
    constexpr std::size_t kDefaultBlobCapacity = CHARM_VIVID_DRAW_CMD_BLOB_BYTES;
    using DefaultDrawCmdBuffer = DrawCmdBuffer<kDefaultCmdCapacity, kDefaultTextCapacity, kDefaultBlobCapacity>;
    using DefaultDrawCmdCompactionWorkspace = DefaultDrawCmdBuffer::CompactionWorkspace;
    static_assert(sizeof(DefaultDrawCmdBuffer) <= CHARM_VIVID_DRAW_CMD_BUFFER_UPPER_BYTES,
                  "DrawCmd buffer exceeds its configure-time upper bound");
}

