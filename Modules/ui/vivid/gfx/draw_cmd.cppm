module;

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>

export module charm.gfx.draw_cmd;

export import charm.core.geometry;
export import charm.gfx.canvas;
export import charm.gfx.color;
export import charm.gfx.framebuffer;
export import charm.gfx.image;
export import charm.gfx.render;
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
        DrawImageNineSlice,
        DrawTextBox,
        FocusRing,
    };

    struct TextSpan {
        std::uint32_t offset{0};
        std::uint16_t length{0};
    };

    struct BlobRef {
        std::uint32_t offset{0};
        std::uint32_t length{0};
    };

    struct ImageId {
        std::uint16_t slot{0xFFFF};
        std::uint16_t generation{0};
    };

    constexpr ImageId invalid_image_id() noexcept {
        return ImageId{0xFFFF, 0};
    }

    constexpr bool image_id_valid(ImageId id) noexcept {
        return id.slot != 0xFFFF;
    }

    constexpr bool operator==(ImageId a, ImageId b) noexcept {
        return a.slot == b.slot && a.generation == b.generation;
    }

    constexpr bool operator!=(ImageId a, ImageId b) noexcept {
        return !(a == b);
    }

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

    constexpr std::uint32_t kDrawCmdBinaryVersion = 1;

    constexpr std::uint32_t draw_cmd_binary_size() noexcept {
        return static_cast<std::uint32_t>(sizeof(DrawCmd));
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
        bool cmd_overflowed{false};
        bool text_overflowed{false};
        bool blob_overflowed{false};
    };

    struct DrawCmdExecStats {
        std::size_t cmd_count{0};
        std::size_t cmd_bytes{0};
        std::size_t clip_pushes{0};
        std::size_t clip_pops{0};
        std::size_t failed_cmds{0};
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
    };

    constexpr std::size_t kMaxImageResources = 64;

    enum class ImageRegisterReason : std::uint8_t {
        Unknown = 0,
        Init = 1,
        DumpReplay = 2,
        FrameRecord = 3,
        SelfTest = 4
    };

    inline const char* image_register_reason_name(ImageRegisterReason reason) noexcept {
        switch (reason) {
        case ImageRegisterReason::Init:
            return "init";
        case ImageRegisterReason::DumpReplay:
            return "dump_replay";
        case ImageRegisterReason::FrameRecord:
            return "frame_record";
        case ImageRegisterReason::SelfTest:
            return "selftest";
        case ImageRegisterReason::Unknown:
        default:
            return "unknown";
        }
    }

    struct ImageRegistryStats {
        std::uint16_t count{0};
        std::uint64_t bytes_total{0};
        std::uint32_t register_calls{0};
        std::uint32_t register_after_lock{0};
        std::uint32_t dedup_hits{0};
        bool overflowed{false};
    };

    struct ImageRegistry {
        std::array<ImageView, kMaxImageResources> views{};
        std::array<std::uint16_t, kMaxImageResources> generations{};
        std::array<std::uint8_t, kMaxImageResources> used{};
        std::array<std::uint64_t, kMaxImageResources> hashes{};
        std::array<std::uint32_t, kMaxImageResources> keys{};
        std::array<std::uint32_t, kMaxImageResources> bytes{};
        std::uint16_t count{0};
        std::uint64_t bytes_total{0};
        std::uint32_t register_calls{0};
        std::uint32_t register_after_lock{0};
        std::uint32_t dedup_hits{0};
        bool overflowed{false};
        std::uint16_t lock_count{0};
#if defined(VIVID_SOA_TRACE_INPUT)
        bool first_after_lock_set{false};
        ImageRegisterReason first_after_lock_reason{ImageRegisterReason::Unknown};
        const char* first_after_lock_tag{nullptr};
#endif

        static std::uint64_t hash_bytes(std::uint64_t hash, const void* data, std::size_t len) noexcept {
            const auto* bytes = static_cast<const std::uint8_t*>(data);
            for (std::size_t i = 0; i < len; ++i) {
                hash ^= bytes[i];
                hash *= 1099511628211ull;
            }
            return hash;
        }

        static std::uint64_t hash_image(const ImageView& view, std::size_t data_bytes) noexcept {
            std::uint64_t hash = 14695981039346656037ull;
            const auto fmt = static_cast<std::uint32_t>(view.format);
            const std::uint32_t w = static_cast<std::uint32_t>(view.w);
            const std::uint32_t h = static_cast<std::uint32_t>(view.h);
            const std::uint32_t stride = static_cast<std::uint32_t>(view.stride_bytes);
            const std::uint8_t premul = view.premultiplied_alpha ? 1u : 0u;
            const std::uint8_t opaque = view.force_opaque ? 1u : 0u;
            hash = hash_bytes(hash, &fmt, sizeof(fmt));
            hash = hash_bytes(hash, &w, sizeof(w));
            hash = hash_bytes(hash, &h, sizeof(h));
            hash = hash_bytes(hash, &stride, sizeof(stride));
            hash = hash_bytes(hash, &premul, sizeof(premul));
            hash = hash_bytes(hash, &opaque, sizeof(opaque));
            if (view.data && data_bytes > 0) {
                hash = hash_bytes(hash, view.data, data_bytes);
            }
            return hash;
        }

        static std::size_t image_bytes(const ImageView& view) noexcept {
            if (!view) return 0;
            if (view.stride_bytes <= 0 || view.h <= 0) return 0;
            return static_cast<std::size_t>(view.stride_bytes) * static_cast<std::size_t>(view.h);
        }

        static bool image_view_equals(const ImageView& a, const ImageView& b, std::size_t data_bytes) noexcept {
            if (a.format != b.format) return false;
            if (a.premultiplied_alpha != b.premultiplied_alpha) return false;
            if (a.force_opaque != b.force_opaque) return false;
            if (a.w != b.w || a.h != b.h || a.stride_bytes != b.stride_bytes) return false;
            if (!a.data || !b.data) return false;
            if (data_bytes == 0) return false;
            return std::memcmp(a.data, b.data, data_bytes) == 0;
        }

        bool locked() const noexcept {
            return lock_count != 0;
        }

        void set_locked(bool on) noexcept {
            if (on) {
                if (lock_count < 0xFFFF) {
                    lock_count = static_cast<std::uint16_t>(lock_count + 1u);
                }
            } else if (lock_count > 0) {
                lock_count = static_cast<std::uint16_t>(lock_count - 1u);
            }
        }

        void note_after_lock(ImageRegisterReason reason, const char* tag) noexcept {
#if defined(VIVID_SOA_TRACE_INPUT)
            if (!first_after_lock_set) {
                first_after_lock_set = true;
                first_after_lock_reason = reason;
                first_after_lock_tag = tag;
            }
#else
            (void)reason;
            (void)tag;
#endif
        }

        bool allow_register(ImageRegisterReason reason, const char* tag) noexcept {
            if (!locked()) return true;
            register_after_lock++;
            note_after_lock(reason, tag);
#ifndef NDEBUG
            assert(false && "ImageRegistry is locked");
#endif
            overflowed = true;
            return false;
        }

        ImageId register_image(const ImageView& view,
                               ImageRegisterReason reason = ImageRegisterReason::Unknown,
                               const char* tag = nullptr) noexcept {
            register_calls++;
            if (!allow_register(reason, tag)) return invalid_image_id();
            if (!view) return invalid_image_id();
            const std::size_t data_bytes = image_bytes(view);
            const std::uint64_t hash = hash_image(view, data_bytes);
            for (std::size_t i = 0; i < views.size(); ++i) {
                if (used[i] == 0) {
                    used[i] = 1;
                    views[i] = view;
                    hashes[i] = hash;
                    keys[i] = 0;
                    bytes[i] = static_cast<std::uint32_t>(data_bytes);
                    if (generations[i] == 0) generations[i] = 1;
                    count++;
                    bytes_total += data_bytes;
                    return ImageId{static_cast<std::uint16_t>(i), generations[i]};
                }
            }
            overflowed = true;
            return invalid_image_id();
        }

        ImageId register_image_key(std::uint32_t key,
                                   const ImageView& view,
                                   ImageRegisterReason reason = ImageRegisterReason::Unknown,
                                   const char* tag = nullptr) noexcept {
            register_calls++;
            if (!allow_register(reason, tag)) return invalid_image_id();
            if (!view) return invalid_image_id();
            if (key != 0) {
                for (std::size_t i = 0; i < views.size(); ++i) {
                    if (used[i] == 0) continue;
                    if (keys[i] == key) {
                        dedup_hits++;
                        return ImageId{static_cast<std::uint16_t>(i), generations[i]};
                    }
                }
            }
            const std::size_t data_bytes = image_bytes(view);
            const std::uint64_t hash = hash_image(view, data_bytes);
            for (std::size_t i = 0; i < views.size(); ++i) {
                if (used[i] == 0) {
                    used[i] = 1;
                    views[i] = view;
                    hashes[i] = hash;
                    keys[i] = key;
                    bytes[i] = static_cast<std::uint32_t>(data_bytes);
                    if (generations[i] == 0) generations[i] = 1;
                    count++;
                    bytes_total += data_bytes;
                    return ImageId{static_cast<std::uint16_t>(i), generations[i]};
                }
            }
            overflowed = true;
            return invalid_image_id();
        }

        ImageId register_image_dedup(const ImageView& view,
                                     ImageRegisterReason reason = ImageRegisterReason::Unknown,
                                     const char* tag = nullptr) noexcept {
            register_calls++;
            if (!allow_register(reason, tag)) return invalid_image_id();
            if (!view) return invalid_image_id();
            const std::size_t data_bytes = image_bytes(view);
            const std::uint64_t hash = hash_image(view, data_bytes);
            for (std::size_t i = 0; i < views.size(); ++i) {
                if (used[i] == 0) continue;
                if (hashes[i] != hash) continue;
                if (image_view_equals(views[i], view, data_bytes)) {
                    dedup_hits++;
                    return ImageId{static_cast<std::uint16_t>(i), generations[i]};
                }
            }
            for (std::size_t i = 0; i < views.size(); ++i) {
                if (used[i] == 0) {
                    used[i] = 1;
                    views[i] = view;
                    hashes[i] = hash;
                    keys[i] = 0;
                    bytes[i] = static_cast<std::uint32_t>(data_bytes);
                    if (generations[i] == 0) generations[i] = 1;
                    count++;
                    bytes_total += data_bytes;
                    return ImageId{static_cast<std::uint16_t>(i), generations[i]};
                }
            }
            overflowed = true;
            return invalid_image_id();
        }

        void unregister_image(ImageId id) noexcept {
            if (!image_id_valid(id)) return;
            if (id.slot >= views.size()) return;
            if (used[id.slot] == 0) return;
            if (generations[id.slot] != id.generation) return;
            used[id.slot] = 0;
            views[id.slot] = ImageView{};
            hashes[id.slot] = 0;
            keys[id.slot] = 0;
            if (bytes[id.slot] != 0) {
                bytes_total -= bytes[id.slot];
                bytes[id.slot] = 0;
            }
            if (count > 0) {
                count--;
            }
            generations[id.slot] = static_cast<std::uint16_t>(generations[id.slot] + 1u);
        }

        const ImageView* get(ImageId id) const noexcept {
            if (!image_id_valid(id)) return nullptr;
            if (id.slot >= views.size()) return nullptr;
            if (used[id.slot] == 0) return nullptr;
            if (generations[id.slot] != id.generation) return nullptr;
            return &views[id.slot];
        }
    };

    struct ImageRegistryEntry {
        ImageId id{};
        ImageView view{};
    };

    inline ImageRegistry& image_registry() noexcept {
        static ImageRegistry registry{};
        return registry;
    }

    inline ImageId register_image(const ImageView& view) noexcept {
        return image_registry().register_image(view);
    }

    inline ImageId register_image_key(std::uint32_t key, const ImageView& view) noexcept {
        return image_registry().register_image_key(key, view);
    }

    inline ImageId register_image_dedup(const ImageView& view) noexcept {
        return image_registry().register_image_dedup(view);
    }

    inline ImageId register_image(const ImageView& view,
                                  ImageRegisterReason reason,
                                  const char* tag) noexcept {
        return image_registry().register_image(view, reason, tag);
    }

    inline ImageId register_image_key(std::uint32_t key,
                                      const ImageView& view,
                                      ImageRegisterReason reason,
                                      const char* tag) noexcept {
        return image_registry().register_image_key(key, view, reason, tag);
    }

    inline ImageId register_image_dedup(const ImageView& view,
                                        ImageRegisterReason reason,
                                        const char* tag) noexcept {
        return image_registry().register_image_dedup(view, reason, tag);
    }

    inline void unregister_image(ImageId id) noexcept {
        image_registry().unregister_image(id);
    }

    inline const ImageView* resolve_image(ImageId id) noexcept {
        return image_registry().get(id);
    }

    inline void clear_image_registry() noexcept {
        auto& registry = image_registry();
        registry.views.fill(ImageView{});
        registry.generations.fill(0);
        registry.used.fill(0);
        registry.hashes.fill(0);
        registry.keys.fill(0);
        registry.bytes.fill(0);
        registry.count = 0;
        registry.bytes_total = 0;
        registry.register_calls = 0;
        registry.register_after_lock = 0;
        registry.dedup_hits = 0;
        registry.overflowed = false;
        registry.lock_count = 0;
#if defined(VIVID_SOA_TRACE_INPUT)
        registry.first_after_lock_set = false;
        registry.first_after_lock_reason = ImageRegisterReason::Unknown;
        registry.first_after_lock_tag = nullptr;
#endif
    }

    inline bool register_image_with_id(ImageId id,
                                       const ImageView& view,
                                       ImageRegisterReason reason = ImageRegisterReason::Unknown,
                                       const char* tag = nullptr) noexcept {
        if (!image_id_valid(id) || !view) return false;
        auto& registry = image_registry();
        registry.register_calls++;
        if (!registry.allow_register(reason, tag)) return false;
        if (id.slot >= registry.views.size()) {
            registry.overflowed = true;
            return false;
        }
        const bool was_used = registry.used[id.slot] != 0;
        const std::size_t data_bytes = ImageRegistry::image_bytes(view);
        const std::uint64_t hash = ImageRegistry::hash_image(view, data_bytes);
        if (!was_used) {
            registry.count++;
        } else {
            if (registry.bytes[id.slot] != 0) {
                registry.bytes_total -= registry.bytes[id.slot];
            }
        }
        registry.views[id.slot] = view;
        registry.used[id.slot] = 1;
        registry.generations[id.slot] = (id.generation == 0) ? 1 : id.generation;
        registry.hashes[id.slot] = hash;
        registry.keys[id.slot] = 0;
        registry.bytes[id.slot] = static_cast<std::uint32_t>(data_bytes);
        registry.bytes_total += data_bytes;
        return true;
    }

    inline ImageRegistryStats image_registry_stats() noexcept {
        const auto& registry = image_registry();
        return ImageRegistryStats{
            registry.count,
            registry.bytes_total,
            registry.register_calls,
            registry.register_after_lock,
            registry.dedup_hits,
            registry.overflowed
        };
    }

    inline void set_image_registry_locked(bool on) noexcept {
        image_registry().set_locked(on);
    }

    inline bool image_registry_locked() noexcept {
        return image_registry().locked();
    }

    inline std::size_t image_registry_capacity() noexcept {
        return image_registry().views.size();
    }

    inline bool image_registry_entry(std::size_t index, ImageRegistryEntry& out) noexcept {
        auto& registry = image_registry();
        if (index >= registry.views.size()) return false;
        if (registry.used[index] == 0) return false;
        out.id = ImageId{static_cast<std::uint16_t>(index), registry.generations[index]};
        out.view = registry.views[index];
        return true;
    }

    inline ImageRegisterReason image_registry_first_after_lock_reason() noexcept {
#if defined(VIVID_SOA_TRACE_INPUT)
        return image_registry().first_after_lock_reason;
#else
        return ImageRegisterReason::Unknown;
#endif
    }

    inline const char* image_registry_first_after_lock_tag() noexcept {
#if defined(VIVID_SOA_TRACE_INPUT)
        return image_registry().first_after_lock_tag;
#else
        return nullptr;
#endif
    }

    constexpr std::size_t align_up(std::size_t value, std::size_t alignment) noexcept {
        if (alignment == 0) return value;
        const std::size_t mask = alignment - 1;
        return (value + mask) & ~mask;
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

        void clear() noexcept {
            count_ = 0;
            cmd_overflowed_ = false;
            text_overflowed_ = false;
            text_used_ = 1;
            text_[0] = '\0';
            blob_.reset();
        }

        [[nodiscard]] std::size_t size() const noexcept { return count_; }
        [[nodiscard]] const DrawCmd* data() const noexcept { return cmds_.data(); }
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

        bool load(const DrawCmd* cmds,
                  std::size_t cmd_count,
                  const char* text,
                  std::size_t text_bytes,
                  const std::byte* blob,
                  std::size_t blob_bytes) noexcept {
            clear();
            if (!cmds && cmd_count != 0) {
                cmd_overflowed_ = true;
                return false;
            }
            if (cmd_count > kMaxCommands) {
                cmd_overflowed_ = true;
                cmd_count = kMaxCommands;
            }
            if (cmd_count > 0) {
                std::memcpy(cmds_.data(), cmds, cmd_count * sizeof(DrawCmd));
            }
            count_ = cmd_count;
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
                count_ * sizeof(DrawCmd),
                text_used_,
                kTextCapacity,
                blob_.used(),
                kBlobCapacity,
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
            if (!points || count < 2) return false;
            Rect bounds{points[0].x, points[0].y, 0, 0};
            int min_x = points[0].x;
            int max_x = points[0].x;
            int min_y = points[0].y;
            int max_y = points[0].y;
            for (int i = 1; i < count; ++i) {
                const int px = points[i].x;
                const int py = points[i].y;
                if (px < min_x) min_x = px;
                if (px > max_x) max_x = px;
                if (py < min_y) min_y = py;
                if (py > max_y) max_y = py;
            }
            bounds.x = min_x;
            bounds.y = min_y;
            bounds.w = max_x - min_x + 1;
            bounds.h = max_y - min_y + 1;
            auto cmd = make_cmd(CmdType::DrawPath, bounds);
            cmd.color = color;
            const BlobRef blob = blob_.add_bytes(points,
                                                 static_cast<std::size_t>(count) * sizeof(Point),
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
            for (std::size_t i = 0; i < count_; ++i) {
                const auto& cmd = cmds_[i];
                if (cmd.type == CmdType::PushClip || cmd.type == CmdType::PopClip) {
                    continue;
                }
                if (cmd.type == CmdType::DrawLine) {
                    const int x0 = cmd.rect.x;
                    const int y0 = cmd.rect.y;
                    const int x1 = cmd.rect.w;
                    const int y1 = cmd.rect.h;
                    Rect bounds{
                        (x0 < x1) ? x0 : x1,
                        (y0 < y1) ? y0 : y1,
                        (x0 < x1) ? (x1 - x0 + 1) : (x0 - x1 + 1),
                        (y0 < y1) ? (y1 - y0 + 1) : (y0 - y1 + 1)
                    };
                    if (rect_intersect(bounds, rect, out)) return true;
                    continue;
                }
                if (!rect_valid(cmd.rect)) continue;
                if (rect_intersect(cmd.rect, rect, out)) return true;
            }
            return false;
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
            if (count_ >= kMaxCommands) {
                cmd_overflowed_ = true;
                return false;
            }
            cmds_[count_++] = cmd;
            return true;
        }

        std::array<DrawCmd, kMaxCommands> cmds_{};
        std::array<char, kTextCapacity> text_{};
        BlobArena<kBlobCapacity> blob_{};
        std::size_t count_{0};
        std::size_t text_used_{1};
        bool cmd_overflowed_{false};
        bool text_overflowed_{false};
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
            stats.cmd_bytes = buf.size() * sizeof(DrawCmd);
            stats.overflowed = buf.overflowed();

            std::array<CanvasBase::ClipState, 64> clip_stack{};
            std::size_t sp = 0;
            const auto base_clip = canvas.save_clip();
            if (initial_clip) {
                canvas.set_clip(*initial_clip);
            }

            const DrawCmd* cmds = buf.data();
            const std::size_t count = buf.size();
            for (std::size_t i = 0; i < count; ++i) {
                const auto& cmd = cmds[i];
                switch (cmd.type) {
                case CmdType::PushClip:
                    if (sp < clip_stack.size()) {
                        clip_stack[sp++] = canvas.save_clip();
                        canvas.set_clip(cmd.rect);
                        stats.clip_pushes++;
                    }
                    break;
                case CmdType::PopClip:
                    if (sp > 0) {
                        canvas.restore_clip(clip_stack[--sp]);
                        stats.clip_pops++;
                    }
                    break;
                case CmdType::DrawLine:
                    ui::render::draw_line(canvas,
                                          cmd.rect.x,
                                          cmd.rect.y,
                                          cmd.rect.w,
                                          cmd.rect.h,
                                          cmd.color);
                    break;
                case CmdType::DrawPath: {
                    const int count = cmd.p0;
                    if (count < 2) {
                        stats.failed_cmds++;
                        break;
                    }
                    const auto blob = buf.blob_at(cmd.blob);
                    const std::size_t need = static_cast<std::size_t>(count) * sizeof(Point);
                    if (blob.size() < need) {
                        stats.failed_cmds++;
                        break;
                    }
                    const auto* points = reinterpret_cast<const Point*>(blob.data());
                    const bool closed = (cmd.p1 != 0);
                    for (int i = 1; i < count; ++i) {
                        ui::render::draw_line(canvas,
                                              points[i - 1].x,
                                              points[i - 1].y,
                                              points[i].x,
                                              points[i].y,
                                              cmd.color);
                    }
                    if (closed) {
                        ui::render::draw_line(canvas,
                                              points[count - 1].x,
                                              points[count - 1].y,
                                              points[0].x,
                                              points[0].y,
                                              cmd.color);
                    }
                    break;
                }
                case CmdType::FillRect:
                    ui::render::draw_rect(canvas, cmd.rect.x, cmd.rect.y, cmd.rect.w, cmd.rect.h, cmd.color, true);
                    break;
                case CmdType::StrokeRect:
                    ui::render::draw_rect(canvas, cmd.rect.x, cmd.rect.y, cmd.rect.w, cmd.rect.h, cmd.color, false);
                    break;
                case CmdType::FillRoundRect:
                    ui::render::draw_round_rect(canvas, cmd.rect.x, cmd.rect.y, cmd.rect.w, cmd.rect.h, cmd.p0, cmd.color, true);
                    break;
                case CmdType::StrokeRoundRect:
                    ui::render::draw_round_rect(canvas, cmd.rect.x, cmd.rect.y, cmd.rect.w, cmd.rect.h, cmd.p0, cmd.color, false);
                    break;
                case CmdType::FillCircle: {
                    const int radius = cmd.p0;
                    const int cx = cmd.rect.x + cmd.rect.w / 2;
                    const int cy = cmd.rect.y + cmd.rect.h / 2;
                    ui::render::draw_circle(canvas, cx, cy, radius, cmd.color, true);
                    break;
                }
                case CmdType::StrokeCircle: {
                    const int radius = cmd.p0;
                    const int cx = cmd.rect.x + cmd.rect.w / 2;
                    const int cy = cmd.rect.y + cmd.rect.h / 2;
                    ui::render::draw_circle(canvas, cx, cy, radius, cmd.color, false);
                    break;
                }
                case CmdType::DrawImage: {
                    const auto* image = resolve_image(cmd.image);
                    if (!image || !(*image)) {
                        stats.failed_cmds++;
                        break;
                    }
                    if (cmd.rect.w > 0 && cmd.rect.h > 0) {
                        ui::render::draw_image_scaled(canvas, cmd.rect.x, cmd.rect.y,
                                                      cmd.rect.w, cmd.rect.h, *image);
                    } else {
                        ui::render::draw_image(canvas, cmd.rect.x, cmd.rect.y, *image);
                    }
                    break;
                }
                case CmdType::DrawImageNineSlice: {
                    const auto* image = resolve_image(cmd.image);
                    if (!image || !(*image)) {
                        stats.failed_cmds++;
                        break;
                    }
                    ui::render::draw_image_nine_slice(canvas,
                                                      cmd.rect.x, cmd.rect.y,
                                                      cmd.rect.w, cmd.rect.h,
                                                      *image,
                                                      cmd.p0, cmd.p1, cmd.p2, cmd.p3);
                    break;
                }
                case CmdType::DrawTextBox: {
                    if (!buf.text_span_valid(cmd.text)) {
                        stats.failed_cmds++;
                        break;
                    }
                    const char* text = buf.text_at(cmd.text.offset);
                    const Font& font = get_font(cmd.font);
                    draw_text_box(canvas, cmd.rect, text, cmd.color, font,
                                  cmd.align_h, cmd.align_v, cmd.wrap, cmd.ellipsis);
                    break;
                }
                case CmdType::FocusRing:
                    ui::render::draw_focus_ring(canvas, cmd.rect, cmd.color, cmd.p0, true, cmd.p1, cmd.p2);
                    break;
                }
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
            stats.cmd_bytes = buf.size() * sizeof(DrawCmd);
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
                const DrawCmd* cmds = buf.data();
                const std::size_t count = buf.size();
                for (std::size_t i = 0; i < count; ++i) {
                    const auto& cmd = cmds[i];
                    if (cmd.type == CmdType::PushClip || cmd.type == CmdType::PopClip) {
                        continue;
                    }
                    Rect bounds = cmd.rect;
                    if (cmd.type == CmdType::DrawLine) {
                        const int x0 = cmd.rect.x;
                        const int y0 = cmd.rect.y;
                        const int x1 = cmd.rect.w;
                        const int y1 = cmd.rect.h;
                        bounds = Rect{
                            (x0 < x1) ? x0 : x1,
                            (y0 < y1) ? y0 : y1,
                            (x0 < x1) ? (x1 - x0 + 1) : (x0 - x1 + 1),
                            (y0 < y1) ? (y1 - y0 + 1) : (y0 - y1 + 1)
                        };
                    }
                    if (!rect_valid(bounds)) continue;
                    if (!rect_intersect(bounds, screen_rect, clipped)) continue;

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
                    (void)execute(tile_canvas, buf, &tile_rect);
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
