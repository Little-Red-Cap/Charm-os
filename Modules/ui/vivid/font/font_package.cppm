module;
#include <array>
#include <cstdio>
#include <cstddef>
#include <string_view>

export module charm.ui.vivid.font_package;

export import charm.font;
export import charm.font.provider_vfs;
export import charm.font.typography;

export namespace charm::font {
    struct FontWeightPaths {
        const char* small_path{nullptr};
        const char* normal_path{nullptr};
        const char* large_path{nullptr};
        const char* mono_path{nullptr};
    };

    struct FontPackageConfig {
        FontWeightPaths regular{};
        FontWeightPaths medium{};
        FontWeightPaths bold{};
        const char* fallback_path{nullptr};
    };

    class VfsFontPackage {
    public:
        VfsFontPackage() = default;

        void set_config(const FontPackageConfig& config) noexcept {
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
            fallback_loaded_ = false;
            fallback_failed_ = false;
            if (loader_.reset) {
                loader_.reset(loader_ctx_, fallback_font_);
            } else {
                fallback_font_ = Font{};
            }
        }

        FontProvider provider() noexcept {
            return FontProvider{this, &kFontApi};
        }

        FontWeightProvider weight_provider() noexcept {
            return FontWeightProvider{this, &kWeightApi};
        }

        void bind() noexcept {
            set_font_provider(provider());
            set_font_weight_provider(weight_provider());
        }

    private:
        struct Slot {
            Font font{};
            bool loaded{false};
            bool failed{false};
        };

        static constexpr std::size_t kFontIdCount = 4;
        static constexpr std::size_t kWeightCount = kFontWeightCount;
        static constexpr std::size_t kSlotCount = kFontIdCount * kWeightCount;

        static const FontProviderApi kFontApi;
        static const FontWeightProviderApi kWeightApi;

        static const Font* get_font_trampoline(void* ctx, FontId id) noexcept {
            return static_cast<VfsFontPackage*>(ctx)->get_font_impl(id, FontWeight::Regular);
        }

        static const Font* get_fallback_trampoline(void* ctx) noexcept {
            return static_cast<VfsFontPackage*>(ctx)->get_fallback_impl();
        }

        static const Font* get_weight_trampoline(void* ctx, FontId id, FontWeight weight) noexcept {
            return static_cast<VfsFontPackage*>(ctx)->get_font_impl(id, weight);
        }

        const Font* get_font_impl(FontId id, FontWeight weight) noexcept {
            const std::size_t idx = slot_index(id, weight);
            auto& slot = slots_[idx];
            if (slot.loaded) return &slot.font;
            if (slot.failed) return nullptr;
            const char* path = font_path(id, weight);
            if (!path || !loader_.load) {
                slot.failed = true;
                return nullptr;
            }
            std::fprintf(stdout, "[font] package load id=%u weight=%u path=%s\n",
                         static_cast<unsigned>(id), static_cast<unsigned>(weight), path);
            if (!loader_.load(loader_ctx_, path, slot.font)) {
                slot.failed = true;
                return nullptr;
            }
            if (!slot.font.fallback_font) {
                if (const auto* fallback = get_fallback_impl()) {
                    if (&slot.font != fallback) {
                        slot.font.fallback_font = fallback;
                    }
                }
            }
            slot.loaded = true;
            return &slot.font;
        }

        const Font* get_fallback_impl() noexcept {
            if (fallback_loaded_) return &fallback_font_;
            if (fallback_failed_) return nullptr;
            if (!config_.fallback_path || !loader_.load) {
                fallback_failed_ = true;
                return nullptr;
            }
            if (!loader_.load(loader_ctx_, config_.fallback_path, fallback_font_)) {
                fallback_failed_ = true;
                return nullptr;
            }
            fallback_loaded_ = true;
            return &fallback_font_;
        }

        static constexpr std::size_t slot_index(FontId id, FontWeight weight) noexcept {
            return weight_index(weight) * kFontIdCount + static_cast<std::size_t>(id);
        }

        const FontWeightPaths& weight_paths(FontWeight weight) const noexcept {
            switch (weight) {
            case FontWeight::Medium: return config_.medium;
            case FontWeight::Bold: return config_.bold;
            case FontWeight::Regular:
            default: return config_.regular;
            }
        }

        const char* font_path(FontId id, FontWeight weight) const noexcept {
            const auto& paths = weight_paths(weight);
            const char* path = font_path_for(paths, id);
            if (path) return path;
            return font_path_for(config_.regular, id);
        }

        static const char* font_path_for(const FontWeightPaths& paths, FontId id) noexcept {
            switch (id) {
            case FontId::Small: return paths.small_path;
            case FontId::Normal: return paths.normal_path;
            case FontId::Large: return paths.large_path;
            case FontId::Mono: return paths.mono_path;
            }
            return paths.normal_path;
        }

        static constexpr std::size_t weight_index(FontWeight weight) noexcept {
            switch (weight) {
            case FontWeight::Medium: return 1;
            case FontWeight::Bold: return 2;
            case FontWeight::Regular:
            default: return 0;
            }
        }

        FontPackageConfig config_{};
        VfsFontLoaderApi loader_{};
        void* loader_ctx_{nullptr};
        std::array<Slot, kSlotCount> slots_{};
        Font fallback_font_{};
        bool fallback_loaded_{false};
        bool fallback_failed_{false};
    };

    inline const FontProviderApi VfsFontPackage::kFontApi{
        &VfsFontPackage::get_font_trampoline,
        &VfsFontPackage::get_fallback_trampoline
    };

    inline const FontWeightProviderApi VfsFontPackage::kWeightApi{
        &VfsFontPackage::get_weight_trampoline
    };
}
