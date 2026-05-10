module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

export module charm.ui.scene.layer_support;

import charm.core.config;
import charm.core.geometry;
import charm.ui.scene.layer_runtime;
import charm.gfx.canvas;
import charm.gfx.color;
import charm.gfx.draw_cmd;
import charm.gfx.pixel_format;

export namespace ui::scene {
    struct CmdStats {
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

    struct ExecStats {
        std::size_t cmd_count{0};
        std::size_t cmd_bytes{0};
        std::uint64_t alpha_blend_count{0};
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

    struct LayerReplayResult {
        LayerReplayStatus status{LayerReplayStatus::InvalidPlan};
        SnapshotHandle source{};
        SnapshotKind kind{SnapshotKind::EmptyFallback};
        Rect target_bounds{};
        ExecStats stats{};

        [[nodiscard]] constexpr bool ok() const noexcept {
            return status == LayerReplayStatus::Ok;
        }
    };

    struct LayerCaptureResult {
        LayerCaptureStatus status{LayerCaptureStatus::NoSnapshotSlot};
        SnapshotHandle handle{};

        [[nodiscard]] constexpr bool ok() const noexcept {
            return status == LayerCaptureStatus::Ok;
        }
    };

    template<std::size_t MaxSlots>
    class PixelSnapshotPayloadStore {
    public:
        [[nodiscard]] std::uint32_t store(CanvasBase& source, Rect bounds) noexcept {
            if (bounds.w <= 0 || bounds.h <= 0) return kInvalidSnapshotPayloadSlot;
            if (bounds.w > layer_cache_width || bounds.h > layer_cache_height) {
                return kInvalidSnapshotPayloadSlot;
            }
            if (bounds.x < 0 || bounds.y < 0 ||
                bounds.x + bounds.w > source.width() ||
                bounds.y + bounds.h > source.height()) {
                return kInvalidSnapshotPayloadSlot;
            }
            if (source.bytes_per_pixel() != ui::draw_cmd::bytes_per_pixel(screen_pixel_format)) {
                return kInvalidSnapshotPayloadSlot;
            }
            for (std::size_t i = 0; i < occupied_.size(); ++i) {
                if (occupied_[i]) continue;
                if (!copy_from_canvas(buffers_[i], source, bounds)) {
                    return kInvalidSnapshotPayloadSlot;
                }
                occupied_[i] = true;
                widths_[i] = bounds.w;
                heights_[i] = bounds.h;
                return static_cast<std::uint32_t>(i);
            }
            return kInvalidSnapshotPayloadSlot;
        }

        [[nodiscard]] bool release(std::uint32_t slot) noexcept {
            if (slot >= occupied_.size() || !occupied_[slot]) return false;
            occupied_[slot] = false;
            widths_[slot] = 0;
            heights_[slot] = 0;
            return true;
        }

        [[nodiscard]] const std::byte* row(std::uint32_t slot, int y) const noexcept {
            if (slot >= occupied_.size() || !occupied_[slot]) return nullptr;
            if (y < 0 || y >= heights_[slot]) return nullptr;
            return buffers_[slot].data() + static_cast<std::size_t>(y) * stride_bytes;
        }

        [[nodiscard]] int width(std::uint32_t slot) const noexcept {
            return (slot < widths_.size() && occupied_[slot]) ? widths_[slot] : 0;
        }

        [[nodiscard]] int height(std::uint32_t slot) const noexcept {
            return (slot < heights_.size() && occupied_[slot]) ? heights_[slot] : 0;
        }

        static constexpr std::size_t stride_bytes =
            static_cast<std::size_t>(layer_cache_width)
            * ui::draw_cmd::bytes_per_pixel(screen_pixel_format);

    private:
        static constexpr std::size_t pixel_bytes =
            static_cast<std::size_t>(layer_cache_width)
            * static_cast<std::size_t>(layer_cache_height)
            * ui::draw_cmd::bytes_per_pixel(screen_pixel_format);

        static bool copy_from_canvas(std::array<std::byte, pixel_bytes>& target,
                                     CanvasBase& source,
                                     Rect bounds) noexcept {
            const auto row_bytes = static_cast<std::size_t>(bounds.w) * source.bytes_per_pixel();
            for (int y = 0; y < bounds.h; ++y) {
                const auto* src = source.row_ptr(bounds.y + y);
                if (!src) return false;
                const auto src_offset = static_cast<std::size_t>(bounds.x) * source.bytes_per_pixel();
                auto* dst = target.data() + static_cast<std::size_t>(y) * stride_bytes;
                std::memcpy(dst, src + src_offset, row_bytes);
            }
            return true;
        }

        std::array<std::array<std::byte, pixel_bytes>, MaxSlots> buffers_{};
        std::array<bool, MaxSlots> occupied_{};
        std::array<int, MaxSlots> widths_{};
        std::array<int, MaxSlots> heights_{};
    };

    template<std::size_t MaxSlots>
    class CommandSnapshotPayloadStore {
    public:
        using Buffer = ui::draw_cmd::DefaultDrawCmdBuffer;

        [[nodiscard]] std::uint32_t store(const Buffer& source) noexcept {
            for (std::size_t i = 0; i < buffers_.size(); ++i) {
                if (occupied_[i]) continue;
                if (!copy_into(buffers_[i], source)) return kInvalidSnapshotPayloadSlot;
                occupied_[i] = true;
                return static_cast<std::uint32_t>(i);
            }
            return kInvalidSnapshotPayloadSlot;
        }

        [[nodiscard]] bool release(std::uint32_t slot) noexcept {
            if (slot >= buffers_.size() || !occupied_[slot]) return false;
            buffers_[slot].clear();
            occupied_[slot] = false;
            return true;
        }

        [[nodiscard]] const Buffer* get(std::uint32_t slot) const noexcept {
            if (slot >= buffers_.size() || !occupied_[slot]) return nullptr;
            return &buffers_[slot];
        }

        void clear() noexcept {
            for (std::size_t i = 0; i < buffers_.size(); ++i) {
                buffers_[i].clear();
                occupied_[i] = false;
            }
        }

    private:
        static bool copy_into(Buffer& target, const Buffer& source) noexcept {
            return target.load(source.cmd_data(),
                               source.cmd_bytes(),
                               source.size(),
                               source.text_data(),
                               source.text_used(),
                               source.blob_data(),
                               source.blob_used());
        }

        std::array<Buffer, MaxSlots> buffers_{};
        std::array<bool, MaxSlots> occupied_{};
    };

    struct TileStats {
        int tiles_total{0};
        int tiles_drawn{0};
        std::size_t cmd_count{0};
        std::size_t cmd_bytes{0};
        std::uint64_t alpha_blend_count{0};
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

    struct TileConfig {
        int tile_width{64};
        int tile_height{64};
        rgba clear_color{0, 0, 0, 0};
        bool clear_tile{true};
    };
}
