module;
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
export module charm.gfx.image;

export import charm.gfx.pixel_format;

export
struct ImageView {
    PixelFormat format{PixelFormat::RGB888};
    bool premultiplied_alpha{false};
    bool force_opaque{false};
    int w{0};
    int h{0};
    int stride_bytes{0};
    const std::byte* data{nullptr};

    constexpr explicit operator bool() const noexcept { return data != nullptr && w > 0 && h > 0; }
};

export
constexpr ImageView make_image_view(PixelFormat fmt,
                                    int w,
                                    int h,
                                    int stride_bytes,
                                    const std::byte* data,
                                    bool premultiplied_alpha = false,
                                    bool force_opaque = false) noexcept
{
    ImageView img{};
    img.format = fmt;
    img.premultiplied_alpha = premultiplied_alpha;
    img.force_opaque = force_opaque;
    img.w = w;
    img.h = h;
    img.stride_bytes = stride_bytes;
    img.data = data;
    return img;
}

export namespace ui::gfx {
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

    constexpr std::size_t kMaxImageResources = 64;

    enum class ImageRegisterReason : std::uint8_t {
        Unknown = 0,
        Init = 1,
        DumpReplay = 2,
        FrameRecord = 3,
        FrameCompact = 4,
        FrameExecute = 5,
        SelfTest = 6
    };

    inline const char* image_register_reason_name(ImageRegisterReason reason) noexcept {
        switch (reason) {
        case ImageRegisterReason::Init:
            return "init";
        case ImageRegisterReason::DumpReplay:
            return "dump_replay";
        case ImageRegisterReason::FrameRecord:
            return "frame_record";
        case ImageRegisterReason::FrameCompact:
            return "frame_compact";
        case ImageRegisterReason::FrameExecute:
            return "frame_execute";
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
        std::uint32_t register_new_total{0};
        std::uint32_t register_new_after_lock{0};
        std::uint32_t register_new_record{0};
        std::uint32_t register_new_compact{0};
        std::uint32_t register_new_execute{0};
        std::uint32_t dedup_hits{0};
        bool overflowed{false};
    };

    struct ImageAsset {
        ImageView view{};
        ImageId id{};
        const char* tag{nullptr};
    };

    struct ImageBundleResult {
        std::uint16_t registered{0};
        std::uint16_t failed{0};
        bool locked{false};
    };

    enum class ImageRegisterStatus : std::uint8_t {
        Ok = 0,
        InvalidView,
        InvalidId,
        Locked,
        Overflow,
    };

    struct ImageRegisterResult {
        ImageId id{};
        ImageRegisterStatus status{ImageRegisterStatus::Ok};
        constexpr bool ok() const noexcept { return status == ImageRegisterStatus::Ok; }
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
        std::uint32_t register_new_total{0};
        std::uint32_t register_new_after_lock{0};
        std::uint32_t register_new_record{0};
        std::uint32_t register_new_compact{0};
        std::uint32_t register_new_execute{0};
        std::uint32_t dedup_hits{0};
        bool overflowed{false};
        std::uint16_t lock_count{0};
        ImageRegisterReason current_phase{ImageRegisterReason::Unknown};
        bool first_after_lock_set{false};
        ImageRegisterReason first_after_lock_reason{ImageRegisterReason::Unknown};
        const char* first_after_lock_tag{nullptr};

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
            if (!first_after_lock_set) {
                first_after_lock_set = true;
                first_after_lock_reason = reason;
                first_after_lock_tag = tag;
            }
        }

        bool allow_register_new(ImageRegisterReason reason, const char* tag) noexcept {
            if (!locked()) return true;
            register_new_after_lock++;
            switch (current_phase) {
            case ImageRegisterReason::FrameRecord:
                register_new_record++;
                break;
            case ImageRegisterReason::FrameCompact:
                register_new_compact++;
                break;
            case ImageRegisterReason::FrameExecute:
                register_new_execute++;
                break;
            default:
                break;
            }
            note_after_lock(reason, tag);
#ifndef NDEBUG
            assert(false && "ImageRegistry is locked");
#endif
            overflowed = true;
            return false;
        }

        ImageRegisterResult register_image(const ImageView& view,
                                           ImageRegisterReason reason = ImageRegisterReason::Unknown,
                                           const char* tag = nullptr) noexcept {
            register_calls++;
            if (!view) return {invalid_image_id(), ImageRegisterStatus::InvalidView};
            if (!allow_register_new(reason, tag)) {
                return {invalid_image_id(), ImageRegisterStatus::Locked};
            }
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
                    register_new_total++;
                    return {ImageId{static_cast<std::uint16_t>(i), generations[i]},
                            ImageRegisterStatus::Ok};
                }
            }
            overflowed = true;
            return {invalid_image_id(), ImageRegisterStatus::Overflow};
        }

        ImageRegisterResult register_image_key(std::uint32_t key,
                                               const ImageView& view,
                                               ImageRegisterReason reason = ImageRegisterReason::Unknown,
                                               const char* tag = nullptr) noexcept {
            register_calls++;
            if (!view) return {invalid_image_id(), ImageRegisterStatus::InvalidView};
            if (key != 0) {
                for (std::size_t i = 0; i < views.size(); ++i) {
                    if (used[i] == 0) continue;
                    if (keys[i] == key) {
                        dedup_hits++;
                        return {ImageId{static_cast<std::uint16_t>(i), generations[i]},
                                ImageRegisterStatus::Ok};
                    }
                }
            }
            if (!allow_register_new(reason, tag)) {
                return {invalid_image_id(), ImageRegisterStatus::Locked};
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
                    register_new_total++;
                    return {ImageId{static_cast<std::uint16_t>(i), generations[i]},
                            ImageRegisterStatus::Ok};
                }
            }
            overflowed = true;
            return {invalid_image_id(), ImageRegisterStatus::Overflow};
        }

        ImageRegisterResult register_image_dedup(const ImageView& view,
                                                 ImageRegisterReason reason = ImageRegisterReason::Unknown,
                                                 const char* tag = nullptr) noexcept {
            register_calls++;
            if (!view) return {invalid_image_id(), ImageRegisterStatus::InvalidView};
            const std::size_t data_bytes = image_bytes(view);
            const std::uint64_t hash = hash_image(view, data_bytes);
            for (std::size_t i = 0; i < views.size(); ++i) {
                if (used[i] == 0) continue;
                if (hashes[i] != hash) continue;
                if (image_view_equals(views[i], view, data_bytes)) {
                    dedup_hits++;
                    return {ImageId{static_cast<std::uint16_t>(i), generations[i]},
                            ImageRegisterStatus::Ok};
                }
            }
            if (!allow_register_new(reason, tag)) {
                return {invalid_image_id(), ImageRegisterStatus::Locked};
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
                    register_new_total++;
                    return {ImageId{static_cast<std::uint16_t>(i), generations[i]},
                            ImageRegisterStatus::Ok};
                }
            }
            overflowed = true;
            return {invalid_image_id(), ImageRegisterStatus::Overflow};
        }

        void unregister_image(ImageId id) noexcept {
            if (!image_id_valid(id)) return;
            if (id.slot >= views.size()) return;
            if (locked()) {
                (void)allow_register_new(ImageRegisterReason::Unknown, "unregister");
                return;
            }
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

        struct PhaseScope {
            ImageRegistry& registry;
            ImageRegisterReason prev{ImageRegisterReason::Unknown};
            explicit PhaseScope(ImageRegistry& r, ImageRegisterReason phase) noexcept
                : registry(r), prev(r.current_phase) {
                registry.current_phase = phase;
            }
            ~PhaseScope() noexcept { registry.current_phase = prev; }
            PhaseScope(const PhaseScope&) = delete;
            PhaseScope& operator=(const PhaseScope&) = delete;
        };

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

    inline ImageRegisterResult register_image(const ImageView& view) noexcept {
        return image_registry().register_image(view);
    }

    inline ImageRegisterResult register_image_key(std::uint32_t key, const ImageView& view) noexcept {
        return image_registry().register_image_key(key, view);
    }

    inline ImageRegisterResult register_image_dedup(const ImageView& view) noexcept {
        return image_registry().register_image_dedup(view);
    }

    inline ImageRegisterResult register_image(const ImageView& view,
                                              ImageRegisterReason reason,
                                              const char* tag) noexcept {
        return image_registry().register_image(view, reason, tag);
    }

    inline ImageRegisterResult register_image_key(std::uint32_t key,
                                                  const ImageView& view,
                                                  ImageRegisterReason reason,
                                                  const char* tag) noexcept {
        return image_registry().register_image_key(key, view, reason, tag);
    }

    inline ImageRegisterResult register_image_dedup(const ImageView& view,
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
        registry.register_new_total = 0;
        registry.register_new_after_lock = 0;
        registry.dedup_hits = 0;
        registry.overflowed = false;
        registry.lock_count = 0;
#if defined(VIVID_SOA_TRACE_INPUT)
        registry.first_after_lock_set = false;
        registry.first_after_lock_reason = ImageRegisterReason::Unknown;
        registry.first_after_lock_tag = nullptr;
#endif
    }

    inline ImageRegisterResult register_image_with_id(ImageId id,
                                                      const ImageView& view,
                                                      ImageRegisterReason reason = ImageRegisterReason::Unknown,
                                                      const char* tag = nullptr) noexcept {
        if (!image_id_valid(id)) {
            return {invalid_image_id(), ImageRegisterStatus::InvalidId};
        }
        if (!view) {
            return {invalid_image_id(), ImageRegisterStatus::InvalidView};
        }
        auto& registry = image_registry();
        registry.register_calls++;
        if (!registry.allow_register_new(reason, tag)) {
            return {invalid_image_id(), ImageRegisterStatus::Locked};
        }
        if (id.slot >= registry.views.size()) {
            registry.overflowed = true;
            return {invalid_image_id(), ImageRegisterStatus::Overflow};
        }
        const bool was_used = registry.used[id.slot] != 0;
        if (registry.locked() && was_used) {
            registry.note_after_lock(reason, tag);
            registry.register_new_after_lock++;
#ifndef NDEBUG
            assert(false && "ImageRegistry overwrite while locked");
#endif
            registry.overflowed = true;
            return {invalid_image_id(), ImageRegisterStatus::Locked};
        }
        const std::size_t data_bytes = ImageRegistry::image_bytes(view);
        const std::uint64_t hash = ImageRegistry::hash_image(view, data_bytes);
        if (!was_used) {
            registry.count++;
            registry.register_new_total++;
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
        return {ImageId{static_cast<std::uint16_t>(id.slot), registry.generations[id.slot]},
                ImageRegisterStatus::Ok};
    }

    inline ImageRegistryStats image_registry_stats() noexcept {
        const auto& registry = image_registry();
        return ImageRegistryStats{
            registry.count,
            registry.bytes_total,
            registry.register_calls,
            registry.register_new_total,
            registry.register_new_after_lock,
            registry.register_new_record,
            registry.register_new_compact,
            registry.register_new_execute,
            registry.dedup_hits,
            registry.overflowed
        };
    }

    inline ImageBundleResult register_image_bundle(std::span<ImageAsset> assets,
                                                   ImageRegisterReason reason = ImageRegisterReason::Init,
                                                   bool dedup = true,
                                                   bool lock_after = true) noexcept {
        ImageBundleResult result{};
        if (assets.empty()) {
            result.locked = image_registry().locked();
            if (lock_after && !result.locked) {
                image_registry().set_locked(true);
                result.locked = true;
            }
            return result;
        }
        for (auto& asset : assets) {
            if (!asset.view) {
                asset.id = invalid_image_id();
                ++result.failed;
                continue;
            }
            const char* tag = asset.tag ? asset.tag : "bundle";
            const ImageRegisterResult reg = dedup
                ? register_image_dedup(asset.view, reason, tag)
                : register_image(asset.view, reason, tag);
            if (reg.ok()) {
                asset.id = reg.id;
                ++result.registered;
            } else {
                asset.id = invalid_image_id();
                ++result.failed;
            }
        }
        result.locked = image_registry().locked();
        if (lock_after && !result.locked) {
            image_registry().set_locked(true);
            result.locked = true;
        }
        return result;
    }

    struct ImageRegistryPhaseGuard {
        explicit ImageRegistryPhaseGuard(ImageRegisterReason phase) noexcept
            : scope_(image_registry(), phase) {}
        ImageRegistryPhaseGuard(const ImageRegistryPhaseGuard&) = delete;
        ImageRegistryPhaseGuard& operator=(const ImageRegistryPhaseGuard&) = delete;
    private:
        ImageRegistry::PhaseScope scope_;
    };

    inline void set_image_registry_locked(bool on) noexcept {
        image_registry().set_locked(on);
    }

    inline bool image_registry_locked() noexcept {
        return image_registry().locked();
    }

    struct ImageRegistryLockGuard {
        explicit ImageRegistryLockGuard(bool enabled = true) noexcept : enabled_(enabled) {
            if (enabled_) set_image_registry_locked(true);
        }
        ~ImageRegistryLockGuard() noexcept {
            if (enabled_) set_image_registry_locked(false);
        }
        ImageRegistryLockGuard(const ImageRegistryLockGuard&) = delete;
        ImageRegistryLockGuard& operator=(const ImageRegistryLockGuard&) = delete;
    private:
        bool enabled_{false};
    };

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

    inline std::uint32_t image_registry_register_after_lock() noexcept {
        return image_registry().register_new_after_lock;
    }

    inline const char* image_registry_first_after_lock_tag() noexcept {
        return image_registry().first_after_lock_tag;
    }

    inline ImageRegisterReason image_registry_first_after_lock_reason() noexcept {
        return image_registry().first_after_lock_reason;
    }
} // namespace ui::gfx
