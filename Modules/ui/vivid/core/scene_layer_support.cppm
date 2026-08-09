module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <span>
#include <type_traits>

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
        bool workspace_overflowed{false};
        bool traversal_phase_conflicted{false};
        bool style_patch_overflowed{false};
        bool semantic_overflowed{false};
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

    class CommandSnapshotPayload {
    public:
        using SourceBuffer = ui::draw_cmd::DefaultDrawCmdBuffer;

        CommandSnapshotPayload() noexcept {}

        [[nodiscard]] bool store(const SourceBuffer& source) noexcept {
            count_ = 0;
            cmd_bytes_used_ = 0;
            text_used_ = 0;
            blob_used_ = 0;
            if (source.overflowed()
                || source.size() > SourceBuffer::kMaxCommands
                || source.cmd_bytes() > SourceBuffer::kCmdBytesCapacity
                || source.text_used() > SourceBuffer::kTextCapacity
                || source.blob_used() > SourceBuffer::kBlobCapacity) {
                return false;
            }
            std::size_t validated_count = 0;
            std::size_t offset = 0;
            while (offset < source.cmd_bytes()) {
                ui::draw_cmd::DrawCmd decoded{};
                std::size_t stride = 0;
                if (!source.read_cmd_at_offset(offset, decoded, stride)) return false;
                offset += stride;
                ++validated_count;
            }
            if (validated_count != source.size()) return false;
            if (source.cmd_bytes() > 0) {
                std::memcpy(cmd_bytes_.data(), source.cmd_data(), source.cmd_bytes());
            }
            if (source.text_used() > 0) {
                std::memcpy(text_.data(), source.text_data(), source.text_used());
            }
            if (source.blob_used() > 0) {
                std::memcpy(blob_.data(), source.blob_data(), source.blob_used());
            }
            count_ = source.size();
            cmd_bytes_used_ = source.cmd_bytes();
            text_used_ = source.text_used();
            blob_used_ = source.blob_used();
            return true;
        }

        [[nodiscard]] std::size_t size() const noexcept { return count_; }
        [[nodiscard]] std::size_t cmd_bytes() const noexcept { return cmd_bytes_used_; }
        [[nodiscard]] bool overflowed() const noexcept { return false; }

        [[nodiscard]] bool read_cmd_at_offset(std::size_t offset,
                                              ui::draw_cmd::DrawCmd& out,
                                              std::size_t& stride) const noexcept {
            if (offset + sizeof(ui::draw_cmd::CmdHeader) > cmd_bytes_used_) return false;
            const auto* header = reinterpret_cast<const ui::draw_cmd::CmdHeader*>(
                cmd_bytes_.data() + offset);
            if (header->size < sizeof(ui::draw_cmd::CmdHeader)) return false;
            stride = ui::draw_cmd::cmd_stride(header->size);
            constexpr std::size_t max_stride =
                SourceBuffer::kCmdBytesCapacity / SourceBuffer::kMaxCommands;
            if (stride == 0 || stride > max_stride || offset + stride > cmd_bytes_used_) {
                return false;
            }
            return ui::draw_cmd::decode_cmd(header, out);
        }

        [[nodiscard]] bool text_span_valid(ui::draw_cmd::TextSpan span) const noexcept {
            if (span.length == 0) return true;
            const std::size_t end = static_cast<std::size_t>(span.offset) + span.length;
            return end <= text_used_;
        }

        [[nodiscard]] const char* text_at(std::uint32_t offset) const noexcept {
            return offset < text_used_ ? text_.data() + offset : text_.data();
        }

        [[nodiscard]] std::span<const std::byte> blob_at(ui::draw_cmd::BlobRef ref) const noexcept {
            if (ref.length == 0) return {};
            const std::size_t end = static_cast<std::size_t>(ref.offset) + ref.length;
            if (end > blob_used_) return {};
            return std::span<const std::byte>(blob_.data() + ref.offset, ref.length);
        }

    private:
        alignas(ui::draw_cmd::kCmdAlign)
            std::array<std::byte, SourceBuffer::kCmdBytesCapacity> cmd_bytes_;
        std::array<char, SourceBuffer::kTextCapacity> text_;
        std::array<std::byte, SourceBuffer::kBlobCapacity> blob_;
        std::size_t count_{0};
        std::size_t cmd_bytes_used_{0};
        std::size_t text_used_{0};
        std::size_t blob_used_{0};
    };

    static_assert(std::is_trivially_destructible_v<CommandSnapshotPayload>);
    static_assert(sizeof(CommandSnapshotPayload)
                  <= sizeof(ui::draw_cmd::DefaultDrawCmdBuffer));

    template<std::size_t MaxSlots, bool CommandEnabled, bool PixelEnabled>
    class SnapshotPayloadStore {
    public:
        using CommandPayload = CommandSnapshotPayload;
        static constexpr bool command_enabled = CommandEnabled;
        static constexpr bool pixel_enabled = PixelEnabled;
        static constexpr std::size_t stride_bytes =
            static_cast<std::size_t>(layer_cache_width)
            * ui::draw_cmd::bytes_per_pixel(screen_pixel_format);
        static constexpr std::size_t pixel_slot_bytes =
            static_cast<std::size_t>(layer_cache_width)
            * static_cast<std::size_t>(layer_cache_height)
            * ui::draw_cmd::bytes_per_pixel(screen_pixel_format);
        static constexpr std::size_t command_slot_bytes = sizeof(CommandPayload);
        static constexpr std::size_t slot_bytes = []() consteval {
            if constexpr (command_enabled && pixel_enabled) {
                return pixel_slot_bytes > command_slot_bytes
                    ? pixel_slot_bytes
                    : command_slot_bytes;
            } else if constexpr (command_enabled) {
                return command_slot_bytes;
            } else if constexpr (pixel_enabled) {
                return pixel_slot_bytes;
            } else {
                return std::size_t{0};
            }
        }();
        static constexpr std::size_t command_capacity_bytes =
            command_enabled ? command_slot_bytes * MaxSlots : 0;
        static constexpr std::size_t pixel_capacity_bytes =
            pixel_enabled ? pixel_slot_bytes * MaxSlots : 0;

        SnapshotPayloadStore() noexcept {}
        SnapshotPayloadStore(const SnapshotPayloadStore&) = delete;
        SnapshotPayloadStore& operator=(const SnapshotPayloadStore&) = delete;
        SnapshotPayloadStore(SnapshotPayloadStore&&) = delete;
        SnapshotPayloadStore& operator=(SnapshotPayloadStore&&) = delete;

        ~SnapshotPayloadStore() noexcept {
            clear();
        }

        [[nodiscard]] bool store_command(std::uint32_t slot,
                                         const ui::draw_cmd::DefaultDrawCmdBuffer& source) noexcept {
            if constexpr (!command_enabled) {
                (void)slot;
                (void)source;
                return false;
            } else {
                if (!slot_available(slot)) return false;
                auto* payload = std::construct_at(command_storage(slot));
                if (!payload->store(source)) {
                    std::destroy_at(payload);
                    return false;
                }
                kinds_[slot] = PayloadKind::Command;
                return true;
            }
        }

        [[nodiscard]] bool store_pixel(std::uint32_t slot,
                                       CanvasBase& source,
                                       Rect bounds) noexcept {
            if constexpr (!pixel_enabled) {
                (void)slot;
                (void)source;
                (void)bounds;
                return false;
            } else {
                if (!slot_available(slot)) return false;
                if (bounds.w <= 0 || bounds.h <= 0) return false;
                if (bounds.w > layer_cache_width || bounds.h > layer_cache_height) return false;
                if (bounds.x < 0 || bounds.y < 0
                    || bounds.x + bounds.w > source.width()
                    || bounds.y + bounds.h > source.height()) {
                    return false;
                }
                if (source.bytes_per_pixel()
                    != ui::draw_cmd::bytes_per_pixel(screen_pixel_format)) {
                    return false;
                }
                if (!copy_from_canvas(slots_[slot].bytes.data(), source, bounds)) return false;
                kinds_[slot] = PayloadKind::Pixel;
                widths_[slot] = bounds.w;
                heights_[slot] = bounds.h;
                return true;
            }
        }

        [[nodiscard]] bool release(std::uint32_t slot) noexcept {
            if (slot >= MaxSlots || kinds_[slot] == PayloadKind::Empty) return false;
            if constexpr (command_enabled) {
                if (kinds_[slot] == PayloadKind::Command) {
                    std::destroy_at(command_payload(slot));
                }
            }
            kinds_[slot] = PayloadKind::Empty;
            widths_[slot] = 0;
            heights_[slot] = 0;
            return true;
        }

        [[nodiscard]] const CommandPayload* command(std::uint32_t slot) const noexcept {
            if constexpr (!command_enabled) {
                (void)slot;
                return nullptr;
            } else {
                if (slot >= MaxSlots || kinds_[slot] != PayloadKind::Command) return nullptr;
                return command_payload(slot);
            }
        }

        [[nodiscard]] const std::byte* pixel_row(std::uint32_t slot, int y) const noexcept {
            if constexpr (!pixel_enabled) {
                (void)slot;
                (void)y;
                return nullptr;
            } else {
                if (slot >= MaxSlots || kinds_[slot] != PayloadKind::Pixel) return nullptr;
                if (y < 0 || y >= heights_[slot]) return nullptr;
                return slots_[slot].bytes.data() + static_cast<std::size_t>(y) * stride_bytes;
            }
        }

        [[nodiscard]] int pixel_width(std::uint32_t slot) const noexcept {
            if constexpr (!pixel_enabled) {
                (void)slot;
                return 0;
            } else {
                return (slot < MaxSlots && kinds_[slot] == PayloadKind::Pixel)
                    ? widths_[slot]
                    : 0;
            }
        }

        [[nodiscard]] int pixel_height(std::uint32_t slot) const noexcept {
            if constexpr (!pixel_enabled) {
                (void)slot;
                return 0;
            } else {
                return (slot < MaxSlots && kinds_[slot] == PayloadKind::Pixel)
                    ? heights_[slot]
                    : 0;
            }
        }

        void clear() noexcept {
            for (std::size_t i = 0; i < MaxSlots; ++i) {
                if (kinds_[i] != PayloadKind::Empty) {
                    (void)release(static_cast<std::uint32_t>(i));
                }
            }
        }

    private:
        enum class PayloadKind : std::uint8_t {
            Empty,
            Command,
            Pixel,
        };

        static constexpr std::size_t storage_alignment = command_enabled
            ? alignof(CommandPayload)
            : alignof(std::byte);

        struct alignas(storage_alignment) SlotStorage {
            std::array<std::byte, slot_bytes> bytes;
        };

        [[nodiscard]] bool slot_available(std::uint32_t slot) const noexcept {
            return slot < MaxSlots && kinds_[slot] == PayloadKind::Empty;
        }

        [[nodiscard]] CommandPayload* command_storage(std::uint32_t slot) noexcept {
            return reinterpret_cast<CommandPayload*>(slots_[slot].bytes.data());
        }

        [[nodiscard]] CommandPayload* command_payload(std::uint32_t slot) noexcept {
            return std::launder(command_storage(slot));
        }

        [[nodiscard]] const CommandPayload* command_payload(std::uint32_t slot) const noexcept {
            return std::launder(reinterpret_cast<const CommandPayload*>(slots_[slot].bytes.data()));
        }

        static bool copy_from_canvas(std::byte* target,
                                     CanvasBase& source,
                                     Rect bounds) noexcept {
            const auto row_bytes = static_cast<std::size_t>(bounds.w) * source.bytes_per_pixel();
            for (int y = 0; y < bounds.h; ++y) {
                const auto* src = source.row_ptr(bounds.y + y);
                if (!src) return false;
                const auto src_offset = static_cast<std::size_t>(bounds.x) * source.bytes_per_pixel();
                auto* dst = target + static_cast<std::size_t>(y) * stride_bytes;
                std::memcpy(dst, src + src_offset, row_bytes);
            }
            return true;
        }

        std::array<SlotStorage, MaxSlots> slots_;
        std::array<PayloadKind, MaxSlots> kinds_{};
        std::array<int, MaxSlots> widths_{};
        std::array<int, MaxSlots> heights_{};
    };

    // Materialize the config specialization in its owning module; GCC 16
    // cannot deserialize a Scene CMI that implicitly owns this specialization.
    template class SnapshotPayloadStore<
        static_cast<std::size_t>(layer_cache_slots),
        snapshot_command_enabled,
        snapshot_pixel_enabled>;

    using DefaultSnapshotPayloadStore = SnapshotPayloadStore<
        static_cast<std::size_t>(layer_cache_slots),
        snapshot_command_enabled,
        snapshot_pixel_enabled>;

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
