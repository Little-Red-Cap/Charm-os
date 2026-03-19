module;
#include <array>
#include <cstdint>
#include <string_view>

export module charm.font.provider_vfs;

export import charm.font;
export import charm.font.typography;

export namespace charm::font {
    struct VfsFontProviderConfig {
        const char* small_path{nullptr};
        const char* normal_path{nullptr};
        const char* large_path{nullptr};
        const char* mono_path{nullptr};
        const char* fallback_path{nullptr};
    };

    struct VfsFontLoaderApi {
        bool (*load)(void* ctx, std::string_view path, Font& out) noexcept;
        void (*reset)(void* ctx, Font& out) noexcept;
    };

    class VfsFontProvider {
    public:
        VfsFontProvider() = default;

        void set_config(const VfsFontProviderConfig& config) noexcept {
            config_ = config;
        }

        void set_loader(const VfsFontLoaderApi& api, void* ctx) noexcept {
            loader_ = api;
            loader_ctx_ = ctx;
        }

        void reset_cache() noexcept {
            for (auto& slot : slots_) {
                slot.loaded = false;
                slot.failed = false;
                if (loader_.reset) {
                    loader_.reset(loader_ctx_, slot.font);
                } else {
                    slot.font = Font{};
                }
            }
        }

        void reset_font(FontId id) noexcept {
            const std::size_t idx = slot_index(id);
            auto& slot = slots_[idx];
            slot.loaded = false;
            slot.failed = false;
            if (loader_.reset) {
                loader_.reset(loader_ctx_, slot.font);
            } else {
                slot.font = Font{};
            }
        }

        void reset_fallback() noexcept {
            auto& slot = slots_[kFallbackSlot];
            slot.loaded = false;
            slot.failed = false;
            if (loader_.reset) {
                loader_.reset(loader_ctx_, slot.font);
            } else {
                slot.font = Font{};
            }
        }

        FontProvider provider() noexcept {
            return FontProvider{this, &kApi};
        }

    private:
        struct Slot {
            Font font{};
            bool loaded{false};
            bool failed{false};
        };

        static constexpr std::size_t kSlotCount = 4;
        static constexpr std::size_t kFallbackSlot = kSlotCount;

        static const FontProviderApi kApi;

        static const Font* get_font_trampoline(void* ctx, FontId id) noexcept {
            return static_cast<VfsFontProvider*>(ctx)->get_font_impl(id);
        }

        static const Font* get_fallback_trampoline(void* ctx) noexcept {
            return static_cast<VfsFontProvider*>(ctx)->get_fallback_impl();
        }

        const Font* get_font_impl(FontId id) noexcept {
            const std::size_t idx = slot_index(id);
            auto& slot = slots_[idx];
            if (slot.loaded) return &slot.font;
            if (slot.failed) return nullptr;
            const char* path = font_path(id);
            if (!path || !loader_.load) {
                slot.failed = true;
                return nullptr;
            }
            if (!loader_.load(loader_ctx_, path, slot.font)) {
                slot.failed = true;
                return nullptr;
            }
            slot.loaded = true;
            return &slot.font;
        }

        const Font* get_fallback_impl() noexcept {
            auto& slot = slots_[kFallbackSlot];
            if (slot.loaded) return &slot.font;
            if (slot.failed) return nullptr;
            const char* path = config_.fallback_path;
            if (!path || !loader_.load) {
                slot.failed = true;
                return nullptr;
            }
            if (!loader_.load(loader_ctx_, path, slot.font)) {
                slot.failed = true;
                return nullptr;
            }
            slot.loaded = true;
            return &slot.font;
        }

        static constexpr std::size_t slot_index(FontId id) noexcept {
            switch (id) {
            case FontId::Small: return 0;
            case FontId::Normal: return 1;
            case FontId::Large: return 2;
            case FontId::Mono: return 3;
            }
            return 1;
        }

        const char* font_path(FontId id) const noexcept {
            switch (id) {
            case FontId::Small: return config_.small_path;
            case FontId::Normal: return config_.normal_path;
            case FontId::Large: return config_.large_path;
            case FontId::Mono: return config_.mono_path;
            }
            return config_.normal_path;
        }

        VfsFontProviderConfig config_{};
        VfsFontLoaderApi loader_{};
        void* loader_ctx_{nullptr};
        std::array<Slot, kSlotCount + 1> slots_{};
    };

    inline const FontProviderApi VfsFontProvider::kApi{
        &VfsFontProvider::get_font_trampoline,
        &VfsFontProvider::get_fallback_trampoline
    };
}
