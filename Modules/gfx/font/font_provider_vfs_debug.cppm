module;
#include <string_view>

export module charm.font.provider_vfs_debug;

export import charm.font;
export import charm.font.provider_vfs;
export import charm.font.font_noto_ascii_12;

export namespace charm::font {
    struct DebugFontLoader {
        const Font* default_font{&font_noto_ascii_12};
        const Font* fallback_font{&font_noto_ascii_12};

        [[nodiscard]] VfsFontLoaderApi api() const noexcept {
            return VfsFontLoaderApi{&load_trampoline, &reset_trampoline};
        }

    private:
        static bool load_trampoline(void* ctx, std::string_view path, Font& out) noexcept {
            (void)path;
            auto* self = static_cast<DebugFontLoader*>(ctx);
            if (!self || !self->default_font) {
                out = Font{};
                return false;
            }
            out = *self->default_font;
            return true;
        }

        static void reset_trampoline(void* ctx, Font& out) noexcept {
            (void)ctx;
            out = Font{};
        }
    };
}
